/*
 * stellar.c: Implementation of Sternenhaufen (Sun and Moon)
 *
 * (C) 2013-2014 Steffen Bauer
 * Created for Simon Tatham's Portable Puzzle Collection
 * See LICENCE for licence details
 *
 * http://www.janko.at/Raetsel/Sternenhaufen/index.htm
 *
 * Puzzle definition: Size of the puzzle, followed by grid definition.
 * The planet illumination is encoded as LEFT/RIGHT|TOP/BOTTOM.
 * Dark parts are encoded with 'X'.
 *
 * There are 9 possible planet illumination states:
 *   XX               (Completly dark planet) 
 *   LX, RX, XT, XB   (Planet half illuminated) 
 *   LT, LB, RT, RB   (Planet 3/4 illuminated)
 *
 * The game logic prohibits 1/4 or fully illuminated planets.
 *
 * Example: Janko puzzle no. 1
 *
 *    -------------
 *   | .  .  .  .  |
 *   | .  RB .  .  |
 *   | .  .  .  LX |
 *   | .  .  XT .  |
 *    -------------
 *
 * would be encoded into: 
 *    4:eRBeLXbXTa
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "puzzles.h"

#define EMPTY_SPACE 0x00
#define CODE_PLANET 0x01
#define CODE_STAR   0x02
#define CODE_CLOUD  0x04
#define CODE_GUESS  0x08
#define CODE_LEFT   0x10
#define CODE_RIGHT  0x20
#define CODE_TOP    0x40
#define CODE_BOTTOM 0x80
#define CODE_CROSS  0x100

#define NO_ERROR     0x00
#define ERROR_STAR   0x01
#define ERROR_CLOUD  0x02
#define ERROR_LEFT   0x04
#define ERROR_RIGHT  0x08
#define ERROR_TOP    0x10
#define ERROR_BOTTOM 0x20

#define ILLUMINATION_LEFTTOP     0x01
#define ILLUMINATION_RIGHTBOTTOM 0x02
#define ILLUMINATION_DARK        0x04

#define SOLUTION_UNIQUE     0x01
#define SOLUTION_AMBIGUOUS  0x02
#define SOLUTION_IMPOSSIBLE 0x04
#define SOLUTION_UNDEFINED  0x08

#define SOLVER_DID_ONE_STEP 0x10
#define SOLVER_NO_PROGRESS  0x20

enum {
    COL_BACKGROUND,
    COL_GRID,
    COL_HIGHLIGHT,
    COL_PLANET_DARK,
    COL_PLANET_LIGHT,
    COL_STAR,
    COL_CLOUD,
    COL_ERROR,
    COL_FLASH,
    NCOLOURS
};

#define DIFFLIST(A)                             \
    A(NORMAL,Normal,n)                          \
    A(HARD,Hard,h)
#define ENUM(upper,title,lower) DIFF_ ## upper,
#define TITLE(upper,title,lower) #title,
#define ENCODE(upper,title,lower) #lower
#define CONFIG(upper,title,lower) ":" #title
enum { DIFFLIST(ENUM) DIFFCOUNT };
static char const *const stellar_diffnames[] = { DIFFLIST(TITLE) "(count)" };
static char const stellar_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCONFIG DIFFLIST(CONFIG)

struct game_params {
    int size;
    int diff;
};

#define DEFAULT_PRESET 0

static const struct game_params stellar_presets[] = { 
    {4, 0}, {5, 0}, {6, 0}, {7, 0}, {8, 0}, {9, 0} };

static game_params *default_params(void) {
    game_params *ret = snew(game_params);
    *ret = stellar_presets[DEFAULT_PRESET];
    return ret;
}

static bool game_fetch_preset(int i, char **name, game_params **params) {
    game_params *ret;
    char buf[64];

    if (i < 0 || i >= lenof(stellar_presets)) return false;

    ret = default_params();
    *ret = stellar_presets[i]; /* struct copy */
    *params = ret;

    sprintf(buf, "%d %s",stellar_presets[i].size, stellar_diffnames[stellar_presets[i].diff]);
    *name = dupstr(buf);

    return true;
}

static void free_params(game_params *params) {
    sfree(params);
}

static game_params *dup_params(const game_params *params) {
    game_params *ret = snew(game_params);
    *ret = *params;
    return ret;
}

static void decode_params(game_params *params, char const *string) {
    params->size = atoi(string);
    while (*string && isdigit((unsigned char) *string)) ++string;

    params->diff = DIFF_NORMAL;
    if (*string == 'd') {
        int i;
        string++;
        for (i = 0; i < DIFFCOUNT; i++)
            if (*string == stellar_diffchars[i])
                params->diff = i;
        if (*string) string++;
    }
    
    return;
}

static char *encode_params(const game_params *params, bool full) {
    char buf[256];
    sprintf(buf, "%d", params->size);
    if (full)
        sprintf(buf + strlen(buf), "d%c", stellar_diffchars[params->diff]);
    return dupstr(buf);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[64];

    ret = snewn(3, config_item);

    ret[0].name = "Size";
    ret[0].type = C_STRING;
    sprintf(buf, "%d", params->size);
    ret[0].u.string.sval = dupstr(buf);
    
    ret[1].name = "Difficulty";
    ret[1].type = C_CHOICES;
    ret[1].u.choices.choicenames = DIFFCONFIG;
    ret[1].u.choices.selected = params->diff;

    ret[2].name = NULL;
    ret[2].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg) {
    game_params *ret = snew(game_params);
    ret->size = atoi(cfg[0].u.string.sval);
    ret->diff = cfg[1].u.choices.selected;
    return ret;
}

static const char *validate_params(const game_params *params, bool full) {
    if (params->size < 3)          return "Grid size must be at least 3x3"; 
    if (params->diff != DIFF_NORMAL && 
        params->diff != DIFF_HARD) return "Unknown puzzle difficulty level";
    return NULL;
}

struct game_state {
    struct game_params params;
    unsigned short *grid;
    unsigned char *errors;
    bool solved;
    bool cheated;
};

static game_state *new_state(const game_params *params) {
    int i, v;
    game_state *state = snew(game_state);

    state->params.size = params->size;
    state->params.diff = params->diff;
    
    v = params->size * params->size;
    state->grid = snewn(v, unsigned short);
    state->errors = snewn(v, unsigned char);
    for (i=0;i<v;i++) {
        state->grid[i] = 0x00;
        state->errors[i] = false;
    }
    state->solved = false;
    state->cheated = false;
    return state;
}

static game_state *dup_game(const game_state *state) {
    int i, v;
    game_state *ret = snew(game_state);

    ret->params.size = state->params.size;
    ret->params.diff = state->params.diff;
    
    v = ret->params.size * ret->params.size;
    ret->grid = snewn(v, unsigned short);
    ret->errors = snewn(v, unsigned char);
    for (i=0;i<v;i++) {
        ret->grid[i] = state->grid[i];
        ret->errors[i] = state->errors[i];
    }
    ret->solved = state->solved;
    ret->cheated = state->cheated;
    return ret;
}

