/*
 * walls.c
 */

/* 
 * TODO:
 *  
 *  - Error handling: 
 *      - Internal loops
 *  - Implement line dragging
 *  - Solver:
 *      - Implement board partition check
 *      - Implement exit parity check
 *      - Implement area parity check
 *      - Implement backtracking
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

/* Macro ickery copied from slant.c */
#define DIFFLIST(A) \
    A(EASY,Easy,e) \
    A(NORMAL,Normal,n) \
    A(TRICKY,Tricky,t) \
    A(HARD,Hard,h)
#define ENUM(upper,title,lower) DIFF_ ## upper,
#define TITLE(upper,title,lower) #title,
#define ENCODE(upper,title,lower) #lower
#define CONFIG(upper,title,lower) ":" #title
enum { DIFFLIST(ENUM) DIFFCOUNT };
static char const *const walls_diffnames[] = { DIFFLIST(TITLE) "(count)" };
static char const walls_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCONFIG DIFFLIST(CONFIG)

#define BLANK (0x00)
#define L (0x01)
#define R (0x02)
#define U (0x04)
#define D (0x08)

char DIRECTIONS[4] = {L, R, U, D};

enum {
    COL_BACKGROUND,
    COL_FLOOR_A,
    COL_FLOOR_B,
    COL_FIXED,
    COL_WALL,
    COL_GRID,
    COL_LINE,
    COL_DRAGLINE,
    COL_ERROR,
    COL_FLASH,
    NCOLOURS
};

#define FLASH_TIME 0.7F

enum {
    TC_CON,
    TC_DIS,
    TC_UNK
};

struct game_params {
    int w, h;
    int difficulty;
};

struct shared_state {
    int w, h, diff, wh, nw;
    bool *fixed; /* size (w+1)*h + w*(h+1): fixed walls */
    int refcnt;
};

struct game_state {
    struct shared_state *shared;
    char *walls;        /* size (w+1)*h + w*(h+1): placed walls / lines */
    bool *errors;       /* size (w+1)*h + w*(h+1): errors detected */
    int completed, used_solve;
};

#define DEFAULT_PRESET 0
static const struct game_params walls_presets[] = {
    {4, 4,  DIFF_EASY},
    {4, 4,  DIFF_NORMAL},
    {6, 6,  DIFF_NORMAL},
    {6, 6,  DIFF_TRICKY},
    {8, 8,  DIFF_NORMAL},
    {8, 8,  DIFF_TRICKY}    
};

static game_params *default_params(void) {
    game_params *ret = snew(game_params);
    *ret = walls_presets[DEFAULT_PRESET];
    return ret;
}

static bool game_fetch_preset(int i, char **name, game_params **params) {
    game_params *ret;
    char buf[64];

    if (i < 0 || i >= lenof(walls_presets)) return false;

    ret = default_params();
    *ret = walls_presets[i]; /* struct copy */
    *params = ret;

    sprintf(buf, "%dx%d %s",
            ret->w, ret->h,
            walls_diffnames[ret->difficulty]);
    *name = dupstr(buf);

    return true;
}

static void free_params(game_params *params)
{
    sfree(params);
}

static game_params *dup_params(const game_params *params) {
    game_params *ret = snew(game_params);
    *ret = *params;               /* structure copy */
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
            if (*string == walls_diffchars[i])
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
                walls_diffchars[params->difficulty]);
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

static game_state *new_state(const game_params *params) {
    int i;
    game_state *state = snew(game_state);

    state->completed = state->used_solve = false;

    state->shared = snew(struct shared_state);
    state->shared->w = params->w;
    state->shared->h = params->h;
    state->shared->diff = params->difficulty;
    state->shared->wh = params->w * params->h;
    state->shared->nw = (params->w + 1)*params->h + params->w*(params->h + 1);
    state->shared->fixed = snewn(state->shared->nw, bool);
    state->shared->refcnt = 1;

    state->walls  = snewn(state->shared->nw, char);
    state->errors = snewn(state->shared->nw, bool);

    for (i=0;i<state->shared->nw;i++) {
        state->shared->fixed[i] = false;
        state->walls[i] = TC_UNK;
        state->errors[i]= false;
    }

    return state;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc) {
    int i, c;
    game_state *state = new_state(params);

    i = 0;
    while (*desc) {
        if(isdigit((unsigned char)*desc)) {            
            for (c = atoi(desc); c > 0; c--) {
                state->walls[i] = TC_DIS;
                state->shared->fixed[i] = true;
                i++;
            }
            while (*desc && isdigit((unsigned char)*desc)) desc++;
        }
        else if(*desc >= 'a' && *desc <= 'z') {
            for (c = *desc - 'a' + 1; c > 0; c--) {
                state->walls[i] = TC_UNK;
                state->shared->fixed[i] = false;
                i++;
            }
            if (*desc < 'z' && i < state->shared->nw) {
                state->walls[i] = TC_DIS;
                state->shared->fixed[i] = true;
                i++;
            }
            desc++;
        }
    }  

    assert(i == state->shared->nw);

    return state;
}

static game_state *dup_game(const game_state *state) {
    int i;
    game_state *ret = snew(game_state);

    ret->shared = state->shared;
    ret->completed = state->completed;
    ret->used_solve = state->used_solve;
    ++ret->shared->refcnt;

    ret->walls = snewn(state->shared->nw, char);
    ret->errors = snewn(state->shared->nw, bool);
    for (i=0;i<state->shared->nw; i++) {
        ret->walls[i] = state->walls[i];
        ret->errors[i] = state->errors[i];
    }
  
    return ret;
}

static void free_game(game_state *state) {
    assert(state);
    if (--state->shared->refcnt == 0) {
        sfree(state->shared->fixed);
        sfree(state->shared);
    }
    sfree(state->errors);
    sfree(state->walls);
    sfree(state);
}

/* ----------------------------------------------------------------------
 * Solver.
 */


static char *game_text_format(const game_state *state);

void print_grid(int w, int h, const char *walls) {
    game_params *params = default_params();
    game_state *state;
    int i;
    params->w = w;
    params->h = h;
    state = new_state(params);
    
    for (i=0;i<state->shared->nw;i++)
        state->walls[i] = walls[i];
    
    sfree(game_text_format(state)); printf("\n");
    
    free_game(state);
    free_params(params);
}

enum {
    SOLVED,
    INVALID,
    AMBIGUOUS
};

int grid_to_wall(int g, int w, int h, int dir) {
    int wall = -1;
    int x = g % w;
    int y = g / w;
    switch(dir) {
        case L: wall = (w+1)*y + x; break;
        case R: wall = (w+1)*y + x + 1; break;
        case U: wall = (w+1)*h + w*y + x; break;
        case D: wall = (w+1)*h + w*y + x + w; break;
    }
    assert(wall >= 0);
    return wall;
}

int wall_to_grid(int wall, int w, int h, int dir) {
    int grid = -2;
    int ws = (w+1)*h;
    int x = (wall < ws) ? wall % (w+1) : (wall-ws) % w;
    int y = (wall < ws) ? wall / (w+1) : (wall-ws) / w;
    if (wall < ws) {
        switch(dir) {
            case L: grid = (x > 0) ? y*w + (x-1) : -1; break;
            case R: grid = (x < w) ? y*w + x     : -1; break;
        }
    }
    else {
        switch(dir) {
            case U: grid = (y > 0) ? (y-1)*w + x : -1; break;
            case D: grid = (y < h) ? y*w + x : -1; break;
        }
    }
    assert(grid >= -1);
    return grid;
}

bool is_border_wall(int wall, int w, int h) {
    
    int ws = (w+1)*h;
    int x = wall % (w+1);
    int y = (wall - ws) / (w);
    
    if (wall < ws && x == 0) return true;
    if (wall < ws && x == w) return true;
    if (wall >= ws && y == 0) return true;
    if (wall >= ws && y == h) return true;
    
    return false;
}

int vertex_to_grid(int v, int w, int h, int dir) {
    int x = v % (w+1);
    int y = v / (w+1);
    if (dir == 0) {
        if (x == 0 && y == 0) return -1;
        if (x == 0) return w*h + 2*w + (y-1);
        if (y == 0) return w*h + (x-1);
        return (x-1) + (y-1)*w; 
    }
    if (dir == 1) {
        if (x == w && y == 0) return -1;
        if (x == w) return w*h + 2*w + h + (y-1);
        if (y == 0) return w*h + x;
        return x + (y-1)*w;
    }
    if (dir == 2) {
        if (x == 0 && y == h) return -1;
        if (x == 0) return w*h + 2*w + y;
        if (y == h) return w*h + w + (x-1);
        return (x-1) + y*w;
    }
    if (dir == 3) {
        if (x == w && y == h) return -1;
        if (x == w) return w*h + 2*w + h + y;
        if (y == h) return w*h + w + x;
        return x + y*w;
    }
    return -1;
}

