/*

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

#define DIFFLIST(A) \
    A(EASY,Easy,e) \
    A(NORMAL,Normal,n) \
    A(HARD,Hard,h)
#define ENUM(upper,title,lower) DIFF_ ## upper,
#define TITLE(upper,title,lower) #title,
#define ENCODE(upper,title,lower) #lower
#define CONFIG(upper,title,lower) ":" #title
enum { DIFFLIST(ENUM) DIFFCOUNT };
static char const *const alcazar_diffnames[] = { DIFFLIST(TITLE) "(count)" };
static char const alcazar_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCONFIG DIFFLIST(CONFIG)

#define BLANK (0x00)
#define L (0x01)
#define R (0x02)
#define U (0x04)
#define D (0x08)

#define EDGE_NONE       (0x00)
#define EDGE_WALL       (0x01)
#define EDGE_PATH       (0x02)
#define EDGE_FIXED      (0x04)
#define EDGE_ERROR      (0x08)
#define EDGE_DRAG       (0x40)


char DIRECTIONS[4] = {L, R, U, D};

enum {
    SOLVED,
    INVALID,
    AMBIGUOUS
};

enum {
    COL_BACKGROUND,
    COL_GRID,
    COL_FLOOR_A,
    COL_FLOOR_B,
    COL_FIXED,
    COL_WALL_A,
    COL_WALL_B,
    COL_PATH,
    COL_DRAG,
    COL_ERROR,
    COL_FLASH,
    NCOLOURS
};

#define FLASH_TIME 0.7F

struct game_params {
    int w, h;
    int difficulty;
};

struct game_state {
    int w, h;
    int diff;
    unsigned char *edge_h;
    unsigned char *edge_v;
};

#define DEFAULT_PRESET 0
static const struct game_params alcazar_presets[] = {
    {4, 3,  DIFF_EASY},
    {4, 4,  DIFF_NORMAL},
    {6, 6,  DIFF_NORMAL},
    {6, 6,  DIFF_HARD},
    {8, 8,  DIFF_NORMAL},
    {8, 8,  DIFF_HARD}
};

static game_params *default_params(void) {
    game_params *ret = snew(game_params);
    *ret = alcazar_presets[DEFAULT_PRESET];
    return ret;
}

static bool game_fetch_preset(int i, char **name, game_params **params) {
    game_params *ret;
    char buf[64];

    if (i < 0 || i >= lenof(alcazar_presets)) return false;

    ret = default_params();
    *ret = alcazar_presets[i]; /* struct copy */
    *params = ret;

    sprintf(buf, "%dx%d %s",
            ret->w, ret->h,
            alcazar_diffnames[ret->difficulty]);
    *name = dupstr(buf);

    return true;
}

static void free_params(game_params *params) {
    sfree(params);
}

static game_params *dup_params(const game_params *params) {
    game_params *ret = snew(game_params);
    *ret = *params;       /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string) {
    params->w = params->h = atoi(string);
    while (*string && isdigit((unsigned char) *string)) ++string;
    if (*string == 'x') {
        string++;
        params->h = atoi(string);
        while (*string && isdigit((unsigned char)*string)) string++;
    }

    params->difficulty = DIFF_EASY;

    if (*string == 'd') {
        int i;
        string++;
        for (i = 0; i < DIFFCOUNT; i++)
            if (*string == alcazar_diffchars[i])
                params->difficulty = i;
        if (*string) string++;
    }
    return;
}

static char *encode_params(const game_params *params, bool full) {
    char buf[256];
    sprintf(buf, "%dx%d", params->w, params->h);
    if (full)
        sprintf(buf + strlen(buf), "d%c",
                alcazar_diffchars[params->difficulty]);
    return dupstr(buf);
}

static config_item *game_configure(const game_params *params) {
    config_item *ret;
    char buf[64];

    ret = snewn(4, config_item);

    ret[0].name = "Width";
    ret[0].type = C_STRING;
    sprintf(buf, "%d", params->w);
    ret[0].u.string.sval = dupstr(buf);

    ret[1].name = "Height";
    ret[1].type = C_STRING;
    sprintf(buf, "%d", params->h);
    ret[1].u.string.sval = dupstr(buf);

    ret[2].name = "Difficulty";
    ret[2].type = C_CHOICES;
    ret[2].u.choices.choicenames = DIFFCONFIG;
    ret[2].u.choices.selected = params->difficulty;

    ret[3].name = NULL;
    ret[3].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg) {
    game_params *ret = snew(game_params);

    ret->w = atoi(cfg[0].u.string.sval);
    ret->h = atoi(cfg[1].u.string.sval);
    ret->difficulty = cfg[2].u.choices.selected;

    return ret;
}

static const char *validate_params(const game_params *params, bool full) {
    if (params->w < 3) return "Width must be at least three";
    if (params->h < 3) return "Height must be at least three";
    if (params->difficulty < 0 || params->difficulty >= DIFFCOUNT)
        return "Unknown difficulty level";
    return NULL;
}

static char *game_text_format(const game_state *state);

static void print_grid(const game_state *state) {
    char *grid;
    grid = game_text_format(state);
    printf("%s", grid);
    sfree(grid);
}