static void free_game(game_state *state) {   
    if (state->errors != NULL) sfree(state->errors);
    if (state->grid != NULL) sfree(state->grid);
    sfree(state);
}

/* --------------------------------------------------------------- */
/* Puzzle solver & generator */

int get_index(int size, int c, int rowcol, int i) {
    return (c==0) ? (i + size * rowcol) : 
                    (rowcol + size * i);
}

bool check_line(int star_pos, int cloud_pos, int planet_pos, int planet_illumination) {
    
    if (planet_illumination == ILLUMINATION_LEFTTOP) {
        if ((cloud_pos < star_pos || planet_pos < cloud_pos) && 
             (star_pos < planet_pos)) 
            return true;
    }
    else if (planet_illumination == ILLUMINATION_RIGHTBOTTOM) {
        if ((planet_pos < star_pos) && 
           (star_pos < cloud_pos || cloud_pos < planet_pos)) 
            return true;
    }
    else {
        if (star_pos < cloud_pos && cloud_pos < planet_pos) 
            return true;
        else if (planet_pos < cloud_pos && cloud_pos < star_pos) 
            return true;
    }
    return false;
}

int planet_position(const game_state *state, int c, int rowcol) {
    int i;
    int ret = -1;
    
    for (i=0;i<state->params.size;i++)
        if (state->grid[get_index(state->params.size, c, rowcol, i)] & CODE_PLANET)
            ret = i;
    
    return ret;
}

bool check_solution(game_state *state) {
    
    bool solved = true;
    int x, y;
    int size = state->params.size;
    for (x = 0; x < size; x++)
    for (y = 0; y < size; y++)
        state->errors[x + y*size] = NO_ERROR;
        
    for (y = 0; y < size; y++) {
        int numstar = 0;
        int numcloud = 0;
        int posstar = -1;
        int poscloud = -1;
        int posplanet = -1;
        int illumination = -1;
        
        for (x = 0; x < size; x++) {
            if (state->grid[x + y*size] == CODE_STAR) { numstar++; posstar = x; }
            if (state->grid[x + y*size] == CODE_CLOUD) { numcloud++; poscloud = x; }
            if (state->grid[x+y*size] & CODE_PLANET) {
                posplanet = x;
                if (state->grid[x + y*size] & CODE_LEFT) illumination = ILLUMINATION_LEFTTOP;
                else if (state->grid[x + y*size] & CODE_RIGHT) illumination = ILLUMINATION_RIGHTBOTTOM;
                else illumination = ILLUMINATION_DARK;
            }
        }
        if (numstar > 1)
            for (x = 0; x < size; x++)
                if (state->grid[x + y*size] == CODE_STAR)
                    state->errors[x + y*size] = ERROR_STAR;
        if (numcloud > 1)
            for (x = 0; x < size; x++)
                if (state->grid[x + y*size] == CODE_CLOUD)
                    state->errors[x + y*size] = ERROR_CLOUD;
        if (numstar != 1 || numcloud != 1) solved = false;
 
        if (numstar == 1 && posplanet > -1 && posstar == posplanet-1 && illumination != ILLUMINATION_LEFTTOP) {
            solved = false;
            state->errors[posplanet + y*size] |= ERROR_LEFT;
        }
        else if (numstar == 1 && posplanet > -1 && posstar == posplanet+1 && illumination != ILLUMINATION_RIGHTBOTTOM) {
            solved = false;
            state->errors[posplanet + y*size] |= ERROR_RIGHT;
        }
                
        if (numstar == 1 && numcloud == 1 && posstar >= 0 && poscloud >= 0 && posplanet >= 0)
            if (!check_line(posstar, poscloud, posplanet, illumination)) {
                solved = false;
                if (posstar < posplanet) state->errors[posplanet + y*size] |= ERROR_LEFT;
                if (posstar > posplanet) state->errors[posplanet + y*size] |= ERROR_RIGHT;
            }
    }
     
    for (x = 0; x < size; x++) {
        int numstar = 0;
        int numcloud = 0;
        int posstar = -1;
        int poscloud = -1;
        int posplanet = -1;
        int illumination = -1;
        for (y = 0; y < size; y++) {
            if (state->grid[x+y*size] == CODE_STAR)  { numstar++; posstar = y; }
            if (state->grid[x+y*size] == CODE_CLOUD) { numcloud++; poscloud = y; }
            if (state->grid[x+y*size] & CODE_PLANET) {
                posplanet = y;
                if (state->grid[x + y*size] & CODE_TOP) illumination = ILLUMINATION_LEFTTOP;
                else if (state->grid[x + y*size] & CODE_BOTTOM) illumination = ILLUMINATION_RIGHTBOTTOM;
                else illumination = ILLUMINATION_DARK;
            }
        }
        if (numstar > 1)
            for (y = 0; y < size; y++)
                if (state->grid[x + y*size] == CODE_STAR)
                    state->errors[x + y*size] = ERROR_STAR;
        if (numcloud > 1)
            for (y = 0; y < size; y++)
                if (state->grid[x + y*size] == CODE_CLOUD)
                    state->errors[x + y*size] = ERROR_CLOUD;
        if (numstar != 1 || numcloud != 1) solved = false;
        
        if (numstar == 1 && posplanet > -1 && 
            posstar == posplanet-1 && 
            illumination != ILLUMINATION_LEFTTOP) {
            solved = false;
            state->errors[x + posplanet*size] |= ERROR_TOP;
        }
        else if (numstar == 1 && posplanet > -1 && 
                 posstar == posplanet+1 && 
                 illumination != ILLUMINATION_RIGHTBOTTOM) {
            solved = false;
            state->errors[x + posplanet*size] |= ERROR_BOTTOM;
        }
       
        if (numstar == 1 && numcloud == 1 && posstar >= 0 && 
            poscloud >= 0 && posplanet >= 0)
            if (!check_line(posstar, poscloud, posplanet, illumination)) {
                solved = false;
                if (posstar < posplanet) state->errors[x + posplanet*size] |= ERROR_TOP;
                if (posstar > posplanet) state->errors[x + posplanet*size] |= ERROR_BOTTOM;
            }

    }
        
    return solved;
}

void cleanup_grid(game_state *state) {
    int c, rowcol, size, i, j;
    size = state->params.size;
 
    for (c=0;c<2;c++) {
        for (rowcol=0; rowcol<size; rowcol++) {
            for (i = 0; i<size; i++) {
                if (state->grid[get_index(size, c, rowcol, i)] == CODE_STAR) {
                    for (j = 0; j<size; j++) {
                        if (state->grid[get_index(size, (c==0) ? 1:0, i, j)] & CODE_GUESS)
                        if (state->grid[get_index(size, (c==0) ? 1:0, i, j)] & CODE_STAR) {
                            state->grid[get_index(size, (c==0) ? 1:0, i, j)] ^= CODE_STAR; 
                        }
                    } 
                }
                if (state->grid[get_index(size, c, rowcol, i)] == CODE_CLOUD) {
                    for (j = 0; j<size; j++) {
                        if (state->grid[get_index(size, (c==0) ? 1:0, i, j)] & CODE_GUESS)
                        if (state->grid[get_index(size, (c==0) ? 1:0, i, j)] & CODE_CLOUD) {
                            state->grid[get_index(size, (c==0) ? 1:0, i, j)] ^= CODE_CLOUD; 
                        }
                    } 
                }                
            }
        }
    }
  
    for (i=0;i<size*size;i++)
        if (state->grid[i] == CODE_GUESS) state->grid[i] = EMPTY_SPACE;
    
    return;
}

