#include <stdio.h>
#include "puzzles.h"

#define EDGE_L 0x01
#define EDGE_R 0x02
#define EDGE_U 0x04
#define EDGE_D 0x08

void fatal(const char *fmt, ...) {
    printf(fmt);
    exit(0);
}

typedef struct gridstate {
    int w, h;
    unsigned char *faces;
} gridstate;

struct neighbour_ctx {
    gridstate *state;
    int i, n, neighbours[4];
};

static int neighbour(int vertex, void *vctx) {
    struct neighbour_ctx *ctx = (struct neighbour_ctx *)vctx;
    if (vertex >= 0) {
        gridstate *state = ctx->state;
        int w = state->w, x = vertex % w, y = vertex / w;
        ctx->i = ctx->n = 0;
        if (state->faces[vertex] & EDGE_R) {
            int nx = x + 1, ny = y;
            if (nx < state->w)
                ctx->neighbours[ctx->n++] = ny * w + nx;
        }
        if (state->faces[vertex] & EDGE_L) {
            int nx = x - 1, ny = y;
            if (nx >= 0)
                ctx->neighbours[ctx->n++] = ny * w + nx;
        }
        if (state->faces[vertex] & EDGE_U) {
            int nx = x, ny = y - 1;
            if (ny >= 0)
                ctx->neighbours[ctx->n++] = ny * w + nx;
        }
        if (state->faces[vertex] & EDGE_D) {
            int nx = x, ny = y + 1;
            if (ny < state->h)
                ctx->neighbours[ctx->n++] = ny * w + nx;
        }
    }
    if (ctx->i < ctx->n)
        return ctx->neighbours[ctx->i++];
    else
        return -1;
}

int main(int argc, char *argv[]) {
    int w = 4;
    int h = 4;
    int x,y;
    gridstate *state = snew(gridstate);
    struct findloopstate *fls;
    struct neighbour_ctx ctx;

    state->w = w;
    state->h = h;
    state->faces = snewn(w*h, unsigned char);

/*
    +---+---+---+---+
    | o===o | o | o |
    +-I-+-I-+-I-+-I-+
    | o===o | o===o |
    +---+---+---+---+
    | o===o | o===o |
    +-I-+---+-I-+-I-+
    | o |   | o===o |
    +---+---+---+---+
*/

    state->faces[0] = EDGE_R | EDGE_D;
    state->faces[1] = EDGE_L | EDGE_D;
    state->faces[2] = EDGE_D;
    state->faces[3] = EDGE_D;
    
    state->faces[4] = EDGE_R | EDGE_U;
    state->faces[5] = EDGE_L | EDGE_U;
    state->faces[6] = EDGE_R | EDGE_U;
    state->faces[7] = EDGE_L | EDGE_U;
    
    state->faces[8] = EDGE_R | EDGE_D;
    state->faces[9] = EDGE_L;
    state->faces[10] = EDGE_R | EDGE_D;
    state->faces[11] = EDGE_L | EDGE_D;
    
    state->faces[12] = EDGE_U;
    state->faces[13] = 0x00;
    state->faces[14] = EDGE_R | EDGE_U;
    state->faces[15] = EDGE_L | EDGE_U;

    fls = findloop_new_state(w*h);
    ctx.state = state;

    if (findloop_run(fls, w*h, neighbour, &ctx)) {
        for (x = 0; x < w; x++)
        for (y = 0; y < h; y++) {
            int u, v;
            u = y*w + x;
            for (v = neighbour(u, &ctx); v >= 0; v = neighbour(-1, &ctx))
                if (findloop_is_loop_edge(fls, u, v))
                    printf("Face with loop: %i\n", u);
        }
    }
    findloop_free_state(fls);

    sfree(state->faces);
    sfree(state);

    return 0;
}