int check_solution(const game_state *state, bool full) {
    /*
        - exactly two exits
        - every cell has two paths
        - all cells connected
        - no loops
    */
    int x,y,i;
    int count_walls;
    int count_paths;
    unsigned char *edges[4];
    int w = state->w;
    int h = state->h;
    for (y=0;y<h;y++)
    for (x=0;x<w;x++) {
        count_walls = 0;
        count_paths = 0;
        edges[0] = state->edge_h + y*w + x;
        edges[1] = edges[0] + w;
        edges[2] = state->edge_v + y*(w+1) + x;
        edges[3] = edges[2] + 1;
        for (i=0;i<4;i++) {
            if ((*edges[i] & EDGE_WALL) > 0x00) count_walls++;
            if ((*edges[i] & EDGE_PATH) > 0x00) count_paths++;
        }
        if (count_walls != 2 || count_paths != 2) return AMBIGUOUS;
    }
    return SOLVED;
}

bool solve_single_cells(game_state *state) {
    int i, x, y;
    bool changed = false;;
    int w = state->w;
    int h = state->h;

    for (y=0;y<h;y++)
    for (x=0;x<w;x++) {
        unsigned char *edges[4];
        unsigned char available_mask = 0;
        unsigned char path_mask = 0;
        unsigned char available_count = 0;
        unsigned char path_count = 0;

        edges[0] = state->edge_h + y*w + x;
        edges[1] = edges[0] + w;
        edges[2] = state->edge_v + y*(w+1) + x;
        edges[3] = edges[2] + 1;

        for (i = 0; i < 4; ++i) {
            if ((*edges[i] & EDGE_WALL) == 0) {
                available_mask |= (0x01 << i);
                available_count++;
            }
            if (*edges[i] & EDGE_PATH) {
                path_mask |= (0x01 << i);
                path_count++;
            }
        }
        if (available_count == 2 && path_count < 2) {
            for (i = 0; i < 4; i++) {
                if (available_mask & (0x01 << i)) *edges[i] |= EDGE_PATH;
                else                              *edges[i] |= EDGE_WALL;
            }
            changed = true;
        }
        else if (path_count == 2 && available_count > 2) {
            for (i = 0; i < 4; ++i) {
                if ((path_mask & (0x01 << i)) == 0) *edges[i] |= EDGE_WALL;
            }
            changed = true;
        }
    }
    return changed;
}

int alcazar_solve(game_state *state) {
    while(true) {
        if (solve_single_cells(state)) continue; 
        break;
    }

    return check_solution(state, false);
}

/*
 * Path generator
 * 
 * Employing the algorithm described at:
 * http://clisby.net/projects/hamiltonian_path/
 */

void reverse_path(int i1, int i2, int *pathx, int *pathy) {
    int i;
    int ilim = (i2-i1+1)/2;
    int temp;
    for (i=0; i<ilim; i++) {
        temp = pathx[i1+i];
        pathx[i1+i] = pathx[i2-i];
        pathx[i2-i] = temp;

        temp = pathy[i1+i];
        pathy[i1+i] = pathy[i2-i];
        pathy[i2-i] = temp;
    }
}

int backbite_left(int step, int n, int *pathx, int *pathy, int w, int h) {
    int neighx, neighy;
    int i, inPath = false;
    switch(step) {
        case L: neighx = pathx[0]-1; neighy = pathy[0];   break;
        case R: neighx = pathx[0]+1; neighy = pathy[0];   break;
        case U: neighx = pathx[0];   neighy = pathy[0]-1; break;
        case D: neighx = pathx[0];   neighy = pathy[0]+1; break; 
        default: neighx = -1; neighy = -1; break;
    }
    if (neighx < 0 || neighx >= w || neighy < 0 || neighy >= h)
        return n;

    for (i=1;i<n;i+=2) {
        if (neighx == pathx[i] && neighy == pathy[i]) { inPath = true; break; }
    }
    if (inPath) {
        reverse_path(0, i-1, pathx, pathy);
    }
    else {
        reverse_path(0, n-1, pathx, pathy);
        pathx[n] = neighx;
        pathy[n] = neighy;
        n++;
    }

    return n;
}

int backbite_right(int step, int n, int *pathx, int *pathy, int w, int h) {
    int neighx, neighy;
    int i, inPath = false;
    switch(step) {
        case L: neighx = pathx[n-1]-1; neighy = pathy[n-1];   break;
        case R: neighx = pathx[n-1]+1; neighy = pathy[n-1];   break;
        case U: neighx = pathx[n-1];   neighy = pathy[n-1]-1; break;
        case D: neighx = pathx[n-1];   neighy = pathy[n-1]+1; break; 
        default: neighx = -1; neighy = -1; break;
    }
    if (neighx < 0 || neighx >= w || neighy < 0 || neighy >= h)
        return n;

    for (i=n-2;i>=0;i-=2) {
        if (neighx == pathx[i] && neighy == pathy[i]) { inPath = true; break; }
    }
    if (inPath) {
        reverse_path(i+1, n-1, pathx, pathy);
    }
    else {
        pathx[n] = neighx;
        pathy[n] = neighy;
        n++;
    }

    return n;
}

int backbite(int n, int *pathx, int *pathy, int w, int h, random_state *rs) {
    return (random_upto(rs, 2) == 0) ?
        backbite_left( DIRECTIONS[random_upto(rs,4)], n, pathx, pathy, w, h) :
        backbite_right(DIRECTIONS[random_upto(rs,4)], n, pathx, pathy, w, h);
}