bool next_guess(struct game_state *game, int *pos, int idx) {
    int i, j;
    int size = game->params.size;
    bool finished;

    if (idx >= 2*size) return false;
    
    finished = true;
    for (i=0;i<2*size;i++)
        if (pos[i] >= 0 && pos[i] < (size-1)) finished = false;
       
    if (finished) return false;    /* No more positions to check */
   
    if (pos[idx] == (size-1)) {                 
        pos[idx] = 0;
        return next_guess(game, pos, idx+1);
    }
    if (pos[idx] < 0)
        return next_guess(game, pos, idx+1);
        
    if (pos[idx] >= 0) pos[idx]++;
   
    for (i=0;i<2*size;i++)
        if (pos[i] >= 0 && !(game->grid[pos[i] + (i/2)*size] & CODE_GUESS)) return next_guess(game, pos, 0);    
   
    for (i=0;i<2*size;i+=2) {
        if (pos[i] >= 0 && !(game->grid[pos[i] + (i/2)*size] & CODE_STAR)) return next_guess(game, pos, 0); 
        if (pos[i] == pos[i+1]) return next_guess(game, pos, 0); 
        for (j=i;j<2*size;j+=2)
            if (pos[j] >= 0 && (pos[2*pos[j]] < 0 || (i != j && (pos[i] == pos[j])))) return next_guess(game, pos, 0);
    }
    for (i=1;i<2*size;i+=2) {    
        if (pos[i] >= 0 && !(game->grid[pos[i] + ((i-1)/2)*size] & CODE_CLOUD)) return next_guess(game, pos, 0); 
        if (pos[i] == pos[i-1]) return next_guess(game, pos, 0); 
        for (j=i;j<2*size;j+=2)
            if (pos[j] >= 0 && (pos[2*pos[j]+1] < 0 || (i != j && pos[i] == pos[j]))) return next_guess(game, pos, 0);
    }
    
    for (i=0;i<size;i++) {
        if (pos[2*i] >= 0)   game->grid[pos[2*i] +   i*size] = CODE_STAR;
        if (pos[2*i+1] >= 0) game->grid[pos[2*i+1] + i*size] = CODE_CLOUD;
    }
        
    return true;
}

void initialize_solver(game_state *state) {
    int i, s;
    s = state->params.size * state->params.size;
    for (i = 0; i < s; i++)
        if (!(state->grid[i] & CODE_PLANET))
            state->grid[i] = CODE_GUESS | CODE_STAR | CODE_CLOUD; 
    
    return;
}

unsigned char solver_combinations(game_state *state) {
    int done_something;
    int i, idx, size, c, rowcol;
    unsigned short *newrowcol;

    size = state->params.size;  
    newrowcol = snewn(size, unsigned short);
    done_something = SOLVER_NO_PROGRESS;
    
    for (c=0;c<2;c++)
    for (rowcol=0; rowcol<size; rowcol++) {
        int ts, tc, illum;
        int p = planet_position(state, c, rowcol);
        for (i=0;i<size;i++) newrowcol[i] = 0x00;
        if ( p < 0 ) continue;
        for (ts = 0; ts < size; ts++) {
            if (ts == p) continue;
            if (!(state->grid[get_index(size, c, rowcol, ts)] & CODE_STAR)) continue;
            for (tc = 0; tc < size; tc++) {
                if (tc == p) continue;
                if (!(state->grid[get_index(size, c, rowcol, tc)] & CODE_CLOUD)) continue;
                idx = get_index(size, c, rowcol, p);
                if      (state->grid[idx] & ((c==0) ? CODE_LEFT : CODE_TOP)) illum = ILLUMINATION_LEFTTOP;
                else if (state->grid[idx] & ((c==0) ? CODE_RIGHT : CODE_BOTTOM)) illum = ILLUMINATION_RIGHTBOTTOM;
                else     illum = ILLUMINATION_DARK;
                
                if (check_line(ts, tc, p, illum)) {
                    newrowcol[ts] |= CODE_STAR;
                    newrowcol[tc] |= CODE_CLOUD;
                }
            }
        }
        for (i=0;i<size;i++) {
            if (i == p) continue;
            idx = get_index(size, c, rowcol, i);
            if (!(newrowcol[i] & CODE_STAR) && (state->grid[idx] & CODE_STAR)) {
                done_something = SOLVER_DID_ONE_STEP;
                state->grid[idx] ^= CODE_STAR;
            }
            if (!(newrowcol[i] & CODE_CLOUD) && (state->grid[idx] & CODE_CLOUD)) {
                done_something = SOLVER_DID_ONE_STEP;
                state->grid[idx] ^= CODE_CLOUD;
            }
        }
    }
    sfree(newrowcol);
    return done_something;
}

unsigned char solver_singles(game_state *state) {
    int done_something;
    int i, size, c, rowcol;

    size = state->params.size;  
    done_something = SOLVER_NO_PROGRESS;
 
    for (c=0;c<2;c++) {
        for (rowcol=0; rowcol<size; rowcol++) {
            int sc, sp;
            int cc, cp;
            sc = 0; sp = -1;
            cc = 0; cp = -1;
            for (i = 0; i<size; i++) {
                if (state->grid[get_index(size, c, rowcol, i)] & CODE_STAR) {
                    sp = i; sc++;
                }
                if (state->grid[get_index(size, c, rowcol, i)] & CODE_CLOUD) {
                    cp = i; cc++;
                }

            }
            if (sc == 0) {
                return SOLUTION_IMPOSSIBLE;
            }
            if (sc == 1 && state->grid[get_index(size, c, rowcol, sp)] != CODE_STAR) {
                state->grid[get_index(size, c, rowcol, sp)] = CODE_STAR;
                done_something = SOLVER_DID_ONE_STEP;
                for (i = 0; i<size; i++) {
                    if (state->grid[get_index(size, (c==0) ? 1:0, sp, i)] & CODE_GUESS)
                    if (state->grid[get_index(size, (c==0) ? 1:0, sp, i)] & CODE_STAR) {
                        state->grid[get_index(size, (c==0) ? 1:0, sp, i)] ^= CODE_STAR; 
                    }
                } 
            }
            if (cc == 0) {
                return SOLUTION_IMPOSSIBLE;
            }
            if (cc == 1 && state->grid[get_index(size, c, rowcol, cp)] != CODE_CLOUD) {
                state->grid[get_index(size, c, rowcol, cp)] = CODE_CLOUD;
                done_something = SOLVER_DID_ONE_STEP;
                for (i = 0; i<size; i++) {
                    if (state->grid[get_index(size, (c==0) ? 1:0, cp, i)] & CODE_GUESS)
                    if (state->grid[get_index(size, (c==0) ? 1:0, cp, i)] & CODE_CLOUD) {
                        state->grid[get_index(size, (c==0) ? 1:0, cp, i)] ^= CODE_CLOUD; 
                    }
                } 
            }
        }
    }
  
    return done_something;
}