int vertex_to_wall(int v, int w, int h, int dir) {
    int x = v % (w+1);
    int y = v / (w+1);
    if (dir == L) return (x == 0) ? -1 : (w+1)*h + (x-1) + y*(w);
    if (dir == R) return (x == w) ? -1 : (w+1)*h + x + y*(w);
    if (dir == U) return (y == 0) ? -1 : x + (y-1)*(w+1);
    if (dir == D) return (y == h) ? -1 : x + y*(w+1);
    return -1;
}

int wall_to_vertex(int wall, int w, int h, int dir) {
    if (wall < (w+1)*h) {
        if (dir == U) return wall;
        if (dir == D) return wall + (w+1);
    }
    else {
        int y = (wall-(w+1)*h) / w;
        if (dir == L) return wall - (w+1)*h +y;
        if (dir == R) return (wall +1) - (w+1)*h +y;
    }
    return -1;
}


int grid_to_vertex(int g, int w, int h, int dir) {
    int x = g % w;
    int y = g / w;
    
    if (dir == 0) return x + y*(w+1);
    if (dir == 1) return (x+1) + y*(w+1);
    if (dir == 2) return x + (y+1)*(w+1);
    if (dir == 3) return (x+1) + (y+1)*(w+1);
    return -1;
}

int check_solution(int w, int h, const char *walls, bool *errors) {
    
    int i,j;
    int *dsf;
    int first_eq;
    int exit1, exit2;
    int ws = (w+1)*h+w*(h+1);

    char *twalls = snewn(ws, char);

    bool surplus_exits = false;
    bool correct_exits = true;
    bool invalid_cells = false;
    bool free_cells = false;
    bool cells_connected = true;
    
    dsf = snewn(w*h,int);
    dsf_init(dsf, w*h);
    
    for (i=0;i<ws;i++) {
        twalls[i] = walls[i];
        if (errors != NULL) errors[i] = false;
    }
    
    for (i=0;i<w*h;i++) {
        char edges[4];
        int linecount = 0;
        int wallcount = 0;
        
        for (j=0;j<4;j++)
            edges[j] = twalls[grid_to_wall(i,w,h,DIRECTIONS[j])];
        
        for (j=0;j<4;j++) {
            if (edges[j] == TC_CON) linecount++;
            if (edges[j] == TC_DIS) wallcount++;
        }

        if (errors == NULL && (linecount > 2 || wallcount > 2)) goto invalid;

        if (linecount == 2) {
            for (j=0;j<4;j++) 
                if (edges[j] != TC_CON) twalls[grid_to_wall(i,w,h,DIRECTIONS[j])] = TC_DIS;            
        }
    }
    
    exit1 = exit2 = -1;
    
    for (i=0;i<w*h;i++) {
        char edges[4];
        int wallcount, linecount, freecount;
        int x = i % w;
        int y = i / w;

        for (j=0;j<4;j++)
            edges[j] = twalls[grid_to_wall(i,w,h,DIRECTIONS[j])];
        
        wallcount = linecount = freecount = 0;
        
        for (j=0;j<4;j++) {
            switch(edges[j]) {
                case TC_UNK: freecount++; break;
                case TC_CON: linecount++; break;
                case TC_DIS: wallcount++; break;               
            }            
        }
        if (freecount > 0) free_cells = true;
        if ((wallcount > 2) || (linecount > 2)) {
            invalid_cells = true;
            if (errors == NULL) goto invalid;
            
            if (errors != NULL && linecount > 2) {
                for (j=0;j<4;j++)
                    if (edges[j] == TC_CON) errors[grid_to_wall(i,w,h,DIRECTIONS[j])] = true;
            }
        }
        
        if (linecount < 3) {
            if ((edges[0] != TC_DIS) && (x > 0))   dsf_merge(dsf, i, i-1);
            if ((edges[1] != TC_DIS) && (x < w-1)) dsf_merge(dsf, i, i+1);
            if ((edges[2] != TC_DIS) && (y > 0))   dsf_merge(dsf, i, i-w);
            if ((edges[3] != TC_DIS) && (y < h-1)) dsf_merge(dsf, i, i+w);            
        }
        
        if (((edges[0] == TC_CON) && (x == 0))   ||
            ((edges[1] == TC_CON) && (x == w-1)) ||
            ((edges[2] == TC_CON) && (y == 0))   || 
            ((edges[3] == TC_CON) && (y == h-1))) {
            if (exit2 != -1) { 
                surplus_exits = true;
                if (errors == NULL) goto invalid;
            }
            if (exit1 != -1) exit2 = i; else exit1 = i;
        }
    }
    
    if (exit1 == -1 || exit2 == -1) correct_exits = false;

    first_eq = dsf_canonify(dsf, 0);
    for (i=1;i<w*h;i++) {
        if (dsf_canonify(dsf, i) != first_eq) {
            cells_connected = false;
            break;
        }
    }
    sfree(twalls);
    sfree(dsf);

    if (errors != NULL && surplus_exits) {
        for (i=0;i<w;i++) {
            if (walls[grid_to_wall(i,w,h,U)] == TC_CON)
                errors[grid_to_wall(i,w,h,U)] = true;
            if (walls[grid_to_wall(i+(w*(h-1)),w,h,D)] == TC_CON)
                errors[grid_to_wall(i+(w*(h-1)),w,h,D)] = true;
        }
        for (i=0;i<h;i++) {
            if (walls[grid_to_wall(i*w,w,h,L)] == TC_CON)
                errors[grid_to_wall(i*w,w,h,L)] = true;
            if (walls[grid_to_wall(i*w+(w-1),w,h,R)] == TC_CON)
                errors[grid_to_wall(i*w+(w-1),w,h,R)] = true;
        }
    }

    if (invalid_cells) return INVALID;
    if (surplus_exits) return INVALID;
    if (!cells_connected) return INVALID;
    if (free_cells) return AMBIGUOUS;
    if (!correct_exits) return INVALID;
    
    return SOLVED;
    
invalid:
    sfree(twalls);
    sfree(dsf);
    return INVALID;
}

bool solve_single_cells(int w, int h, char *walls) {
  
    int i,j;
    bool ret = false;
       
    for (i=0;i<w*h;i++) {
        int edges[4];
        int cells[4];
        int wallcount = 0;
        int pathcount = 0;
        int freecount = 0;
        
        for (j=0;j<4;j++) {
            cells[j] = grid_to_wall(i,w,h,DIRECTIONS[j]);
            edges[j] = walls[cells[j]];
        }
        
        for (j=0;j<4;j++) {
            if (edges[j] == TC_CON) pathcount++;
            if (edges[j] == TC_DIS) wallcount++;
            if (edges[j] == TC_UNK) freecount++;
        }
        
        if ((wallcount > 2) || (pathcount > 2)) return false;
        
        if (wallcount == 2 && freecount > 0) {
            for (j=0;j<4;j++)
                if (walls[cells[j]] == TC_UNK) walls[cells[j]] = TC_CON;
            ret = true;
        }
        else if (pathcount == 2 && freecount > 0) {
            for (j=0;j<4;j++)
                if (walls[cells[j]] == TC_UNK) walls[cells[j]] = TC_DIS;
            ret = true;
        }
    }
    
    return ret;
}

bool solve_check_loops(int w, int h, int diff, char *walls) {
  
    int i,j;
    int ws = (w+1)*h+w*(h+1);
    char *testwalls = snewn(ws, char);
 
    for (i=0;i<ws;i++) {
        if (walls[i] == TC_UNK) {
            for (j=0;j<ws;j++) testwalls[j] = walls[j];
            testwalls[i] = TC_DIS;
            if (diff < DIFF_TRICKY)
                for (j=0;j<2;j++) {
                    if (solve_single_cells(w,h,testwalls)) break;
                }
            else
                while (solve_single_cells(w,h,testwalls)) {}
            if (check_solution(w,h,testwalls,NULL) == INVALID) {
                walls[i] = TC_CON;
                sfree(testwalls);
                return true;                
            }
            for (j=0;j<ws;j++) testwalls[j] = walls[j];
            testwalls[i] = TC_CON;
            if (diff < DIFF_TRICKY)
                for (j=0;j<2;j++) {
                    if (solve_single_cells(w,h,testwalls)) break;
                }
            else
                while (solve_single_cells(w,h,testwalls)) {}
            if (check_solution(w,h,testwalls,NULL) == INVALID) {
                walls[i] = TC_DIS;
                sfree(testwalls);
                return true;                
            }        
        }
    }    
    sfree(testwalls);
    return false;
}