void generate_hamiltonian_path(game_state *state, random_state *rs) {
    int w = state->w;
    int h = state->h;
    int *pathx = snewn(w*h, int);
    int *pathy = snewn(w*h, int);
    int n = 1;
    int pos, x, y;
    pathx[0] = random_upto(rs, w);
    pathy[0] = random_upto(rs, h);

    while (n < w*h) {
        n = backbite(n, pathx, pathy, w, h, rs);
    }

    while (!(pathx[0] == 0 || pathx[0] == w-1) && 
           !(pathy[0] == 0 || pathy[0] == h-1)) {
        backbite_left(DIRECTIONS[random_upto(rs,4)], n, pathx, pathy, w, h);
    }

    while (!(pathx[n-1] == 0 || pathx[n-1] == w-1) && 
           !(pathy[n-1] == 0 || pathy[n-1] == h-1)) {
        backbite_right(DIRECTIONS[random_upto(rs,4)], n, pathx, pathy, w, h);
    }

    for (n=0;n<w*h;n++) {
        pos = pathx[n] + pathy[n]*w;
        x = pos % w;
        y = pos / w;
        if (n < (w*h-1)) {
            if      (pathx[n+1] - pathx[n] ==  1) state->edge_v[y*(w+1)+x+1] = EDGE_NONE;
            else if (pathx[n+1] - pathx[n] == -1) state->edge_v[y*(w+1)+x]   = EDGE_NONE;
            else if (pathy[n+1] - pathy[n] ==  1) state->edge_h[y*w+x+w]     = EDGE_NONE;
            else if (pathy[n+1] - pathy[n] == -1) state->edge_h[y*w+x]       = EDGE_NONE;
        }
        if (n == 0 || n == (w*h)-1) {
            if      (pathx[n] == 0)   state->edge_v[y*(w+1)+x]   = EDGE_NONE;
            else if (pathx[n] == w-1) state->edge_v[y*(w+1)+x+1] = EDGE_NONE;
            else if (pathy[n] == 0)   state->edge_h[y*w+x]       = EDGE_NONE;
            else if (pathy[n] == h-1) state->edge_h[y*w+x+w]     = EDGE_NONE;
        }
    }

    sfree(pathx);
    sfree(pathy);

    return;
}

static game_state *new_state(const game_params *params) {
    game_state *state = snew(game_state);

    state->w = params->w;
    state->h = params->h;
    state->diff = params->difficulty;

    state->edge_h = snewn(state->w*(state->h + 1), unsigned char);
    state->edge_v = snewn((state->w + 1)*state->h, unsigned char);

    memset(state->edge_h, EDGE_WALL | EDGE_FIXED, state->w*(state->h+1)*sizeof(unsigned char));
    memset(state->edge_v, EDGE_WALL | EDGE_FIXED, (state->w+1)*state->h*sizeof(unsigned char));

    return state;
}

static game_state *dup_state(const game_state *state) {
    game_state *ret = snew(game_state);
    ret->w = state->w;
    ret->h = state->h;
    ret->diff = state->diff;

    ret->edge_h = snewn(state->w*(state->h + 1), unsigned char);
    ret->edge_v = snewn((state->w + 1)*state->h, unsigned char);

    memcpy(ret->edge_h, state->edge_h, state->w*(state->h + 1) * sizeof(unsigned char));
    memcpy(ret->edge_v, state->edge_v, (state->w + 1)*state->h * sizeof(unsigned char));

    return ret;
}

static void free_state(game_state *state) {
    sfree(state->edge_v);
    sfree(state->edge_h);
    sfree(state);
}

static void count_edges(unsigned char edge, char **e, int *erun, int *wrun) {
    if ((edge & EDGE_WALL) == 0 && *wrun > 0) {
        *e += sprintf(*e, "%d", *wrun);
        *wrun = *erun = 0;
    }
    else if ((edge & EDGE_WALL) && *erun > 0) {
        while (*erun >= 26) {
            *(*e)++ = 'z';
            *erun -= 26;
        }
        if (*erun == 0) *wrun = 0;
        else {
            *(*e)++ = ('a' + *erun - 1);
            *erun = 0; *wrun = -1;
        }
    }
    if(edge & EDGE_WALL) (*wrun)++;
    else                 (*erun)++;
}