unsigned char solve_bruteforce(game_state *state) {
    int *positions;
    struct game_state *test_game;
    int i, j, size;
    bool bp;
    bool sol;
    bool first_solution = false;
    
    size = state->params.size;
    
    positions = snewn(2*size, int);
    for (i=0;i<2*size;i++) {
        bp = false;
        for (j=0;j<size;j++) {
            if (state->grid[j + (i/2)*size] == ((i%2 == 0) ? CODE_STAR : CODE_CLOUD)) {
                bp = true;
                break;
            }
        }
        positions[i] = bp ? -1 : 0;
    }
    
    test_game = dup_game(state);
    while (true) {
        for (i = 0;i<size*size;i++) test_game->grid[i] = state->grid[i];
        if (!next_guess(test_game, positions, 0)) break;
        sol = check_solution(test_game);
        printf("Testing ");
        for (i=0;i<2*size;i++) printf("%i ", positions[i]);
        printf("Result %i \n", sol);
        if (first_solution && sol) {
            free_game(test_game);
            sfree(positions);
            return SOLUTION_AMBIGUOUS;
        }
        if (sol) first_solution = true;
        
    }
    free_game(test_game);
    sfree(positions);
    return first_solution ? SOLUTION_UNIQUE : SOLUTION_IMPOSSIBLE;
}

unsigned char solve_sequential(game_state *state) {
    unsigned char done_something;
    while(true) {
        
        done_something = solver_combinations(state);        
        if (done_something == SOLVER_DID_ONE_STEP) continue;
        
        done_something = solver_singles(state);
        if (done_something == SOLVER_DID_ONE_STEP) continue;
        if (done_something == SOLUTION_IMPOSSIBLE) return SOLUTION_IMPOSSIBLE;
        
        break;
    }  
    cleanup_grid(state);
    if (check_solution(state)) return SOLUTION_UNIQUE;
    return SOLUTION_UNDEFINED;
}

unsigned char solve_recursive(game_state *state, int depth) {
    int t, i, j, open, size;
    game_state *test_state;
    unsigned short *sol_grid;
    bool first_solution;
    unsigned char sol;
      
    size = state->params.size;
    sol_grid = snewn(size*size, unsigned short);
    first_solution = false;
    
    open = 0;
    for (i=0;i<size*size;i++) 
    for (t=0;t<2;t++) {
        if (state->grid[i] & CODE_GUESS)
        if (state->grid[i] & (t == 0 ? CODE_STAR : CODE_CLOUD) ) {
            open++;
            test_state = dup_game(state);
            test_state->grid[i] = (t == 0 ? CODE_STAR : CODE_CLOUD);
            sol = solve_sequential(test_state);
            if (sol == SOLUTION_IMPOSSIBLE) {
                state->grid[i] ^= (t == 0 ? CODE_STAR : CODE_CLOUD);
                if (solve_sequential(state) == SOLUTION_IMPOSSIBLE) {
                    free_game(test_state);
                    sfree(sol_grid);
                    return SOLUTION_IMPOSSIBLE;
                }
            }
      
            if (sol == SOLUTION_UNDEFINED) {
                sol = solve_recursive(test_state, depth+1);
                if (sol == SOLUTION_AMBIGUOUS) {
                    state->grid[i] ^= (t == 0 ? CODE_STAR : CODE_CLOUD);
                    solve_sequential(state);
                    free_game(test_state);
                    sfree(sol_grid);
                    return SOLUTION_AMBIGUOUS;

                }
            }
            
            if (!first_solution && sol == SOLUTION_UNIQUE) {
                first_solution = true;
                for (j=0;j<size*size;j++)
                    sol_grid[j] = test_state->grid[j];
            }
            else if (first_solution && sol == SOLUTION_UNIQUE) {
                for (j=0; j<size*size;j++) {
                    if (test_state->grid[j] != sol_grid[j]) {
                        free_game(test_state);
                        sfree(sol_grid);
                        printf("Ambiguous solutions at depth %i\n", depth);
                        return SOLUTION_AMBIGUOUS;
                    }
                }
            }
            free_game(test_state);
        }
    }
    if (first_solution) {
        for (i=0;i<size*size;i++)
            state->grid[i] = sol_grid[i];
        sfree(sol_grid);  
        printf("Found a unique solution at depth %i\n", depth);
        return SOLUTION_UNIQUE;
    }
    
    sfree(sol_grid);
    return SOLUTION_IMPOSSIBLE;
}

unsigned char solve_stellar(game_state *state, int difficulty) {
    unsigned char sol;
    
    initialize_solver(state);
    sol = solve_sequential(state);
    if (sol == SOLUTION_UNIQUE) return SOLUTION_UNIQUE;
    if (sol == SOLUTION_IMPOSSIBLE) return SOLUTION_IMPOSSIBLE;

    if (difficulty == DIFF_HARD) return solve_recursive(state, 0);
    
    return SOLUTION_IMPOSSIBLE;
}