int walls_solve(int w, int h, char *walls, int diff) {
    
    while(true) {
        
        if (solve_single_cells(w, h, walls)) continue;
        if (diff == DIFF_EASY) break;
        
        if (solve_check_loops(w, h, diff, walls)) continue; 
 
        break;
    }

    return check_solution(w,h,walls,NULL);
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
    for (i=0; i<ilim; i++)
    {
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
    if (neighx < 0 || neighx >= w || neighy < 0 || neighy >= h) return n;
    
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
    if (neighx < 0 || neighx >= w || neighy < 0 || neighy >= h) return n;
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
    int w = state->shared->w;
    int h = state->shared->h;
    int *pathx = snewn(w*h, int);
    int *pathy = snewn(w*h, int);
    int n = 1;
    
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

    for (n=0;n<state->shared->nw;n++)
        state->walls[n] = TC_DIS;

    for (n=0;n<w*h;n++) {
        int pos = pathx[n] + pathy[n]*w;
        
        if (n < (w*h-1)) {
            if      (pathx[n+1] - pathx[n] ==  1) state->walls[grid_to_wall(pos, w, h, R)] = TC_UNK;
            else if (pathx[n+1] - pathx[n] == -1) state->walls[grid_to_wall(pos, w, h, L)] = TC_UNK;
            else if (pathy[n+1] - pathy[n] ==  1) state->walls[grid_to_wall(pos, w, h, D)] = TC_UNK;
            else if (pathy[n+1] - pathy[n] == -1) state->walls[grid_to_wall(pos, w, h, U)] = TC_UNK;
        }
        if (n == 0 || n == (w*h)-1) {
            if      (pathx[n] == 0)   state->walls[grid_to_wall(pos, w, h, L)] = TC_UNK;
            else if (pathx[n] == w-1) state->walls[grid_to_wall(pos, w, h, R)] = TC_UNK;
            else if (pathy[n] == 0)   state->walls[grid_to_wall(pos, w, h, U)] = TC_UNK;
            else if (pathy[n] == h-1) state->walls[grid_to_wall(pos, w, h, D)] = TC_UNK;
        }
    }
    
    sfree(pathx);
    sfree(pathy);

    return;
}
static const char *validate_desc(const game_params *params, const char *desc);

static char *new_game_desc(const game_params *params, random_state *rs,
                           char **aux, bool interactive) {
    int i,j;
    game_state *new;
    char *desc, *e;
    int erun, wrun;
    int *wallindices;
    int wallnum;
    int *gridindices;
    int gridnum;
    int *borderindices;
    int bordernum;
    int borderreduce;
    
    char *twalls;
    
    int ws = (params->w + 1) * params->h + params->w * (params->h + 1);
    int w = params->w;
    int h = params->h;
    int difficulty = params->difficulty;
    
    if (difficulty == DIFF_HARD) difficulty = DIFF_TRICKY; /* Hard not implemented yet */
    if (w == 3 && h == 3 && difficulty >= DIFF_TRICKY) difficulty = DIFF_NORMAL;
    
    wallindices = snewn(ws, int);
    gridindices = snewn(ws, int);
    borderindices = snewn(ws, int);

    twalls = snewn(ws, char);

    while(true) {
    
        new = new_state(params);
        generate_hamiltonian_path(new, rs);
    
       /*
        * Remove as many walls as possible while retaining solubility.
        */
  
        wallnum = gridnum = bordernum = 0;
        for (i=0;i<ws;i++) {         
            if (new->walls[i] == TC_DIS) {
                if (is_border_wall(i, w, h))
                    borderindices[bordernum++] = i;
                else
                    gridindices[gridnum++] = i;
            }        
        }

        shuffle(borderindices, bordernum, sizeof(int), rs);
    
        switch(params->difficulty) {
            case DIFF_EASY:   borderreduce = random_upto(rs,bordernum/4); break;
            case DIFF_NORMAL: borderreduce = random_upto(rs,bordernum/2); break;
            case DIFF_TRICKY: borderreduce = random_upto(rs,bordernum); break;
            case DIFF_HARD:   borderreduce = bordernum; break;
            default:     borderreduce = 2; break;
        }

        for (i=0;i<gridnum;i++) wallindices[wallnum++] = gridindices[i];
        for (i=0;i<borderreduce;i++) wallindices[wallnum++] = borderindices[i];

        shuffle(wallindices, wallnum, sizeof(int), rs);

        for (i=0;i<wallnum;i++) {
            int index = wallindices[i];
            for (j=0;j<ws;j++) twalls[j] = new->walls[j];
            twalls[index] = TC_UNK;
            if (walls_solve(w, h, twalls, difficulty) == SOLVED) {
                new->walls[index] = TC_UNK;
            }
        }
        for (j=0;j<ws;j++) twalls[j] = new->walls[j];
        if (walls_solve(w, h, twalls, difficulty) != SOLVED) {
            free_game(new);
            continue;
        }
        if (difficulty == DIFF_EASY) break;
        for (j=0;j<ws;j++) twalls[j] = new->walls[j];
        if (walls_solve(w, h, twalls, difficulty-1) == SOLVED) {
            free_game(new);
            continue;
        }
        break;
    }

    sfree(twalls);
    sfree(wallindices);
    sfree(gridindices);
    sfree(borderindices);
    
    /* We have a valid puzzle! */
    print_grid(w,h,new->walls);

    /* Encode walls */
    desc = snewn(ws + (w*h),char);
    e = desc;
    erun = wrun = 0;
    for(i = 0; i < ws; i++) {
        if((new->walls[i] != TC_DIS) && wrun > 0) {
            e += sprintf(e, "%d", wrun);
            wrun = erun = 0;
        }
        else if((new->walls[i] == TC_DIS) && erun > 0) {
            while (erun >= 26) {
                *e++ = 'z';
                erun -= 26;
            }
            if (erun == 0) {
                wrun = 0;
            }
            else {
                *e++ = ('a' + erun - 1);
                erun = 0; wrun = -1;
            }
        }
        if(new->walls[i] != TC_DIS) erun++;
        else                  wrun++;
    }
    if(wrun > 0) e += sprintf(e, "%d", wrun);
    if(erun > 0) *e++ = ('a' + erun - 1);
    *e++ = '\0';
    
    free_game(new);
    assert (validate_desc(params, desc) == NULL);
    
    return desc;
}

static const char *validate_desc(const game_params *params, const char *desc) {
    int wsl = 0;
    int ws = (params->w + 1) * params->h + params->w * (params->h + 1);
    
    while (*desc) {
        if(isdigit((unsigned char)*desc)) {
            wsl += atoi(desc);
            while (*desc && isdigit((unsigned char)*desc)) desc++;
        }
        else if(*desc >= 'a' && *desc <= 'z') {
            wsl += *desc - 'a' + 1 + (*desc != 'z' ? 1 : 0);
            desc++;
            if (!(*desc) && wsl == ws + 1) wsl--;
        }
        else
            return "Faulty game description";
    }
    if (wsl < ws) return "Too few walls in game description";
    if (wsl > ws) return "Too many walls in game description";

    return NULL;
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error) {
    int i;
    int w = state->shared->w;
    int h = state->shared->h;
    char *move = snewn(w*h*40, char), *p = move;

    game_state *solve_state = dup_game(state);
    walls_solve(w,h,solve_state->walls, DIFF_HARD);

    *p++ = 'S';
    for (i = 0; i < state->shared->nw; i++) {
        if (solve_state->walls[i] == TC_UNK)
            p += sprintf(p, ";C%d", i);
        if (solve_state->walls[i] == TC_DIS)
            p += sprintf(p, ";W%d", i);
        if (solve_state->walls[i] == TC_CON)
            p += sprintf(p, ";L%d", i);
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
    char *ret, *p;
    char iswall, isline, isleft, isright, isup, isdown;
    int w = state->shared->w, h = state->shared->h;
    ret = snewn((9*w*h) + (3*w) + (6*h) + 1, char);

    p = ret;

    for (y=0;y<h;y++) {
        for (x=0;x<w;x++) {
            iswall = state->walls[(w + 1)*h + y*w + x] == TC_DIS;
            isline = state->walls[(w + 1)*h + y*w + x] == TC_CON;
            *p++ = '+';
            *p++ = iswall ? '-' : ' ';
            *p++ = isline ? '*' : iswall ? '-' : ' ';
            *p++ = iswall ? '-' : ' ';
        }
        *p++ = '+'; *p++ = '\n';
        for (x=0;x<w;x++) {
            iswall = state->walls[y*(w+1) + x] == TC_DIS;
            isleft = state->walls[y*(w+1) + x] == TC_CON;
            isright = state->walls[y*(w+1) + x + 1] == TC_CON;
            isup = state->walls[(w+1)*h + y*w + x] == TC_CON;
            isdown = state->walls[(w+1)*h + w*y + x + w] == TC_CON;
            
            *p++ = isleft ? '*' : iswall ? '|' : ' ';
            *p++ = isleft ? '*' : ' ';
            *p++ = (isleft || isright || isup || isdown) ? '*' : ' ';
            *p++ = isright ? '*' : ' ';
        }
        iswall = state->walls[y*(w+1) + w] == TC_DIS;
        isright = state->walls[y*(w+1) + w] == TC_CON;
        *p++ = isright ? '*' : iswall ? '|' : ' '; 
        *p++ = '\n';        
    }
    for (x=0;x<w;x++) {
        iswall = state->walls[((w+1) * h) + (w * h) + x] == TC_DIS;
        isline = state->walls[((w+1) * h) + (w * h) + x] == TC_CON;
        *p++ = '+';
        *p++ = iswall ? '-' : ' ';
        *p++ = isline ? '*' : iswall ? '-' : ' ';
        *p++ = iswall ? '-' : ' ';
    }
    *p++ = '+'; *p++ = '\n'; *p = 0x00;
    
    printf("%s", ret);
    return ret;
}


struct game_ui {
    int *dragcoords;
    int ndragcoords;
    int cx, cy;
};

static game_ui *new_ui(const game_state *state) {
    
    game_ui *ui = snew(game_ui);
    
    ui->ndragcoords = -1;
    ui->dragcoords = snewn(state->shared->nw, int);
    ui->cx = ui->cy = -1;
    
    return ui;
}

static void free_ui(game_ui *ui) {
    sfree(ui->dragcoords);
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

#define PREFERRED_TILE_SIZE 48
#define TILESIZE (ds->tilesize)
#define BORDER (3*TILESIZE/4)
#define COORD(x) ( (x) * TILESIZE + BORDER )
#define CENTERED_COORD(x) ( COORD(x) + TILESIZE/2 )
#define FROMCOORD(x) ( ((x) < BORDER) ? -1 : ((x) - BORDER) / TILESIZE )

struct game_drawstate {
    int tilesize;
    int started;
    char *walls;
    char *errors;
};

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int x, int y, int button) {
    char buf[80];
    int type = 0;
    int w = state->shared->w;
    int h = state->shared->h;
    int fx = FROMCOORD(x);
    int fy = FROMCOORD(y);
    int lx = x - (fx * TILESIZE) - BORDER;
    int ly = y - (fy * TILESIZE) - BORDER;
    
    if      (lx < (TILESIZE/2 - abs(TILESIZE/2 - ly))) type = L;
    else if (lx > (TILESIZE/2 + abs(TILESIZE/2 - ly))) type = R;
    else if (ly < (TILESIZE/2 - abs(TILESIZE/2 - lx))) type = U;
    else if (ly > (TILESIZE/2 + abs(TILESIZE/2 - lx))) type = D;
        
    if (type == 0) return NULL;

    if (button == LEFT_BUTTON) {
        if ( (fx < 0 || fx > h) && (fy < 0 || fy > w) ) {
            ui->ndragcoords = -1;
            return NULL;
        }
        ui->cx = fx;
        ui->cy = fy;
        ui->ndragcoords = 0;
        return "";
    }
    if (button == LEFT_DRAG) {
        int p = -1;
        int g = ui->cx + ui->cy * w;
        if (fx != ui->cx && fy == ui->cy && fy >= 0 && fy < h ) {
            if (fx < 0)       p = grid_to_wall(g,w,h,L);
            else if (fx >= w) p = grid_to_wall(g,w,h,R);
            else              p = grid_to_wall(fx + fy*w,w,h,fx < ui->cx ? R : L);
        }
        if (fy != ui->cy && fx == ui->cx && fx >= 0 && fx < w) {
            if (fy < 0)       p = grid_to_wall(g,w,h,U);
            else if (fy >= h) p = grid_to_wall(g,w,h,D);
            else              p = grid_to_wall(fx + fy*w,w,h,fy < ui->cy ? D : U);
        }
        if (p != -1 && state->walls[p] != TC_DIS) {
            ui->cx = fx; ui->cy = fy;
            if (state->walls[p] == TC_UNK) sprintf(buf,"L%d",p);
            if (state->walls[p] == TC_CON) sprintf(buf,"C%d",p);
            return dupstr(buf);
        }
        return NULL;
    }
    
    if (IS_MOUSE_RELEASE(button)) {
        if (button == LEFT_RELEASE) {
            ui->ndragcoords = -1;
            return "";
        }
    }
    
    else if (button == RIGHT_BUTTON) {
        int pos;
        
        if (fx ==  w && type == L) { fx = w-1; type = R; }
        if (fx == -1 && type == R) { fx =   0; type = L; }
        if (fy ==  h && type == U) { fy = h-1; type = D; }
        if (fy == -1 && type == D) { fy =   0; type = U; }
    
        pos = grid_to_wall(fx+fy*w,w,h,type);

        if (state->shared->fixed[pos]) return NULL;
        if (state->walls[pos] == TC_CON) return NULL;
        if (state->walls[pos] == TC_UNK) sprintf(buf,"W%d",pos);
        if (state->walls[pos] == TC_DIS) sprintf(buf,"C%d",pos);
        return dupstr(buf);
    }
    
    return NULL;
}

static game_state *execute_move(const game_state *state, const char *move) {
    char c;
    int g, n;
    game_state *ret = dup_game(state);

    while (*move) {
        c = *move;
        if (c == 'S') {
            ret->used_solve = true;
            move++;
        } else if (c == 'W' || c == 'L' || c == 'C') {
            move++;
            if (sscanf(move, "%d%n", &g, &n) < 1) goto badmove;
            if (c == 'W') ret->walls[g] = TC_DIS;
            if (c == 'L') ret->walls[g] = TC_CON;
            if (c == 'C') ret->walls[g] = TC_UNK;
            move += n;
        }

        if (*move == ';')
            move++;
        else if (*move)
            goto badmove;
    }

    if (check_solution(ret->shared->w, ret->shared->h, ret->walls, ret->errors) == SOLVED)
        ret->completed = true;

    return ret;

badmove:
    printf("Bad move!\n");
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

    *x = (params->w) * TILESIZE + 2 * BORDER;
    *y = (params->h) * TILESIZE + 2 * BORDER;
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize) {
    ds->tilesize = tilesize;
}

static float *game_colours(frontend *fe, int *ncolours) {
    float *ret = snewn(3 * NCOLOURS, float);

    frontend_default_colour(fe, &ret[COL_BACKGROUND * 3]);

    ret[COL_FLOOR_A * 3 + 0] = 0.9F;
    ret[COL_FLOOR_A * 3 + 1] = 0.9F;
    ret[COL_FLOOR_A * 3 + 2] = 0.9F;

    ret[COL_FLOOR_B * 3 + 0] = 0.7F;
    ret[COL_FLOOR_B * 3 + 1] = 0.7F;
    ret[COL_FLOOR_B * 3 + 2] = 0.7F;

    ret[COL_FIXED * 3 + 0] = 0.1F;
    ret[COL_FIXED * 3 + 1] = 0.1F;
    ret[COL_FIXED * 3 + 2] = 0.1F;

    ret[COL_WALL * 3 + 0] = 0.4F;
    ret[COL_WALL * 3 + 1] = 0.4F;
    ret[COL_WALL * 3 + 2] = 0.4F;

    ret[COL_GRID * 3 + 0] = 0.0F;
    ret[COL_GRID * 3 + 1] = 0.0F;
    ret[COL_GRID * 3 + 2] = 0.0F;

    ret[COL_LINE * 3 + 0] = 0.1F;
    ret[COL_LINE * 3 + 1] = 0.1F;
    ret[COL_LINE * 3 + 2] = 0.1F;

    ret[COL_DRAGLINE * 3 + 0] = 0.0F;
    ret[COL_DRAGLINE * 3 + 1] = 0.0F;
    ret[COL_DRAGLINE * 3 + 2] = 1.0F;

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
    struct game_drawstate *ds = snew(struct game_drawstate);

    ds->tilesize = 0;
    ds->started = false;
    ds->walls = snewn(state->shared->nw, char);
    ds->errors = snewn(state->shared->nw, char);
    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds) {
    sfree(ds->errors);
    sfree(ds->walls);
    sfree(ds);
}


static void draw_horizontal_dotted_line(drawing *dr, int x1, int x2, int y, int color, int bgcolor) {
    int i;
    for (i=x1;i<x2;i+=4) { 
        draw_line(dr, i, y, i+1, y, color);
        draw_line(dr, i+2, y, i+3, y, bgcolor);
    }
}
static void draw_vertical_dotted_line(drawing *dr, int y1, int y2, int x, int color, int bgcolor) {
    int i;
    for (i=y1;i<y2;i+=4) {
        draw_line(dr, x, i, x, i+1, color);
        draw_line(dr, x, i+2, x, i+3, bgcolor);
    }
}

void draw_path(drawing *dr, game_drawstate *ds, const game_ui *ui, int i, 
               const game_state *state, bool flash) {
    int w = state->shared->w;
    int h = state->shared->h;
    int width = ds->tilesize/6;
    int cx,cy,cwx,cwy;
    if (i < (w+1)*h) {
        int x = i % (w+1);
        int y = i / (w+1);
        cx = COORD(x)-(ds->tilesize/2)-(width/2);
        cy = COORD(y)+(ds->tilesize/2)-(width/2);
        cwx = ds->tilesize+width;
        cwy = width;
        if (x == 0) { cx += width; cwx -= width; }
        if (x == w) { cwx -= width; }                        
    }
    else {
        int x = (i-(w+1)*h) % w;
        int y = (i-(w+1)*h) / w;
        cx = COORD(x)+(ds->tilesize/2)-(width/2);
        cy = COORD(y)-(ds->tilesize/2)-(width/2);
        cwx = width;
        cwy = ds->tilesize+width;
        if (y == 0) { cy += width; cwy -= width; }
        if (y == h) { cwy -= width; }
    }
    draw_rect(dr, cx, cy, cwx, cwy, state->errors[i] ? COL_ERROR : 
                                    flash            ? COL_FLASH : 
                                                       COL_DRAGLINE);
    draw_update(dr, cx, cy, cwx, cwy);

}

void draw_wall(drawing *dr, game_drawstate *ds, const game_ui *ui, int i, 
               const game_state *state) {
    int w = state->shared->w;
    int h = state->shared->h;
    int width = ds->tilesize/18;
    int dx,dy,dwx,dwy;

    if (i < (w+1)*h) {
        int x = i % (w+1);
        int y = i / (w+1);
        int vu = wall_to_vertex(i,w,h,U);
        int vd = wall_to_vertex(i,w,h,D);
        bool fu = false;
        bool fd = false;
        int w1 = vertex_to_wall(vu,w,h,L);
        int w2 = vertex_to_wall(vu,w,h,R);
        int w3 = vertex_to_wall(vu,w,h,U);
        int w4 = vertex_to_wall(vd,w,h,L);
        int w5 = vertex_to_wall(vd,w,h,R);
        int w6 = vertex_to_wall(vd,w,h,D);
        fu = ((w1 != -1 && state->shared->fixed[w1]) ||
              (w2 != -1 && state->shared->fixed[w2]) ||
              (w3 != -1 && state->shared->fixed[w3]));
        fd = ((w4 != -1 && state->shared->fixed[w4]) ||
              (w5 != -1 && state->shared->fixed[w5]) ||
              (w6 != -1 && state->shared->fixed[w6]));
        dx = COORD(x)-width;
        dy = COORD(y)-width;
        dwx = 2*width;
        dwy = ds->tilesize+2*width;
        if (fu) { dy += 2*width; dwy -= 2*width; }
        if (fd) { dwy -= 2*width; }
    }
    else {
        int x = (i-(w+1)*h) % w;
        int y = (i-(w+1)*h) / w;
        int vl = wall_to_vertex(i,w,h,L);
        int vr = wall_to_vertex(i,w,h,R);
        bool fl = false;
        bool fr = false;
        int w1 = vertex_to_wall(vl,w,h,U);
        int w2 = vertex_to_wall(vl,w,h,L);
        int w3 = vertex_to_wall(vl,w,h,D);
        int w4 = vertex_to_wall(vr,w,h,U);
        int w5 = vertex_to_wall(vr,w,h,R);
        int w6 = vertex_to_wall(vr,w,h,D);
        fl = ((w1 != -1 && state->shared->fixed[w1]) ||
              (w2 != -1 && state->shared->fixed[w2]) ||
              (w3 != -1 && state->shared->fixed[w3]));
        fr = ((w4 != -1 && state->shared->fixed[w4]) ||
              (w5 != -1 && state->shared->fixed[w5]) ||
              (w6 != -1 && state->shared->fixed[w6]));
        dx = COORD(x)-width;
        dy = COORD(y)-width;
        dwx = ds->tilesize+2*width;
        dwy = 2*width;
        if (fl) { dx += 2*width; dwx -= 2*width; }
        if (fr) { dwx -= 2*width; }
    }
    draw_rect(dr, dx,dy,dwx,dwy, COL_WALL);
    draw_update(dr, dx,dy,dwx,dwy);
}

void draw_empty_path(drawing *dr, game_drawstate *ds, const game_ui *ui, int i, 
               const game_state *state) {
    int w = state->shared->w;
    int h = state->shared->h;
    int width = ds->tilesize/6;
    
    if (i < (w+1)*h) {
        int x = i % (w+1);
        int y = i / (w+1);
        int parity = (y % 2 == 0) ? (x % 2 == 1) : (x % 2 == 0);
        bool nl = false;
        bool nr = false;
        bool el = false;
        bool er = false;
        int gl = wall_to_grid(i,w,h,L);
        int gr = wall_to_grid(i,w,h,R);
        int lx,ly,lwx,lwy,rx,ry,rwx,rwy;
        if (gl != -1) {
            if (state->walls[grid_to_wall(gl,w,h,U)] == TC_CON) nl = true;
            if (state->walls[grid_to_wall(gl,w,h,D)] == TC_CON) nl = true;
            if (state->walls[grid_to_wall(gl,w,h,L)] == TC_CON) nl = true;
            if (state->errors[grid_to_wall(gl,w,h,U)]) el = true;
            if (state->errors[grid_to_wall(gl,w,h,D)]) el = true;
            if (state->errors[grid_to_wall(gl,w,h,L)]) el = true;
        }
        if (gr != -1) {
            if (state->walls[grid_to_wall(gr,w,h,U)] == TC_CON) nr = true;
            if (state->walls[grid_to_wall(gr,w,h,D)] == TC_CON) nr = true;
            if (state->walls[grid_to_wall(gr,w,h,R)] == TC_CON) nr = true;
            if (state->errors[grid_to_wall(gr,w,h,U)]) er = true;
            if (state->errors[grid_to_wall(gr,w,h,D)]) er = true;
            if (state->errors[grid_to_wall(gr,w,h,R)]) er = true;
        }
        lx = COORD(x)-(ds->tilesize/2) + (!nl ? -(width/2) : (width/2));
        ly = COORD(y)+(ds->tilesize/2)-(width/2);
        lwx = ds->tilesize/2 + (nl ? -(width/2) : (width/2));
        lwy = width;
        rx = COORD(x);
        ry = COORD(y)+(ds->tilesize/2)-(width/2);
        rwx = ds->tilesize/2 - (nr ? (width/2) : -(width/2));
        rwy = width;
        clip(dr, lx, ly, rx-lx+rwx, width);
        draw_rect(dr, lx, ly, lwx, lwy, 
                  gl == -1 ? COL_BACKGROUND :
                  parity ? COL_FLOOR_B : COL_FLOOR_A);
        draw_rect(dr, rx, ry, rwx, rwy, 
                  gr == -1 ? COL_BACKGROUND :
                  parity ? COL_FLOOR_A : COL_FLOOR_B);
        draw_vertical_dotted_line(dr, COORD(y), COORD(y+1)-1, COORD(x), 
                                  COL_GRID, COL_BACKGROUND);
        draw_update(dr, lx, ly, rx-lx+rwx, width);
        unclip(dr);
        if (nl) {
            draw_rect(dr, COORD(x)-(ds->tilesize/2)-(width/2),
                      COORD(y)+(ds->tilesize/2)-(width/2),
                      width, width, el ? COL_ERROR : COL_DRAGLINE);
            draw_update(dr, COORD(x)-(ds->tilesize/2)-(width/2),
                      COORD(y)+(ds->tilesize/2)-(width/2),
                      width, width);
        }
        if (nr) {
            draw_rect(dr, COORD(x)+(ds->tilesize/2)-(width/2),
                      COORD(y)+(ds->tilesize/2)-(width/2),
                      width, width, er ? COL_ERROR : COL_DRAGLINE);
            draw_update(dr, COORD(x)+(ds->tilesize/2)-(width/2),
                      COORD(y)+(ds->tilesize/2)-(width/2),
                      width, width);
        }
    }
    else {
        int x = (i-(w+1)*h) % w;
        int y = (i-(w+1)*h) / w;
        int parity = (y % 2 == 0) ? (x % 2 == 1) : (x % 2 == 0);
        bool nu = false;
        bool nd = false;
        bool eu = false;
        bool ed = false;
        int gu = wall_to_grid(i,w,h,U);
        int gd = wall_to_grid(i,w,h,D);
        int ux,uy,uwx,uwy,dx,dy,dwx,dwy;
        if (gu != -1) {
            if (state->walls[grid_to_wall(gu,w,h,U)] == TC_CON) nu = true;
            if (state->walls[grid_to_wall(gu,w,h,L)] == TC_CON) nu = true;
            if (state->walls[grid_to_wall(gu,w,h,R)] == TC_CON) nu = true;
            if (state->errors[grid_to_wall(gu,w,h,U)]) eu = true;
            if (state->errors[grid_to_wall(gu,w,h,L)]) eu = true;
            if (state->errors[grid_to_wall(gu,w,h,R)]) eu = true;
        }
        if (gd != -1) {
            if (state->walls[grid_to_wall(gd,w,h,D)] == TC_CON) nd = true;
            if (state->walls[grid_to_wall(gd,w,h,L)] == TC_CON) nd = true;
            if (state->walls[grid_to_wall(gd,w,h,R)] == TC_CON) nd = true;
            if (state->errors[grid_to_wall(gd,w,h,D)]) ed = true;
            if (state->errors[grid_to_wall(gd,w,h,L)]) ed = true;
            if (state->errors[grid_to_wall(gd,w,h,R)]) ed = true;
        }
        ux = COORD(x)+(ds->tilesize/2)-(width/2);
        uy = COORD(y)-(ds->tilesize/2)+ (!nu ? -(width/2) : (width/2));
        uwx = width;
        uwy = ds->tilesize/2 + (nu ? -(width/2) : (width/2));
        dx = COORD(x)+(ds->tilesize/2)-(width/2);
        dy = COORD(y);
        dwx = width;
        dwy = ds->tilesize/2 - (nd ? (width/2) : -(width/2));
        clip(dr, ux, uy, width, dy-uy+dwy);
        draw_rect(dr, ux, uy, uwx, uwy, 
                  gu == -1 ? COL_BACKGROUND :
                  parity ? COL_FLOOR_B : COL_FLOOR_A);
        draw_rect(dr, dx, dy, dwx, dwy, 
                  gd == -1 ? COL_BACKGROUND :
                  parity ? COL_FLOOR_A : COL_FLOOR_B);
        draw_horizontal_dotted_line(dr, COORD(x), COORD(x+1), COORD(y),
                                  COL_GRID, COL_BACKGROUND);
        draw_update(dr, ux, uy, width, dy-uy+dwy);
        unclip(dr);

        if (nu) {
            draw_rect(dr, COORD(x)+(ds->tilesize/2)-(width/2),
                      COORD(y)-(ds->tilesize/2)-(width/2),
                      width, width, eu ? COL_ERROR : COL_DRAGLINE);
            draw_update(dr, COORD(x)+(ds->tilesize/2)-(width/2),
                      COORD(y)-(ds->tilesize/2)-(width/2),
                      width, width);
        }
        if (nd) {
            draw_rect(dr, COORD(x)+(ds->tilesize/2)-(width/2),
                      COORD(y)+(ds->tilesize/2)-(width/2),
                      width, width, ed ? COL_ERROR : COL_DRAGLINE);
            draw_update(dr, COORD(x)+(ds->tilesize/2)-(width/2),
                      COORD(y)+(ds->tilesize/2)-(width/2),
                      width, width);
        }
    } 
}

void draw_empty_wall(drawing *dr, game_drawstate *ds, const game_ui *ui, int i, 
               const game_state *state) {
    int w = state->shared->w;
    int h = state->shared->h;
    int width = ds->tilesize/18;
    
    if (i < (w+1)*h) {
        int x = i % (w+1);
        int y = i / (w+1);
        int parity = (y % 2 == 0) ? (x % 2 == 1) : (x % 2 == 0);
        int cx,cy,cwx,cwy;
        int vu = wall_to_vertex(i,w,h,U);
        int vd = wall_to_vertex(i,w,h,D);
        bool wu = false;
        bool wd = false;
        int w1 = vertex_to_wall(vu,w,h,L);
        int w2 = vertex_to_wall(vu,w,h,R);
        int w3 = vertex_to_wall(vu,w,h,U);
        int w4 = vertex_to_wall(vd,w,h,L);
        int w5 = vertex_to_wall(vd,w,h,R);
        int w6 = vertex_to_wall(vd,w,h,D);
        wu = ((w1 != -1 && state->walls[w1] == TC_DIS) ||
              (w2 != -1 && state->walls[w2] == TC_DIS) ||
              (w3 != -1 && state->walls[w3] == TC_DIS));
        wd = ((w4 != -1 && state->walls[w4] == TC_DIS) ||
              (w5 != -1 && state->walls[w5] == TC_DIS) ||
              (w6 != -1 && state->walls[w6] == TC_DIS));
        cx = COORD(x)-width;
        cy = COORD(y)+ (wu ? width : -width);
        cwx = 2*width;
        cwy = ds->tilesize+2*width;
        if (wu) cwy -= 2*width;
        if (wd) cwy -= 2*width; 
        clip(dr, cx, cy, cwx, cwy);
        draw_rect(dr, cx, cy, cwx/2, cwy, (x == 0) ? COL_BACKGROUND : parity ? COL_FLOOR_B : COL_FLOOR_A);
        draw_rect(dr, cx+width, cy, cwx/2, cwy, (x == w) ? COL_BACKGROUND : parity ? COL_FLOOR_A : COL_FLOOR_B);
        draw_vertical_dotted_line(dr, COORD(y), COORD(y+1), COORD(x), COL_GRID, COL_BACKGROUND);
        if (!wu) {
            if (y == 0) {
                draw_rect(dr, cx, cy, cwx, width, COL_BACKGROUND);
            }
            else {
                draw_rect(dr, cx, cy, cwx/2, width, (x == 0) ? COL_BACKGROUND : parity ? COL_FLOOR_A : COL_FLOOR_B);
                draw_rect(dr, cx+width, cy, cwx/2, width, (x == w) ? COL_BACKGROUND : parity ? COL_FLOOR_B : COL_FLOOR_A);
                draw_vertical_dotted_line(dr, COORD(y-1), COORD(y), COORD(x), COL_GRID, COL_BACKGROUND);
            }
            if (x != 0) draw_horizontal_dotted_line(dr, COORD(x-1), COORD(x), COORD(y), COL_GRID, COL_BACKGROUND);
            if (x != w) draw_horizontal_dotted_line(dr, COORD(x), COORD(x+1), COORD(y), COL_GRID, COL_BACKGROUND);
        }
        if (!wd) {
            if (y == (h-1)) {
                draw_rect(dr, cx, COORD(y+1), cwx, width, COL_BACKGROUND);
            }
            else {
                draw_rect(dr, cx, COORD(y+1), cwx/2, width, (x == 0) ? COL_BACKGROUND : parity ? COL_FLOOR_A : COL_FLOOR_B);
                draw_rect(dr, cx+width, COORD(y+1), cwx/2, width, (x == w) ? COL_BACKGROUND : parity ? COL_FLOOR_B : COL_FLOOR_A);
                draw_vertical_dotted_line(dr, COORD(y+1), COORD(y+2), COORD(x), COL_GRID, COL_BACKGROUND);
            }
            if (x != 0) draw_horizontal_dotted_line(dr, COORD(x-1), COORD(x), COORD(y+1), COL_GRID, COL_BACKGROUND);
            if (x != w) draw_horizontal_dotted_line(dr, COORD(x), COORD(x+1), COORD(y+1), COL_GRID, COL_BACKGROUND);
        }
        draw_update(dr, cx, cy, cwx, cwy);
        unclip(dr);
    }
    else {
        int x = (i-(w+1)*h) % w;
        int y = (i-(w+1)*h) / w;
        int parity = (y % 2 == 0) ? (x % 2 == 1) : (x % 2 == 0);
        int cx,cy,cwx,cwy;
        int vl = wall_to_vertex(i,w,h,L);
        int vr = wall_to_vertex(i,w,h,R);
        bool wl = false;
        bool wr = false;
        int w1 = vertex_to_wall(vl,w,h,U);
        int w2 = vertex_to_wall(vl,w,h,L);
        int w3 = vertex_to_wall(vl,w,h,D);
        int w4 = vertex_to_wall(vr,w,h,U);
        int w5 = vertex_to_wall(vr,w,h,R);
        int w6 = vertex_to_wall(vr,w,h,D);
        wl = ((w1 != -1 && state->walls[w1] == TC_DIS) ||
              (w2 != -1 && state->walls[w2] == TC_DIS) ||
              (w3 != -1 && state->walls[w3] == TC_DIS));
        wr = ((w4 != -1 && state->walls[w4] == TC_DIS) ||
              (w5 != -1 && state->walls[w5] == TC_DIS) ||
              (w6 != -1 && state->walls[w6] == TC_DIS));
        cx = COORD(x)+ (wl ? width : -width);
        cy = COORD(y)-width;
        cwx = ds->tilesize+2*width;
        cwy = 2*width;
        if (wl) cwx -= 2*width;
        if (wr) cwx -= 2*width; 
        clip(dr, cx, cy, cwx, cwy);
        draw_rect(dr, cx, cy, cwx, cwy/2, (y == 0) ? COL_BACKGROUND : parity ? COL_FLOOR_B : COL_FLOOR_A);
        draw_rect(dr, cx, cy+width, cwx, cwy/2, (y == h) ? COL_BACKGROUND : parity ? COL_FLOOR_A : COL_FLOOR_B);
        draw_horizontal_dotted_line(dr, COORD(x), COORD(x+1), COORD(y), COL_GRID, COL_BACKGROUND);
        if (!wl) {
            if (x == 0) {
                draw_rect(dr, cx, cy, width, cwy, COL_BACKGROUND);
            }
            else {
                draw_rect(dr, cx, cy, width, cwy/2, (y == 0) ? COL_BACKGROUND : parity ? COL_FLOOR_A : COL_FLOOR_B);
                draw_rect(dr, cx, cy+width, width, cwy/2, (y == h) ? COL_BACKGROUND : parity ? COL_FLOOR_B : COL_FLOOR_A);
                draw_horizontal_dotted_line(dr, COORD(x-1), COORD(x), COORD(y), COL_GRID, COL_BACKGROUND);
            }
            if (y != 0) draw_vertical_dotted_line(dr, COORD(y-1), COORD(y), COORD(x), COL_GRID, COL_BACKGROUND);
            if (y != h) draw_vertical_dotted_line(dr, COORD(y), COORD(y+1), COORD(x), COL_GRID, COL_BACKGROUND);
        }
        if (!wr) {
            if (x == (w-1)) {
                draw_rect(dr, COORD(x+1), cy, width, cwy, COL_BACKGROUND);
            }
            else {
                draw_rect(dr, COORD(x+1), cy, width, cwy/2, (y == 0) ? COL_BACKGROUND : parity ? COL_FLOOR_A : COL_FLOOR_B);
                draw_rect(dr, COORD(x+1), cy+width, width, cwy/2, (y == h) ? COL_BACKGROUND : parity ? COL_FLOOR_B : COL_FLOOR_A);
                draw_horizontal_dotted_line(dr, COORD(x+1), COORD(x+2), COORD(y), COL_GRID, COL_BACKGROUND);
            }
            if (y != 0) draw_vertical_dotted_line(dr, COORD(y-1), COORD(y), COORD(x+1), COL_GRID, COL_BACKGROUND);
            if (y != h) draw_vertical_dotted_line(dr, COORD(y), COORD(y+1), COORD(x+1), COL_GRID, COL_BACKGROUND);
        }
        draw_update(dr, cx, cy, cwx, cwy);
        unclip(dr);
    }

}

void draw_fixed_walls(drawing *dr, game_drawstate *ds, const game_state *state) {
    int w = state->shared->w;
    int h = state->shared->h;
    int i;
    int width = ds->tilesize/18;
    
    for (i=0;i<state->shared->nw;i++) {
        if (state->walls[i] == TC_DIS && state->shared->fixed[i]) {
            if (i < (w+1)*h) {
                int x = i % (w+1);
                int y = i / (w+1);
                draw_rect(dr, COORD(x)-width, COORD(y)-width, 2*width,ds->tilesize+2*width, COL_FIXED);
            }
            else {
                int x = (i-(w+1)*h) % w;
                int y = (i-(w+1)*h) / w;
                draw_rect(dr, COORD(x)-width, COORD(y)-width, ds->tilesize+2*width, 2*width, COL_FIXED);
            }
        }
    }
    draw_update(dr, 0, 0, w*TILESIZE + 2*BORDER, h*TILESIZE + 2*BORDER);
}


void draw_grid(drawing *dr, game_drawstate *ds, const game_state *state) {
    int w = state->shared->w;
    int h = state->shared->h;
    int x,y;
    
    draw_rect(dr, 0, 0, w*TILESIZE + 2*BORDER, h*TILESIZE + 2*BORDER, COL_BACKGROUND);

    for (y=0;y<h;y++)
    for (x=0;x<w;x++) {
        int parity = (y % 2 == 0) ? (x % 2 == 1) : (x % 2 == 0);

        draw_rect(dr, COORD(x), COORD(y), ds->tilesize, ds->tilesize, 
                  parity ? COL_FLOOR_A : COL_FLOOR_B);
        draw_horizontal_dotted_line(dr, COORD(x), COORD(x+1), COORD(y), 
                                    COL_GRID, COL_BACKGROUND);
        draw_vertical_dotted_line(dr, COORD(y), COORD(y+1), COORD(x), 
                                  COL_GRID, COL_BACKGROUND);
    }
    for (x=0;x<w;x++)
        draw_horizontal_dotted_line(dr, COORD(x), COORD(x+1), COORD(h), 
                                    COL_GRID, COL_BACKGROUND);
    for (y=0;y<h;y++)
        draw_vertical_dotted_line(dr, COORD(y), COORD(y+1), COORD(w), 
                                  COL_GRID, COL_BACKGROUND);
    
    draw_update(dr, 0, 0, w*TILESIZE + 2*BORDER, h*TILESIZE + 2*BORDER);
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime) {
    int i;
    bool flash = (bool)(flashtime * 5 / FLASH_TIME) % 2;
    
    if (!ds->started) {

        draw_grid(dr, ds, state);
        draw_fixed_walls(dr, ds, state);

        for (i=0;i<state->shared->nw;i++) {
            ds->walls[i] = TC_UNK;
            ds->errors[i] = state->errors[i];
        }

        ds->started = true;
    }

    for (i=0;i<state->shared->nw;i++) {
        if (state->walls[i] == TC_CON && 
             (ds->walls[i] != TC_CON || 
              flashtime > 0.0 || 
              state->errors[i] != ds->errors[i])) {
            draw_path(dr, ds, ui, i, state, flash);
        }
        if (state->walls[i] == TC_UNK && ds->walls[i] == TC_CON) {
            draw_empty_path(dr, ds, ui, i, state);
        }
        if (state->walls[i] == TC_UNK && ds->walls[i] == TC_DIS) {
            draw_empty_wall(dr, ds, ui, i, state);
        }
        if (state->walls[i] == TC_DIS && !state->shared->fixed[i] && ds->walls[i] != TC_DIS) {
            draw_wall(dr, ds, ui, i, state);
        }
    }

    for (i=0;i<state->shared->nw;i++) {
        ds->walls[i] = state->walls[i];
        ds->errors[i] = state->errors[i];
    }

    return;
}

static float game_anim_length(const game_state *oldstate,
                              const game_state *newstate, int dir, game_ui *ui) {
    return 0.0F;
}

static float game_flash_length(const game_state *oldstate,
                               const game_state *newstate, int dir, game_ui *ui) {
    if (!oldstate->completed && newstate->completed &&
        !oldstate->used_solve && !newstate->used_solve)
        return FLASH_TIME;
    else
        return 0.0F;
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
#define thegame walls
#endif

const struct game thegame = {
    "Walls", "games.walls", "walls",
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
    NULL,
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
    NULL,
    game_status,
    false, false, game_print_size, game_print,
    false,                   /* wants_statusbar */
    false, game_timing_state,
    0,                       /* flags */
};


#ifdef STANDALONE_SOLVER

#include <stdarg.h>

int main(int argc, char **argv) {
    game_params *p;
    random_state *rs;
    int c;
    rs = random_new("123456", 6);
    p = default_params();
    p->w = 6;
    p->h = 5;
    p->difficulty = DIFF_TRICKY;
    
    while (true) {
        sfree(new_game_desc(p, rs, NULL, 0));
        c = getchar();
        if (c == 'q') break;
    }
    
    random_free(rs);
    free_params(p);
    return 0;
}

#endif

#ifdef UNIT_TESTS

/* int check_solution(int w, int h, const char *walls) { */


void parse_board(int w, int h, char *walls, const char *board) {
    int x, y, c, i;
    
    c = 0; i = 0;
    for (y=0;y<h;y++) {
        i += 4*w+1;
        for (x=0;x<(w+1);x++) {
            if (board[i] == '|') walls[c++] = TC_DIS;
            else if (board[i] == ' ') walls[c++] = TC_UNK;
            else if (board[i] == '*') walls[c++] = TC_CON;
            if (x < w) i += 4;
            else i++;
        }
    }
    
    i = 0;
    for (y=0;y<(h+1);y++) {
        for (x=0;x<w;x++) {
            i += 2;
            if (board[i] == '-') walls[c++] = TC_DIS;
            else if (board[i] == ' ') walls[c++] = TC_UNK;
            else if (board[i] == '*') walls[c++] = TC_CON;
            i += 2;
        }
        if (y < h) i += 4*w+2;
    }

    assert(c == (w+1)*h+w*(h+1)); 
    return;
}

int main(int argc, char **argv) {
    game_params *p;
    random_state *rs;
    char *board1 = "+---+---+   +"
                   "|            "
                   "+   +   +   +"
                   "|         *  "
                   "+   +   + * +"
                   "|       | ***"
                   "+---+---+---+";
 
    char *board2 = "+---+---+   +"
                   "|            "
                   "+   +   +   +"
                   "|         ***"
                   "+   +   + * +"
                   "|       | ***"
                   "+---+---+---+";

    char *board3 = "+---+---+ * +"
                   "| ********* |"
                   "+ * +   +   +"
                   "| *   ***** |"
                   "+ * + * + * +"
                   "| *****   ***"
                   "+---+---+---+";

    char *board4 = "+---+---+   +"
                   "| *****      "
                   "+ * + * +   +"
                   "| *****   *  "
                   "+   +   + * +"
                   "|         ***"
                   "+---+---+---+";

    char *walls;
    rs = random_new("123456",6);
    p = default_params();
    p->w = 3;
    p->h = 3;
    
    walls = snewn(((p->w)+1)*(p->h) + (p->w)*((p->h)+1), char);
    
    parse_board(p->w,p->h,walls, board1);    
    assert(check_solution(p->w,p->h,walls,NULL) == AMBIGUOUS);
    
    parse_board(p->w,p->h,walls, board2);    
    assert(check_solution(p->w,p->h,walls,NULL) == INVALID);
    
    parse_board(p->w,p->h,walls, board3);    
    assert(check_solution(p->w,p->h,walls,NULL) == SOLVED);
    
    parse_board(p->w,p->h,walls, board4);    
    assert(check_solution(p->w,p->h,walls,NULL) == INVALID);
    
    sfree(walls);
    
    assert(is_border_wall(0, 3, 4) == true);
    assert(is_border_wall(1, 3, 4) == false);
    assert(is_border_wall(3, 3, 4) == true);
    assert(is_border_wall(12, 3, 4) == true);
    assert(is_border_wall(14, 3, 4) == false);
    assert(is_border_wall(15, 3, 4) == true);
    assert(is_border_wall(16, 3, 4) == true);
    assert(is_border_wall(17, 3, 4) == true);
    assert(is_border_wall(19, 3, 4) == false);
    assert(is_border_wall(27, 3, 4) == false);
    assert(is_border_wall(28, 3, 4) == true);
    assert(is_border_wall(30, 3, 4) == true);

    assert(vertex_to_grid(0, 3, 4, 0) == -1);
    assert(vertex_to_grid(0, 3, 4, 1) == 12);
    assert(vertex_to_grid(0, 3, 4, 2) == 18);
    assert(vertex_to_grid(0, 3, 4, 3) == 0);

    assert(vertex_to_grid(1, 3, 4, 0) == 12);
    assert(vertex_to_grid(1, 3, 4, 1) == 13);
    assert(vertex_to_grid(1, 3, 4, 2) == 0);
    assert(vertex_to_grid(1, 3, 4, 3) == 1);

    assert(vertex_to_grid(3, 3, 4, 0) == 14);
    assert(vertex_to_grid(3, 3, 4, 1) == -1);
    assert(vertex_to_grid(3, 3, 4, 2) == 2);
    assert(vertex_to_grid(3, 3, 4, 3) == 22);

    assert(vertex_to_grid(4, 3, 4, 0) == 18);
    assert(vertex_to_grid(4, 3, 4, 1) == 0);
    assert(vertex_to_grid(4, 3, 4, 2) == 19);
    assert(vertex_to_grid(4, 3, 4, 3) == 3);

    assert(vertex_to_grid(5, 3, 4, 0) == 0);
    assert(vertex_to_grid(5, 3, 4, 1) == 1);
    assert(vertex_to_grid(5, 3, 4, 2) == 3);
    assert(vertex_to_grid(5, 3, 4, 3) == 4);

    assert(vertex_to_grid(14, 3, 4, 0) == 7);
    assert(vertex_to_grid(14, 3, 4, 1) == 8);
    assert(vertex_to_grid(14, 3, 4, 2) == 10);
    assert(vertex_to_grid(14, 3, 4, 3) == 11);

    assert(vertex_to_grid(16, 3, 4, 0) == 21);
    assert(vertex_to_grid(16, 3, 4, 1) == 9);
    assert(vertex_to_grid(16, 3, 4, 2) == -1);
    assert(vertex_to_grid(16, 3, 4, 3) == 15);

    assert(vertex_to_grid(19, 3, 4, 0) == 11);
    assert(vertex_to_grid(19, 3, 4, 1) == 25);
    assert(vertex_to_grid(19, 3, 4, 2) == 17);
    assert(vertex_to_grid(19, 3, 4, 3) == -1);

    assert(vertex_to_wall(0, 3, 4, L) == -1);
    assert(vertex_to_wall(0, 3, 4, R) == 16);
    assert(vertex_to_wall(0, 3, 4, U) == -1);
    assert(vertex_to_wall(0, 3, 4, D) == 0);

    assert(vertex_to_wall(3, 3, 4, L) == 18);
    assert(vertex_to_wall(3, 3, 4, R) == -1);
    assert(vertex_to_wall(3, 3, 4, U) == -1);
    assert(vertex_to_wall(3, 3, 4, D) == 3);

    assert(vertex_to_wall(5, 3, 4, L) == 19);
    assert(vertex_to_wall(5, 3, 4, R) == 20);
    assert(vertex_to_wall(5, 3, 4, U) == 1);
    assert(vertex_to_wall(5, 3, 4, D) == 5);

    assert(vertex_to_wall(14, 3, 4, L) == 26);
    assert(vertex_to_wall(14, 3, 4, R) == 27);
    assert(vertex_to_wall(14, 3, 4, U) == 10);
    assert(vertex_to_wall(14, 3, 4, D) == 14);

    assert(vertex_to_wall(16, 3, 4, L) == -1);
    assert(vertex_to_wall(16, 3, 4, R) == 28);
    assert(vertex_to_wall(16, 3, 4, U) == 12);
    assert(vertex_to_wall(16, 3, 4, D) == -1);

    assert(vertex_to_wall(19, 3, 4, L) == 30);
    assert(vertex_to_wall(19, 3, 4, R) == -1);
    assert(vertex_to_wall(19, 3, 4, U) == 15);
    assert(vertex_to_wall(19, 3, 4, D) == -1);

    assert(grid_to_vertex(0, 3, 4, 0) == 0);
    assert(grid_to_vertex(0, 3, 4, 1) == 1);
    assert(grid_to_vertex(0, 3, 4, 2) == 4);
    assert(grid_to_vertex(0, 3, 4, 3) == 5);

    assert(grid_to_vertex(2, 3, 4, 0) == 2);
    assert(grid_to_vertex(2, 3, 4, 1) == 3);
    assert(grid_to_vertex(2, 3, 4, 2) == 6);
    assert(grid_to_vertex(2, 3, 4, 3) == 7);

    assert(grid_to_vertex(9, 3, 4, 0) == 12);
    assert(grid_to_vertex(9, 3, 4, 1) == 13);
    assert(grid_to_vertex(9, 3, 4, 2) == 16);
    assert(grid_to_vertex(9, 3, 4, 3) == 17);

    assert(grid_to_vertex(11, 3, 4, 0) == 14);
    assert(grid_to_vertex(11, 3, 4, 1) == 15);
    assert(grid_to_vertex(11, 3, 4, 2) == 18);
    assert(grid_to_vertex(11, 3, 4, 3) == 19);

    random_free(rs);
    free_params(p);
    return 0;
}

#endif