static char *new_game_desc(const game_params *params, random_state *rs,
                           char **aux, bool interactive) {
    game_state *new;
    game_state *tmp;
    
    char *desc, *e;
    int erun, wrun;
    int w = params->w;
    int h = params->h;
    int x,y;

    int i;
    int borderreduce;
    int bordernum = 0;
    int wallnum = 0;
    int ws = (w+1)*h + w*(h+1);
    int vo = w*(h+1);
    int *wallidx;

    borderreduce = 200; /* 2*w+2*h; */
    wallidx = snewn(ws, int);
    
    new = new_state(params);
    generate_hamiltonian_path(new, rs);
    /* print_grid(new); */

    for (i=0;i<w*(h+1);i++)
        if ((new->edge_h[i] & EDGE_WALL) > 0x00)
            wallidx[wallnum++] = i;
    for (i=0;i<(w+1)*h;i++)
        if ((new->edge_v[i] & EDGE_WALL) > 0x00)
            wallidx[wallnum++] = i+vo;

    shuffle(wallidx, wallnum, sizeof(int), rs);

    for (i=0;i<wallnum;i++) {
        int wi = wallidx[i];

        /* Remove border walls up to 'borderreduce' */
        /* Avoid two open border edges on a corner cell */
        if (wi<vo) {
            int xh, yh;
            xh = wi%w; yh = wi/w;
            /* printf("Horizontal edge at %i/%i (%i)\n",xh,yh,wi); */
            if ((xh == 0     && yh == 0 && ((new->edge_v[0]           & EDGE_WALL) == 0x00)) ||
                (xh == (w-1) && yh == 0 && ((new->edge_v[w]           & EDGE_WALL) == 0x00)) ||
                (xh == 0     && yh == h && ((new->edge_v[w*h]         & EDGE_WALL) == 0x00)) ||
                (xh == (w-1) && yh == h && ((new->edge_v[(w+1)*h-1]   & EDGE_WALL) == 0x00)) ||
                ((yh == 0 || yh == h) && (bordernum >= borderreduce)))
                continue;
        }
        else {
            int xv, yv;
            xv = (wi-vo)%(w+1); yv = (wi-vo)/(w+1);
            /* printf("Vertical edge at %i/%i (%i)\n",xv,yv,wi-vo); */
            if ((xv == 0 && yv == 0     && ((new->edge_h[0]           & EDGE_WALL) == 0x00)) ||
                (xv == w && yv == 0     && ((new->edge_h[w-1]         & EDGE_WALL) == 0x00)) ||
                (xv == 0 && yv == (h-1) && ((new->edge_h[w*h]         & EDGE_WALL) == 0x00)) ||
                (xv == w && yv == (h-1) && ((new->edge_h[w*(h+1)-1]   & EDGE_WALL) == 0x00)) ||
                ((xv == 0 || xv == w) && (bordernum >= borderreduce))) {
                continue;
            }
        }

        tmp = dup_state(new);
        /* Remove interior wall */
        if (wi<vo) tmp->edge_h[wi]    = EDGE_NONE;
        else       tmp->edge_v[wi-vo] = EDGE_NONE;
        
        if (alcazar_solve(tmp) == SOLVED) {
            /* printf("Remove edge at %i\n", wi); */
            if (wi<vo) new->edge_h[wi]    = EDGE_NONE;
            else       new->edge_v[wi-vo] = EDGE_NONE;
            if (wi<vo && (wi/w == 0 || wi/w == h)) bordernum++;
            else if ((wi-vo)%(w+1) == 0 || (wi-vo)%(w+1) == w) bordernum++;
        }
        /* else printf("Keep edge at %i\n", wi); */
        free_state(tmp);
    }
    /* printf("Removed %i border walls\n", bordernum); */
    print_grid(new);
    /* Encode walls */
    desc = snewn((w+1)*h + w*(h+1) + (w*h) + 1, char);
    e = desc;
    erun = wrun = 0;
    for (y=0;y<=h;y++)
    for (x=0;x<w;x++)
        count_edges(new->edge_h[y*w+x], &e, &erun, &wrun);
    if(wrun > 0) e += sprintf(e, "%d", wrun);
    if(erun > 0) *e++ = ('a' + erun - 1);
    *e++ = ',';
    erun = wrun = 0;
    for (y=0;y<h;y++)
    for (x=0;x<=w;x++) 
        count_edges(new->edge_v[y*(w+1)+x], &e, &erun, &wrun);
    if(wrun > 0) e += sprintf(e, "%d", wrun);
    if(erun > 0) *e++ = ('a' + erun - 1);
    *e++ = '\0';
    printf(" - %zu/%d\n", strlen(desc), (w+1)*h + w*(h+1) + (w*h) + 1);

    free_state(new);
    sfree(wallidx);

    return desc;
}

static const char *validate_desc(const game_params *params, const char *desc) {
    return NULL;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc) {
    int i, c;
    int w = params->w;
    int h = params->h;
    bool fh;
    game_state *state = new_state(params);

    i = 0; fh = true;
    while (*desc) {
        if(isdigit((unsigned char)*desc)) {
            for (c = atoi(desc); c > 0; c--) {
                if (fh) state->edge_h[i] = EDGE_WALL | EDGE_FIXED;
                else    state->edge_v[i] = EDGE_WALL | EDGE_FIXED;
                i++;
            }
            while (*desc && isdigit((unsigned char)*desc)) desc++;
        }
        else if(*desc >= 'a' && *desc <= 'z') {
            for (c = *desc - 'a' + 1; c > 0; c--) {
                if (fh) state->edge_h[i] = EDGE_NONE;
                else    state->edge_v[i] = EDGE_NONE;
                i++;
            }
            if (*desc < 'z' && i < (fh ? w*(h+1) : (w+1)*h)) {
                if (fh) state->edge_h[i] = EDGE_WALL | EDGE_FIXED;
                else    state->edge_v[i] = EDGE_WALL | EDGE_FIXED;
                i++;
            }
            desc++;
        }
        else if (*desc == ',') {
            /* printf("%d/%d horizontals\n", i, w*(h+1)); */
            assert(i == w*(h+1));
            fh = false;
            i = 0;
            desc++;
        }
    }
    /* printf("%d/%d verticals\n", i, (w+1)*h); */
    assert(i == (w+1)*h);
    return state;
}

static game_state *dup_game(const game_state *state) {
    return dup_state(state);
}