static char *new_game_desc(const game_params *params, random_state *rs,
               char **aux, bool interactive)
{
    game_state *new;
    int *pos;
    int i, index, r, count;
    char *e;
    char *desc; 
    unsigned short c;
    
    pos = snewn(params->size, int);    
    new = new_state(params);
    count = 0;
    
    while (true) {
        for (i=0;i<params->size;i++) pos[i] = i;
        shuffle(pos, params->size, sizeof(int), rs);
        for (i=0;i<(params->size)*(params->size);i++)
            new->grid[i] = EMPTY_SPACE;
        for (i=0;i<params->size;i++) {
            index = i + (params->size)*pos[i];
            r = random_upto(rs, 9);
            if      (r == 0) new->grid[index] = CODE_PLANET;

            else if (r == 1) new->grid[index] = CODE_PLANET | CODE_LEFT;
            else if (r == 2) new->grid[index] = CODE_PLANET | CODE_RIGHT;
            else if (r == 3) new->grid[index] = CODE_PLANET | CODE_TOP;
            else if (r == 4) new->grid[index] = CODE_PLANET | CODE_BOTTOM;

            else if (r == 5) new->grid[index] = CODE_PLANET | CODE_LEFT | CODE_TOP;
            else if (r == 6) new->grid[index] = CODE_PLANET | CODE_LEFT | CODE_BOTTOM;
            else if (r == 7) new->grid[index] = CODE_PLANET | CODE_RIGHT | CODE_TOP;
            else if (r == 8) new->grid[index] = CODE_PLANET | CODE_RIGHT | CODE_BOTTOM;                                    
        }
        if (solve_stellar(new, DIFF_NORMAL) == SOLUTION_UNIQUE) break;
        count++;
        
    }
    printf("Valid puzzle found after %i iterations: ", count);

    for (i=0;i<params->size;i++) {
        int saved_planet;
        index = i + (params->size)*pos[i];
        saved_planet = new->grid[index];
        new->grid[index] = EMPTY_SPACE;
        
        if (solve_stellar(new, DIFF_NORMAL) != SOLUTION_UNIQUE) {
            new->grid[index] = saved_planet;
        }
    }

    if (solve_stellar(new, DIFF_NORMAL) != SOLUTION_UNIQUE) {
        if (solve_stellar(new, DIFF_HARD) == SOLUTION_UNIQUE)
            printf("Found a recursive only puzzle\n");
    }

    desc = snewn((params->size)*(params->size) + params->size + 1 , char);
    e = desc;
    
    count = 0;
    for (i=0;i<(params->size) * (params->size);i++) {
        c = new->grid[i];
        if (count > 25) {
            *e++ = 'z';
            count -= 26;
        }
        
        if (c & CODE_PLANET) {
            if (count > 0) *e++ = (count-1 + 'a');

            if (c & CODE_LEFT) *e++ = 'L';
            else if (c & CODE_RIGHT) *e++ = 'R';
            else *e++ = 'X';
            
            if (c & CODE_TOP) *e++ = 'T';
            else if (c & CODE_BOTTOM) *e++ = 'B';
            else *e++ = 'X';
            
            count = 0;
        }
        else {
            count++;  
        }
        
    }
    if (count > 0) *e++ = (count-1 + 'a');

    *e++ = '\0';
    desc = sresize(desc, e - desc, char);

    printf("%s\n", desc);

    free_game(new);    
    sfree(pos);
    return desc;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    int squares = 0;
    while (*desc) {
        int n = *desc++;
        if (n >= 'a' && n <= 'z') {
            squares += n - 'a' + 1;
        } 
        else if (n == 'L' || n == 'R' || n == 'X' ) {
            int n2 = *desc++;
            if (n2 == 'T' || n2 == 'B' || n2 == 'X' ) squares++;
            else return "Invalid character in game description!";
        } 
        else return "Invalid character in game description!";
    }
    if (squares != params->size * params->size)
        return "Game description does not match grid size!";
    
    if (*desc) return "Unexpected additional data at end of game description";
    return NULL;
}

static game_state *new_game(midend *me, const game_params *params, const char *desc)
{
    int squares;
    game_state *state;
    squares = 0;
    
    state = new_state(params);
    while (*desc) {
        int n = *desc++;
        if (n >= 'a' && n <= 'z') {
            squares += n - 'a' + 1;
        } else if (n == 'L' || n == 'R' || n == 'X' ) {
            int n2 = *desc++;
            if (n2 == 'T' || n2 == 'B' || n2 == 'X' ) {
                state->grid[squares] = CODE_PLANET;
                if (n  == 'L') state->grid[squares] |= CODE_LEFT;
                if (n  == 'R') state->grid[squares] |= CODE_RIGHT;
                if (n2 == 'T') state->grid[squares] |= CODE_TOP;
                if (n2 == 'B') state->grid[squares] |= CODE_BOTTOM;
                squares++;
            }
            else {
                assert(!"Invalid character in game description!");
                return NULL;
            }
        } 
        else {
            assert(!"Invalid character in game description!");
            return NULL;
        }
    }
    if (squares != params->size * params->size) {
        assert(!"Game description does not match grid size!");
        return NULL;
    }

    return state;
}

static char *solve_game(const game_state *state, const game_state *currstate,
            const char *aux, const char **error)
{
    int i, g;
    char *move, *c;
    game_state *solve_state = dup_game(currstate);
    
    solve_stellar(solve_state, DIFF_HARD);

    g = solve_state->params.size * solve_state->params.size;
    move = snewn(g * 16 +2, char);
    c = move;
    *c++='R';
    for (i = 0; i < g; i++) {
        if (!(solve_state->grid[i] & CODE_PLANET)) c += sprintf(c, ";E%d",i); 
        if (solve_state->grid[i] == CODE_STAR) c += sprintf(c, ";S%d", i);
        else if (solve_state->grid[i] == CODE_CLOUD) c += sprintf(c, ";C%d", i);
        else if (solve_state->grid[i] & CODE_GUESS) {
            if (solve_state->grid[i] & CODE_STAR) c += sprintf(c, ";s%d", i);
            if (solve_state->grid[i] & CODE_CLOUD) c += sprintf(c, ";c%d", i);
        }
    }
    *c++ = '\0';
    move = sresize(move, c - move, char);

    free_game(solve_state); 
    return move; 
}

static bool game_can_format_as_text_now(const game_params *params)
{
    return true;
}

static char *game_text_format(const game_state *state)
{
    return NULL;
}

struct game_ui {
    int hx, hy;                         /* as for solo.c, highlight pos */
    int hshow, hpencil, hcursor;        /* show state, type, and ?cursor. */
};

static game_ui *new_ui(const game_state *state) {
    game_ui *ui = snew(game_ui);
    ui->hx = ui->hy = -1;
    ui->hpencil = ui->hshow = ui->hcursor = 0;
    return ui;
}

static void free_ui(game_ui *ui) {    
    sfree(ui);
    return;
}

static char *encode_ui(const game_ui *ui) {
    return NULL;
}

static void decode_ui(game_ui *ui, const char *encoding) {
    return;
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate) {
}

#define TILE_SIZE 64
#define BORDER (TILE_SIZE/2)

struct game_drawstate {
    int tilesize;
    bool started, solved;
    int size;

    unsigned short *grid;
    unsigned char *grid_errors;
    
    int hx, hy; 
    bool hshow, hpencil; /* as for game_ui. */
    bool hflash;
 };

static char *interpret_move(const game_state *state, game_ui *ui, 
                            const game_drawstate *ds,
                            int x, int y, int button)
{
    int gx, gy, g;
    char buf[80]; 

    gx = ((x-BORDER) / ds->tilesize );
    gy = ((y-BORDER) / ds->tilesize );
    g = gx + gy * ds->size;
    
    if (((x-BORDER) < 0) || (x-BORDER) > (ds->tilesize * (ds->size)) || 
        ((y-BORDER) < 0) || (y-BORDER) > (ds->tilesize * (ds->size)) )
        g = -1;
            
    if (ui->hshow == 1) {
        int hg = ui->hx + (ui->hy) * ds->size;
        if (hg >= 0 && hg < (ds->size * ds->size) && 
            !(state->grid[hg] & CODE_PLANET) ) {
            if (button == 's' || button == 'S' || button == '1') {
                sprintf(buf, (ui->hpencil == 0) ? "S%d" : "s%d" , hg);
                if (!ui->hcursor) ui->hpencil = ui->hshow = 0;
                return dupstr(buf);
            }
            if (button == 'c' || button == 'C' || button == '2') {
                sprintf(buf, (ui->hpencil == 0) ? "C%d" : "c%d" , hg);
                if (!ui->hcursor) ui->hpencil = ui->hshow = 0;
                return dupstr(buf);
            }
            if (button == 'x' || button == 'X' || button == '3' ||
                button == '-' || button == '_') {
                sprintf(buf, (ui->hpencil == 0) ? "X%d" : "x%d" , hg);
                if (!ui->hcursor) ui->hpencil = ui->hshow = 0;
                return dupstr(buf);
            }
            if (button == 'e' || button == 'E' || button == CURSOR_SELECT2 ||
                button == '0' || button == '\b' ) {
                sprintf(buf, "E%d", hg);
                if (!ui->hcursor) ui->hpencil = ui->hshow = 0;
                return dupstr(buf);
            }
        }       
    }

    if (IS_CURSOR_MOVE(button)) {
        if (ui->hx == -1 || ui->hy == -1) {
            ui->hx = 0;
            ui->hy = 0;
        }
        else switch (button) {
              case CURSOR_UP:     ui->hy -= (ui->hy > 0)     ? 1 : 0; break;
              case CURSOR_DOWN:   ui->hy += (ui->hy < ds->size-1) ? 1 : 0; break;
              case CURSOR_RIGHT:  ui->hx += (ui->hx < ds->size-1) ? 1 : 0; break;
              case CURSOR_LEFT:   ui->hx -= (ui->hx > 0)     ? 1 : 0; break;
            }
        ui->hshow = ui->hcursor = 1;
        return UI_UPDATE;
    }
    if (ui->hshow && button == CURSOR_SELECT) {
        ui->hpencil = 1 - ui->hpencil;
        ui->hcursor = 1;
        return UI_UPDATE;
    }

    if (g >= 0 && !(state->grid[g] & CODE_PLANET)) {
        if (ui->hshow == 0) {
            if (button == LEFT_BUTTON) {
                ui->hshow = 1; ui->hpencil = 0; ui->hcursor = 0;
                ui->hx = gx; ui->hy = gy;
                return UI_UPDATE;
            }
            else if (button == RIGHT_BUTTON) {
                ui->hshow = 1; ui->hpencil = 1; ui->hcursor = 0;
                ui->hx = gx; ui->hy = gy;
                return UI_UPDATE;
            }
        }

        else if (ui->hshow == 1) {
            if (button == LEFT_BUTTON) {
                if (ui->hpencil == 0) {
                    if (gx == ui->hx && gy == ui->hy) {
                        ui->hshow = 0; ui->hpencil = 0; ui->hcursor = 0;
                        ui->hx = 0; ui->hy = 0;
                        return UI_UPDATE;
                    }
                    else {
                        ui->hshow = 1; ui->hpencil = 0; ui->hcursor = 0;
                        ui->hx = gx; ui->hy = gy;
                        return UI_UPDATE;
                    }
                }
                else {
                    ui->hshow = 1; ui->hpencil = 0; ui->hcursor = 0;
                    ui->hx = gx; ui->hy = gy;
                    return UI_UPDATE;
                }
            }
            else if (button == RIGHT_BUTTON) {
                if (ui->hpencil == 0) {
                    ui->hshow = 1; ui->hpencil = 1; ui->hcursor = 0;
                    ui->hx = gx; ui->hy = gy;
                    return UI_UPDATE;
                }
                else {
                    if (gx == ui->hx && gy == ui->hy) {
                        ui->hshow = 0; ui->hpencil = 0; ui->hcursor = 0;
                        ui->hx = 0; ui->hy = 0;
                        return UI_UPDATE;
                    }
                }
            }
        }
    }
 
    return NULL;
}

static game_state *execute_move(const game_state *state, 
                                const char *move)
{
    int x, n;
    char c;
    bool correct; 
    bool solver; 

    game_state *ret = dup_game(state);
    solver = false;

    while (*move) {
        c = *move;
        if (c == 'R') {
            move++;
            solver = true;
        }
        if (c == 'S' || c == 'C' || c == 'E' ||
            c == 'X' || c == 'x' ||
            c == 's' || c == 'c') {
            move++;
            sscanf(move, "%d%n", &x, &n);
            if (c == 'S') ret->grid[x] = CODE_STAR;
            if (c == 'C') ret->grid[x] = CODE_CLOUD;
            if (c == 'X') ret->grid[x] = CODE_CROSS;
            if (c == 'E') ret->grid[x] = 0x00;
            if (c == 's' || c == 'c' || c == 'x') { 
                ret->grid[x] ^= (c == 's' ? CODE_STAR : c == 'c' ? CODE_CLOUD : CODE_CROSS); 
                if ((ret->grid[x] & CODE_STAR) || 
                    (ret->grid[x] & CODE_CLOUD) ||
                    (ret->grid[x] & CODE_CROSS))
                    ret->grid[x] |= CODE_GUESS;
                else
                    ret->grid[x] &= !CODE_GUESS;
            }
            move += n;
        }
        if (*move == ';') move++;
    }

    correct = check_solution(ret);
    if (correct && !solver) ret->solved = true;
    if (solver) ret->cheated = true;
    
    return ret;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

static void game_compute_size(const game_params *params, int tilesize,
                              int *x, int *y)
{
    *x = *y =  2*BORDER + (params->size)*tilesize;           /* FIXME */
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
}


static float *game_colours(frontend *fe, int *ncolours)
{
    float *ret = snewn(3 * NCOLOURS, float);
    frontend_default_colour(fe, &ret[COL_BACKGROUND * 3]);

#define COLOUR(ret, i, r, g, b) ((ret[3*(i)+0] = (r)), \
                                (ret[3*(i)+1] = (g)), \
                                (ret[3*(i)+2] = (b)))

    COLOUR(ret, COL_GRID,         0.0F, 0.0F, 0.0F);
 
    ret[COL_HIGHLIGHT * 3 + 0] = 0.78F * ret[COL_BACKGROUND * 3 + 0];
    ret[COL_HIGHLIGHT * 3 + 1] = 0.78F * ret[COL_BACKGROUND * 3 + 1];
    ret[COL_HIGHLIGHT * 3 + 2] = 0.78F * ret[COL_BACKGROUND * 3 + 2];

    COLOUR(ret, COL_PLANET_DARK,  0.0F, 0.0F, 0.0F);
    COLOUR(ret, COL_PLANET_LIGHT, 1.0F, 1.0F, 0.0F);
    COLOUR(ret, COL_STAR,         1.0F, 1.0F, 0.0F);
    COLOUR(ret, COL_CLOUD,        0.5F, 0.0F, 0.5F);
    COLOUR(ret, COL_ERROR,        1.0F, 0.0F, 0.0F);
    COLOUR(ret, COL_FLASH,        1.0F, 1.0F, 1.0F);

#undef COLOUR

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    int i;
    struct game_drawstate *ds = snew(struct game_drawstate);

    ds->tilesize = 0;
    ds->started = ds->solved = false;
    ds->size = state->params.size;

    ds->grid = snewn((ds->size * ds->size), unsigned short);
    ds->grid_errors = snewn((ds->size * ds->size), unsigned char);

    for (i=0;i<(ds->size * ds->size); i++) {
        ds->grid[i] = ds->grid_errors[i] = 0x00;
    }

    ds->hshow = ds->hpencil = ds->hflash = false;
    ds->hx = ds->hy = 0;
   
    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->grid_errors);
    sfree(ds->grid);
    sfree(ds);
    return;
}