static void free_game(game_state *state) {
    free_state(state);
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error) {
    int i;
    int w = state->w;
    int h = state->h;
    char *move = snewn(w*h*40, char), *p = move;
    int voff = w*(h+1);
    
    game_state *solve_state = dup_game(state);
    alcazar_solve(solve_state);
    p += sprintf(p, "S");
    for (i = 0; i < w*(h+1); i++) {
        if (solve_state->edge_h[i] == EDGE_WALL)
            p += sprintf(p, ";W%d", i);
        if (solve_state->edge_h[i] == EDGE_NONE)
            p += sprintf(p, ";C%d", i);
        if (solve_state->edge_h[i] == EDGE_PATH)
            p += sprintf(p, ";P%d", i);
    }
    for (i = 0; i < (w+1)*h; i++) {
        if (solve_state->edge_v[i] == EDGE_WALL)
            p += sprintf(p, ";W%d", i+voff);
        if (solve_state->edge_v[i] == EDGE_NONE)
            p += sprintf(p, ";C%d", i+voff);
        if (solve_state->edge_v[i] == EDGE_PATH)
            p += sprintf(p, ";P%d", i+voff);
    }
    *p++ = '\0';
    move = sresize(move, p - move, char);
    free_game(solve_state);

    return move;
}

static bool game_can_format_as_text_now(const game_params *params) {
    return true;
}

static char *game_text_format(const game_state *state) {
    int x,y;
    int w = state->w;
    int h = state->h;
    bool iswall, isline, isleft, isright, isup, isdown;
    char *ret, *p;
    ret = snewn((9*w*h) + (3*w) + (6*h) + 1, char);
    p = ret;

    for (y=0;y<=h;y++) {
        for (x=0;x<w;x++) {
            iswall = state->edge_h[y*w + x] & EDGE_WALL;
            isline = state->edge_h[y*w + x] & EDGE_PATH;
            *p++ = '+';
            *p++ = iswall ? '-' : ' ';
            *p++ = isline ? '*' : iswall ? '-' : ' ';
            *p++ = iswall ? '-' : ' ';
        }
        *p++ = '+'; *p++ = '\n';
        if (y<h) {
            for (x=0;x<w;x++) {
                iswall  = state->edge_v[y*(w+1) + x]     & EDGE_WALL;
                isleft  = state->edge_v[y*(w+1) + x]     & EDGE_PATH;
                isright = state->edge_v[y*(w+1) + x + 1] & EDGE_PATH;
                isup    = state->edge_h[y*w + x]         & EDGE_PATH;
                isdown  = state->edge_h[y*w + x + w]     & EDGE_PATH;
                *p++ = isleft ? '*' : iswall ? '|' : ' ';
                *p++ = isleft ? '*' : ' ';
                *p++ = (isleft || isright || isup || isdown) ? '*' : ' ';
                *p++ = isright ? '*' : ' ';
            }
            iswall  = state->edge_v[y*(w+1) + w] & EDGE_WALL;
            isright = state->edge_v[y*(w+1) + w] & EDGE_PATH;
            *p++ = isright ? '*' : iswall ? '|' : ' '; 
            *p++ = '\n';
        }
    }
    *p++ =  0x00;
    
    return ret;
}

struct game_ui {
    int *dragcoords;       /* list of (y*w+x) coords in drag so far */
    int ndragcoords;       /* number of entries in dragcoords.
                            * 0 = click but no drag yet. -1 = no drag at all */
    int clickx, clicky;    /* pixel position of initial click */

    int curx, cury;        /* grid position of keyboard cursor */
    bool cursor_active;    /* true if cursor is shown */
    bool show_grid;        /* true if checkerboard grid is shown */
};


static game_ui *new_ui(const game_state *state) {
    game_ui *ui = snew(game_ui);

    ui->cursor_active = false;
    ui->show_grid = true;
    return ui;
}

static void free_ui(game_ui *ui) {

    sfree(ui);
}

static char *encode_ui(const game_ui *ui) {
    return NULL;
}

static void decode_ui(game_ui *ui, const char *encoding) {
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate) {
}

#define PREFERRED_TILE_SIZE (8)
#define TILESIZE (ds->tilesize)
#define BORDER (9*TILESIZE/2)

#define COORD(x) ( (x) * 8 * TILESIZE + BORDER )
#define CENTERED_COORD(x) ( COORD(x) + 4*TILESIZE )
#define FROMCOORD(x) ( ((x) < BORDER) ? -1 : ( ((x) - BORDER) / (8 * TILESIZE) ) )

struct game_drawstate {
    int tilesize;
    int w, h;
    bool tainted;
    unsigned short *cell;
};

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int x, int y, int button) {

    char buf[80];
    int w = state->w;
    int h = state->h;
    int fx = FROMCOORD(x);
    int fy = FROMCOORD(y); 
    int cx = CENTERED_COORD(fx);
    int cy = CENTERED_COORD(fy);

    if (button == LEFT_BUTTON || button == RIGHT_BUTTON) {
        int direction, edge;
        if ((fx<0 && fy<0) || (fx>=w && fy<0) || (fx<0 && fy>=h) || (fx>=w && fy>=h)) return NULL;
        if      (fx<0 && x > cx)  direction = R;
        else if (fx>=w && x < cx) direction = L;
        else if (fy<0 && y > cy)  direction = D;
        else if (fy>=h && y < cy) direction = U;
        else if (fx<0 || fx>=w || fy<0 || fy >=h) return NULL;
        else {
            if (abs(x-cx) < abs(y-cy)) direction = (y < cy) ? U : D;
            else                       direction = (x < cx) ? L : R;
        }

        printf("Click on %i/%i (%i/%i) direction %i\n",fx,fy,cx,cy,direction);


        if (direction == U || direction == D) {
            edge = direction == U ? fx+fy*w : fx+(fy+1)*w;
            if ((state->edge_h[edge] & EDGE_FIXED) > 0x00) return NULL;
            if ((state->edge_h[edge] & EDGE_PATH) > 0x00 || 
                (state->edge_h[edge] & EDGE_WALL) > 0x00) sprintf(buf,"C%d",edge);
            else sprintf(buf,"%c%d",(button == LEFT_BUTTON) ? 'P' : 'W', edge);
        }
        else {
            edge = direction == L ? fx+fy*(w+1) : (fx+1)+fy*(w+1);
            if ((state->edge_v[edge] & EDGE_FIXED) > 0x00) return NULL;
            if ((state->edge_v[edge] & EDGE_PATH) > 0x00 || 
                (state->edge_v[edge] & EDGE_WALL) > 0x00) sprintf(buf,"C%d",edge+w*(h+1));
            else sprintf(buf,"%c%d",(button == LEFT_BUTTON) ? 'P' : 'W', edge+w*(h+1));
        }
        return dupstr(buf);
    }

    if (button == 'G' || button == 'g') {
        ui->show_grid = !ui->show_grid;
        return UI_UPDATE;
    }
    return NULL;
}

static game_state *execute_move(const game_state *state, const char *move) {
    char c;
    unsigned char newedge;
    int edge, n;
    int w = state->w;
    int h = state->h;
    game_state *ret = dup_game(state);
    printf("Move: '%s'\n", move);

    while (*move) {
        c = *move;
        if (c == 'S') {
            move++;
        } 
        else if (c == 'W' || c == 'P' || c == 'C') {
            move++;
            if (sscanf(move, "%d%n", &edge, &n) < 1) goto badmove;
            newedge = (c=='W') ? EDGE_WALL :
                      (c=='P') ? EDGE_PATH :
                                 EDGE_NONE;
            if (edge < w*(h+1)) ret->edge_h[edge] = newedge;
            else ret->edge_v[edge-w*(h+1)] = newedge;
            move += n;
        }
        if (*move == ';')
            move++;
        else if (*move)
            goto badmove;
    }

    return ret;

badmove:
    free_game(ret);
    return NULL;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
                              int *x, int *y) {
   /* Ick: fake up `ds->tilesize' for macro expansion purposes */
    struct { int tilesize; } ads, *ds = &ads;
    ads.tilesize = tilesize;

    *x = (8 * (params->w) + 7) * TILESIZE;
    *y = (8 * (params->h) + 7) * TILESIZE;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize) {
    ds->tilesize = tilesize;
}

static float *game_colours(frontend *fe, int *ncolours) {
    float *ret = snewn(3 * NCOLOURS, float);

    frontend_default_colour(fe, &ret[COL_BACKGROUND * 3]);

    ret[COL_GRID * 3 + 0] = 0.0F;
    ret[COL_GRID * 3 + 1] = 0.0F;
    ret[COL_GRID * 3 + 2] = 0.0F;

    ret[COL_FLOOR_A * 3 + 0] = 0.8F;
    ret[COL_FLOOR_A * 3 + 1] = 0.8F;
    ret[COL_FLOOR_A * 3 + 2] = 0.8F;

    ret[COL_FLOOR_B * 3 + 0] = 0.6F;
    ret[COL_FLOOR_B * 3 + 1] = 0.6F;
    ret[COL_FLOOR_B * 3 + 2] = 0.6F;

    ret[COL_FIXED * 3 + 0] = 0.0F;
    ret[COL_FIXED * 3 + 1] = 0.0F;
    ret[COL_FIXED * 3 + 2] = 0.0F;

    ret[COL_WALL_A * 3 + 0] = 0.1F;
    ret[COL_WALL_A * 3 + 1] = 0.1F;
    ret[COL_WALL_A * 3 + 2] = 0.1F;

    ret[COL_WALL_B * 3 + 0] = 0.9F;
    ret[COL_WALL_B * 3 + 1] = 0.5F;
    ret[COL_WALL_B * 3 + 2] = 0.0F;

    ret[COL_PATH * 3 + 0] = 0.1F;
    ret[COL_PATH * 3 + 1] = 0.1F;
    ret[COL_PATH * 3 + 2] = 0.9F;

    ret[COL_DRAG * 3 + 0] = 1.0F;
    ret[COL_DRAG * 3 + 1] = 0.0F;
    ret[COL_DRAG * 3 + 2] = 1.0F;

    ret[COL_ERROR * 3 + 0] = 1.0F;
    ret[COL_ERROR * 3 + 1] = 0.0F;
    ret[COL_ERROR * 3 + 2] = 0.0F;

    ret[COL_FLASH * 3 + 0] = 1.0F;
    ret[COL_FLASH * 3 + 1] = 1.0F;
    ret[COL_FLASH * 3 + 2] = 1.0F;

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state) {
    int i;
    int w = state->w;
    int h = state->h;

    struct game_drawstate *ds = snew(struct game_drawstate);

    ds->tilesize = 0;
    ds->w = w;
    ds->h = h;
    ds->tainted = true;
    ds->cell   = snewn((8*w+7)*(8*h+7), unsigned short);

    for (i=0;i<(8*w+7)*(8*h+7); i++) ds->cell[i] = 0x0000;

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds) {
    sfree(ds->cell);
    sfree(ds);
}