static void draw_cell_background(drawing *dr, game_drawstate *ds,
                                 const game_state *state, const game_ui *ui,
                                 int x, int y) {
    int dx,dy;
    int t = ds->tilesize;
    int hon = (ui->hshow && x == ui->hx && y == ui->hy);
    
    dx = BORDER+(x*t)+(t/2);
    dy = BORDER+(y*t)+(t/2);
  
    draw_rect(dr, dx - (t/2), dy - (t/2), t-1, t-1, 
             (hon && !ui->hpencil) ? COL_HIGHLIGHT : COL_BACKGROUND);        

    if (hon && ui->hpencil) {
        int coords[6];
        coords[0] = dx-(t/2);
        coords[1] = dy-(t/2);
        coords[2] = coords[0] + t/2;
        coords[3] = coords[1];
        coords[4] = coords[0];
        coords[5] = coords[1] + t/2;
        draw_polygon(dr, coords, 3, COL_HIGHLIGHT, COL_HIGHLIGHT);
    }

    return;
}

static void draw_star_template(drawing *dr, game_drawstate *ds,
                               int x, int y, int size, 
                               unsigned char err, bool hflash) {

    int poly[20];
    float c1,c2,s1,s2;
    int black = (hflash ? COL_FLASH : COL_GRID);

    c1 = (sqrt(5.0) - 1.0) / 4.0;
    c2 = (sqrt(5.0) + 1.0) / 4.0;
    s1 = sqrt(10 + 2.0*sqrt(5.0)) / 4.0;
    s2 = sqrt(10 - 2.0*sqrt(5.0)) / 4.0;
    
    poly[0] = x; 
    poly[1] = y - size/3;
    
    poly[2] = x + ((float)size * s2 / 6.0);
    poly[3] = y - ((float)size * c2 / 6.0);
    
    poly[4] = x + ((float)size * s1 / 3.0);
    poly[5] = y - ((float)size * c1 / 3.0);

    poly[6] = x + ((float)size * s1 / 6.0);
    poly[7] = y + ((float)size * c1 / 6.0);
    
    poly[8] = x + ((float)size * s2 / 3.0);
    poly[9] = y + ((float)size * c2 / 3.0);

    poly[10] = x;
    poly[11] = y + size/6;    

    poly[12] = x - ((float)size * s2 / 3.0);
    poly[13] = y + ((float)size * c2 / 3.0);
    
    poly[14] = x - ((float)size * s1 / 6.0);
    poly[15] = y + ((float)size * c1 / 6.0);

    poly[16] = x - ((float)size * s1 / 3.0);
    poly[17] = y - ((float)size * c1 / 3.0);
    
    poly[18] = x - ((float)size * s2 / 6.0);
    poly[19] = y - ((float)size * c2 / 6.0);    
    
    draw_polygon(dr, poly, 10, err ? COL_ERROR : COL_STAR, black);

    return;
}

static void draw_cloud_template(drawing *dr, game_drawstate *ds,
                               int x, int y, int size, 
                               unsigned char err, bool hflash) {

    draw_rect(dr, x-size/3, y-size/3, 2*size/3 -1, 2*size/3 -1, 
              err ? COL_ERROR : hflash ? COL_FLASH : COL_CLOUD);        
    return;
}

static void draw_cross_template(drawing *dr, game_drawstate *ds,
                               int x, int y, int size, 
                               unsigned char err, bool hflash) {
    double thick = (size <= 21 ? 1 : 2.5);

    draw_thick_line(dr, thick,
        x-size/3, y-size/3, x+size/3, y+size/3,
        err ? COL_ERROR : hflash ? COL_FLASH : COL_GRID);  
    
    draw_thick_line(dr, thick,
        x+size/3, y-size/3, x-size/3, y+size/3,
        err ? COL_ERROR : hflash ? COL_FLASH : COL_GRID);  
    
    return;
}

static void draw_pencils(drawing *dr, game_drawstate *ds,
                             unsigned short pencil, 
                             int x, int y, bool hflash) {
    int dx, dy;
    int t = ds->tilesize;

    dx = BORDER+(x*t)+(t/4);
    dy = BORDER+(y*t)+(t/4);

    if (pencil & CODE_STAR)
        draw_star_template(dr, ds, dx, dy, t/2, false, hflash);

    if (pencil & CODE_CLOUD)
        draw_cloud_template(dr, ds, dx+t/2, dy, t/2, false, hflash);

    if (pencil & CODE_CROSS)
        draw_cross_template(dr, ds, dx, dy+t/2, t/2, false, hflash);

    return;
}

static void draw_planet(drawing *dr, game_drawstate *ds,
                             unsigned short planet, unsigned char error, 
                             int x, int y, bool hflash) {
    int dx,dy;
    int t = ds->tilesize;
    int black = (hflash ? COL_FLASH : COL_GRID);

    dx = BORDER+(x*t)+(t/2);
    dy = BORDER+(y*t)+(t/2);
    draw_circle(dr, dx-1, dy-1, t/3, COL_PLANET_DARK, black);

    if (planet & CODE_LEFT) {
        clip(dr, dx-(t/2)+2, dy-(t/2)+2, t/2-3, t-3);
        draw_circle(dr, dx-1, dy-1, t/3, COL_PLANET_LIGHT, black);
        unclip(dr);
    }
    else if (planet & CODE_RIGHT) {
        clip(dr, dx-1, dy-(t/2)+2, t/2-3, t-3);
        draw_circle(dr, dx-1, dy-1, t/3, COL_PLANET_LIGHT, black);
        unclip(dr);
    }
    
    if (planet & CODE_TOP) {
        clip(dr, dx-(t/2)+2, dy-(t/2)+2, t-3, t/2-3);
        draw_circle(dr, dx-1, dy-1, t/3, COL_PLANET_LIGHT, black);
        unclip(dr);
    }
    else if (planet & CODE_BOTTOM) {
        clip(dr, dx-(t/2)+2, dy-1, t-3, t/2-3);
        draw_circle(dr, dx-1, dy-1, t/3, COL_PLANET_LIGHT, black);
        unclip(dr);
    }

    if (error & ERROR_LEFT) {
        clip(dr, dx-(t/2)+2, dy-(t/2)+2, t/2-3, t-3);
        draw_circle(dr, dx-1, dy-1, t/3, COL_ERROR, black);
        unclip(dr);
    }
    if (error & ERROR_RIGHT) {
        clip(dr, dx-1, dy-(t/2)+2, t/2-3, t-3);
        draw_circle(dr, dx-1, dy-1, t/3, COL_ERROR, black);
        unclip(dr);
    }
    
    if (error & ERROR_TOP) {
        clip(dr, dx-(t/2)+2, dy-(t/2)+2, t-3, t/2-3);
        draw_circle(dr, dx-1, dy-1, t/3, COL_ERROR, black);
        unclip(dr);
    }
    if (error & ERROR_BOTTOM) {
        clip(dr, dx-(t/2)+2, dy-1, t-3, t/2-3);
        draw_circle(dr, dx-1, dy-1, t/3, COL_ERROR, black);
        unclip(dr);
    }

    return;
}


static void draw_star(drawing *dr, game_drawstate *ds,
                             unsigned char error, 
                             int x, int y, bool hflash) {
    int dx,dy;
    int t = ds->tilesize;

    dx = BORDER+(x*t)+(t/2);
    dy = BORDER+(y*t)+(t/2);

    draw_star_template(dr, ds, dx, dy, t, (error & ERROR_STAR), hflash);

     return;
}

static void draw_cloud(drawing *dr, game_drawstate *ds,
                             unsigned char error, 
                             int x, int y, bool hflash) {
    int dx,dy;
    int t = ds->tilesize;
    dx = BORDER+(x*t)+(t/2);
    dy = BORDER+(y*t)+(t/2);
 
    draw_cloud_template(dr, ds, dx, dy, t, (error & ERROR_CLOUD), hflash);
    
     return;
}

static void draw_cross(drawing *dr, game_drawstate *ds,
                             unsigned char error, 
                             int x, int y, bool hflash) {
    int dx,dy;
    int t = ds->tilesize;
    dx = BORDER+(x*t)+(t/2);
    dy = BORDER+(y*t)+(t/2);

    draw_cross_template(dr, ds, dx, dy, t, false, hflash);

    return;
}

#define FLASH_TIME 0.7F

static void game_redraw(drawing *dr, game_drawstate *ds, 
                        const game_state *oldstate, 
                        const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime)
{
    int x, y;
    bool stale, hflash, hchanged; 
    int t = ds->tilesize;
    hflash = (int)(flashtime * 5 / FLASH_TIME) % 2;

    if (!ds->started) {
        draw_rect(dr, 0, 0, 
                  2*BORDER + (ds->size)*t,
                  2*BORDER + (ds->size)*t, COL_BACKGROUND);
        draw_rect(dr, BORDER-2, BORDER-2,
                  (ds->size)*t +3, 
                  (ds->size)*t +3, COL_GRID);
        
        for (y=0;y<ds->size;y++)
        for (x=0;x<ds->size;x++)
            draw_rect(dr, BORDER+(t*x), BORDER+(t*y), 
                          t-1, t-1, COL_BACKGROUND);        
        
        draw_update(dr, 0, 0, 
                    2*BORDER+(ds->size)*t,
                    2*BORDER+(ds->size)*t);
     }

    hchanged = false;
    if (ds->hx != ui->hx || ds->hy != ui->hy ||
        ds->hshow != ui->hshow || ds->hpencil != ui->hpencil)
        hchanged = true;
    
    for (y=0;y<ds->size;y++) 
    for (x=0;x<ds->size;x++) {
        unsigned short c;
        unsigned char err;
        
        stale = false;
        c = state->grid[x+y*ds->size];
        err = state->errors[x+y*ds->size];
    
        if (ds->hflash != hflash) stale = true;

        if (hchanged) {
            if ((x == ui->hx && y == ui->hy) ||
                (x == ds->hx && y == ds->hy))
                stale = true;
        }
        
        if (ds->grid_errors[x+y*ds->size] != err) {
            stale = true;
            ds->grid_errors[x+y*ds->size] = err;
        }
            
        if (ds->grid[x+y*ds->size] != c) {
            stale = true;
            ds->grid[x+y*ds->size] = c;
        }
                    
        if (stale) {
            draw_cell_background(dr, ds, state, ui, x, y);
            if (c & CODE_GUESS) {
                draw_pencils(dr, ds, c, x, y, hflash);
            }
            else if (c & CODE_PLANET) {
                draw_planet(dr, ds, c, err, x, y, hflash);
            }
            else if (c & CODE_STAR) {
                draw_star(dr, ds, err, x, y, hflash);
            }
            else if (c & CODE_CLOUD) {
                draw_cloud(dr, ds, err, x, y, hflash);
            }
            else if (c & CODE_CROSS) {
                draw_cross(dr, ds, err, x, y, hflash);
            }
            draw_update(dr, BORDER+t*x, BORDER+t*y, t-1, t-1);
        }    
    }

    ds->hx = ui->hx; ds->hy = ui->hy;
    ds->hshow = ui->hshow;
    ds->hpencil = ui->hpencil;
    ds->hflash = hflash;
    ds->started = true;
    return;
}

static float game_anim_length(const game_state *oldstate,
                              const game_state *newstate, int dir, game_ui *ui)
{
    return 0.0F;
}

static float game_flash_length(const game_state *oldstate,
                               const game_state *newstate, int dir, game_ui *ui)
{
    return (!oldstate->solved && newstate->solved && !oldstate->cheated &&
            !newstate->cheated) ? FLASH_TIME : 0.0F;

}

static void game_get_cursor_location(const game_ui *ui,
                                     const game_drawstate *ds,
                                     const game_state *state,
                                     const game_params *params,
                                     int *x, int *y, int *w, int *h)
{
    if(ui->hshow) {
        *x = BORDER + (ui->hx) * TILE_SIZE;
        *y = BORDER + (ui->hy + 1) * TILE_SIZE;
        *w = *h = TILE_SIZE;
    }
}

static int game_status(const game_state *state)
{
    return state->solved;
}

static bool game_timing_state(const game_state *state, game_ui *ui)
{
    return true;
}

static void game_print_size(const game_params *params, float *x, float *y)
{
}

static void game_print(drawing *dr, const game_state *state, int tilesize)
{
}

#ifdef COMBINED
#define thegame nullgame
#endif

const struct game thegame = {
    "Stellar", "games.stellar", "stellar",
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
    NULL, /* current_key_label */
    interpret_move,
    execute_move,
    TILE_SIZE, game_compute_size, game_set_size,
    game_colours,
    game_new_drawstate,
    game_free_drawstate,
    game_redraw,
    game_anim_length,
    game_flash_length,
    game_get_cursor_location,
    game_status,
    false, false, game_print_size, game_print,
    false,                   /* wants_statusbar */
    false, game_timing_state,
    0,                       /* flags */
};

#ifdef STANDALONE_SOLVER

#include <stdarg.h>

int main(int argc, const char *argv[]) {
    

    return 0;
}

#endif