static void draw_cell(drawing *dr, game_drawstate *ds, int pos) {
    int i;
    int dx, dy, col;
    int w = ds->w;
    int h = ds->h;
    int x = pos%(8*w+7);
    int y = pos/(8*w+7);
    int ox = x*ds->tilesize;
    int oy = y*ds->tilesize;
    int ts2 = (ds->tilesize+1)/2;

    clip(dr, ox, oy, ds->tilesize, ds->tilesize);

    if ((ds->cell[pos] & (EDGE_WALL | EDGE_FIXED)) == (EDGE_WALL|EDGE_FIXED)) {
        draw_rect(dr, ox, oy, ds->tilesize, ds->tilesize, COL_FIXED);
    }
    else if ((ds->cell[pos] & EDGE_PATH) == EDGE_PATH) {
        draw_rect(dr, ox, oy, ds->tilesize, ds->tilesize, COL_PATH);
    }
    else {
        for (i=0;i<4;i++) {
            unsigned char bg = (ds->cell[pos] >> 8+2*i) & 0x03;
            if      (i==0) { dx = ox;     dy = oy; }
            else if (i==1) { dx = ox;     dy = oy+ts2; }
            else if (i==2) { dx = ox+ts2; dy = oy; }
            else           { dx = ox+ts2; dy = oy+ts2; }
            col = (bg==0x00) ? COL_BACKGROUND :
                  (bg==0x01) ? COL_FLOOR_A :
                               COL_FLOOR_B;
            draw_rect(dr, dx, dy, ts2, ts2, col);
        }
        if ((ds->cell[pos] & EDGE_WALL) == EDGE_WALL) {
            int c[8];
            c[0] = ox; c[1] = oy;
            c[2] = ox; c[3] = oy+ts2;
            c[4] = ox+ts2; c[5] = oy;
            draw_polygon(dr, c, 3, COL_WALL_A, COL_WALL_A);
            c[0] = ox; c[1] = oy+ts2; 
            c[2] = ox; c[3] = oy+ds->tilesize;
            c[4] = ox+ds->tilesize; c[5] = oy; 
            c[6] = ox+ts2; c[7] = oy;
            draw_polygon(dr, c, 4, COL_WALL_B, COL_WALL_B);
            c[0] = ox; c[1] = oy+ds->tilesize; 
            c[2] = ox+ts2; c[3] = oy+ds->tilesize;
            c[4] = ox+ds->tilesize; c[5] = oy+ts2; 
            c[6] = ox+ds->tilesize; c[7] = oy;
            draw_polygon(dr, c, 4, COL_WALL_A, COL_WALL_A);
            c[0] = ox+ts2; c[1] = oy+ds->tilesize;
            c[2] = ox+ds->tilesize; c[3] = oy+ds->tilesize;
            c[4] = ox+ds->tilesize; c[5] = oy+ts2;
            draw_polygon(dr, c, 3, COL_WALL_B, COL_WALL_B);
        }
    }
    draw_update(dr, ox, oy, ds->tilesize, ds->tilesize);
    unclip(dr);
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime) {
    int i,j,w,h,x,y,cx,cy;
    unsigned short bg[4];
    unsigned short *newcell;
    bool flash = (bool)(flashtime * 5 / FLASH_TIME) % 2;

    w = state->w; h = state->h;
    newcell = snewn((8*w+7)*(8*h+7), unsigned short);
    for (i=0;i<((8*w)+7)*((8*h)+7); i++) {
        x = i%(8*w+7)-4; y = i/(8*w+7)-4;
        cx = x/8; cy = y/8;
        newcell[i] = 0x0000;

        /* Corner border cells. Unused. */
        if ((x<0 && y<0) || (x<0 && y>8*h) || (x>8*w && y<0) || (x>8*w && y>8*h)) {}

        /* Left / Right border cells */
        else if (x<0 || x>8*w) {
            if (y%8==4 && x==-1)    newcell[i] |= state->edge_v[cy*(w+1)]         & (EDGE_PATH|EDGE_ERROR);
            if (y%8==4 && x==8*w+1) newcell[i] |= state->edge_v[w+cy*(w+1)]   & (EDGE_PATH|EDGE_ERROR);
        }

        /* Top / Bottom border cells */
        else if (y<0 || y>8*h) {
            if (x%8==4 && y==-1)   newcell[i] |= state->edge_h[cx]         & (EDGE_PATH|EDGE_ERROR);
            if (x%8==4 && y==8*h+1) newcell[i] |= state->edge_h[cx+h*w] & (EDGE_PATH|EDGE_ERROR);
        }

        /* 4-Corner cells */
        else if ((x%8 == 0) && (y%8 == 0)) {
            if (ui->show_grid) {
                if (cx > 0 && cy > 0) newcell[i] |= ((cx-1)^(cy-1))&1 ? 0x0100:0x0200;
                if (cx > 0 && cy < h) newcell[i] |= ((cx-1)^ cy)   &1 ? 0x0400:0x0800;
                if (cx < w && cy > 0) newcell[i] |= ( cx   ^(cy-1))&1 ? 0x1000:0x2000;
                if (cx < w && cy < h) newcell[i] |= ( cx   ^ cy)   &1 ? 0x4000:0x8000;
            }
            if (cx > 0) newcell[i] |= state->edge_h[(cx-1)+cy*w]     & (EDGE_WALL | EDGE_FIXED);
            if (cx < w) newcell[i] |= state->edge_h[cx+cy*w]         & (EDGE_WALL | EDGE_FIXED);
            if (cy > 0) newcell[i] |= state->edge_v[cx+(cy-1)*(w+1)] & (EDGE_WALL | EDGE_FIXED);
            if (cy < h) newcell[i] |= state->edge_v[cx+cy*(w+1)]     & (EDGE_WALL | EDGE_FIXED);
        }

        /* Horizontal edge cells */
        else if (y%8 == 0) {
            if (ui->show_grid) {
                if (cy > 0) newcell[i] |= (cx^(cy-1))&1 ? 0x1100:0x2200;
                if (cy < h) newcell[i] |= (cx^ cy)   &1 ? 0x4400:0x8800;
            }
            newcell[i] |= state->edge_h[cx+cy*w] & (EDGE_WALL|EDGE_FIXED);
            if (x%8==4) newcell[i] |= state->edge_h[cx+cy*w] & (EDGE_PATH|EDGE_ERROR);
        }

        /* Vertical edge cells */
        else if (x%8 == 0) {
            if (ui->show_grid) {
                if (cx > 0) newcell[i] |= ((cx-1)^cy)&1 ? 0x0500:0x0a00;
                if (cx < w) newcell[i] |= ( cx   ^cy)&1 ? 0x5000:0xa000;
            }
            newcell[i] |= state->edge_v[cx+cy*(w+1)] & (EDGE_WALL|EDGE_FIXED);
            if (y%8==4) newcell[i] |= state->edge_v[cx+cy*(w+1)] & (EDGE_PATH|EDGE_ERROR);
        }
        
        /* Path cells */
        else {
            if (ui->show_grid) 
                for (j=0;j<4;j++) newcell[i] |= ((cx^cy)&1 ? 0x01:0x02) << (8+2*j);
            if (x%8==4 && y%8<=4) newcell[i] |= state->edge_h[cx+cy*w]         & (EDGE_PATH|EDGE_ERROR);
            if (x%8==4 && y%8>=4) newcell[i] |= state->edge_h[cx+(cy+1)*w]     & (EDGE_PATH|EDGE_ERROR);
            if (y%8==4 && x%8<=4) newcell[i] |= state->edge_v[cx+cy*(w+1)]     & (EDGE_PATH|EDGE_ERROR);
            if (y%8==4 && x%8>=4) newcell[i] |= state->edge_v[(cx+1)+cy*(w+1)] & (EDGE_PATH|EDGE_ERROR);
        }
    }
    for (i=0;i<((8*w)+7)*((8*h)+7); i++) {
        if (newcell[i] != ds->cell[i] || ds->tainted) {
            ds->cell[i] = newcell[i];
            draw_cell(dr, ds, i);
        }
    }
    ds->tainted = false;
    sfree(newcell);
}

static float game_anim_length(const game_state *oldstate,
                              const game_state *newstate, int dir, game_ui *ui) {
    return 0.0F;
}

static float game_flash_length(const game_state *oldstate,
                               const game_state *newstate, int dir, game_ui *ui) {
    return 0.0F;
}

static void game_get_cursor_location(const game_ui *ui,
                                     const game_drawstate *ds,
                                     const game_state *state,
                                     const game_params *params,
                                     int *x, int *y, int *w, int *h) {
}

static int game_status(const game_state *state) {
    return 0;
}

static bool game_timing_state(const game_state *state, game_ui *ui) {
    return true;
}

static void game_print_size(const game_params *params, float *x, float *y) {
}

static void game_print(drawing *dr, const game_state *state, int tilesize) {
}

#ifdef COMBINED
#define thegame alcazar
#endif

const struct game thegame = {
    "Alcazar", "games.alcazar", "alcazar",
    default_params,
    game_fetch_preset, NULL,
    decode_params,
    encode_params,
    free_params,
    dup_params,
    true, game_configure, custom_params,
    validate_params,
    new_game_desc,
    validate_desc,
    new_game,
    dup_game,
    free_game,
    true, solve_game,
    true, game_can_format_as_text_now, game_text_format,
    new_ui,
    free_ui,
    encode_ui,
    decode_ui,
    NULL, /* game_request_keys */
    game_changed_state,
    interpret_move,
    execute_move,
    PREFERRED_TILE_SIZE, game_compute_size, game_set_size,
    game_colours,
    game_new_drawstate,
    game_free_drawstate,
    game_redraw,
    game_anim_length,
    game_flash_length,
    game_get_cursor_location,
    game_status,
    false, false, game_print_size, game_print,
    false,                 /* wants_statusbar */
    false, game_timing_state,
    REQUIRE_RBUTTON,       /* flags */
};

#ifdef DEVELOP

#include <time.h>
int main(int argc, char **argv) {
    game_params *p;
    random_state *rs;
    game_state *state;
    char *desc;
    int i;
    time_t seed = time(NULL);

    rs = random_new((void*)&seed, sizeof(time_t));
    /* rs = random_new("123455", 6); */
    p = default_params();
    p->w = 3;
    p->h = 4;
    p->difficulty = DIFF_EASY;

    for (i=0;i<1;i++) {
        desc = new_game_desc(p, rs, NULL, 0);
        printf("%s\n", desc);
        state = new_game(NULL, p, desc);
        print_grid(state);
        alcazar_solve(state);
        print_grid(state);
        free_state(state);
        sfree(desc);
        printf("\n");
    }

    random_free(rs);
    free_params(p);
    return 0;
}

#endif

