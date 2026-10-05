#include <stdio.h>

#include "lookup_form.h"

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9
};

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

#define get_face(step) (step & 0x3)

#define get_turn(step) ((step >> 2) & 0x3)

char next_turn_form[] = {0x1, 0x2, 0x4, 0xFF, 0x5, 0x6, 0x8, 0xFF, 0x9, 0xa, 0xFF};

typedef struct {
    state_t now_state;
    uint8_t step;
    uint8_t next_step;
} stack_entry_t;

static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};
static const uint8_t inverse_move[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};
/* Each destination takes a cubie from source[face][destination]. */
static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

static state_t apply_move(state_t state, uint8_t move)
{
    
    uint8_t turns = get_turn(move), face = get_face(move);
    for (uint8_t i = 0; i <= turns; ++i)
        state = quarter_turn(state, face);
    return state;
}

static uint32_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (CUBIES - i) + smaller;
    }
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state->o[i];
    return p * ORIENTATIONS + o;
}


static int valid(const state_t *state)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state->p[i] >= CUBIES || state->o[i] >= 3)
            return 0;
        for (uint8_t j = 0; j < i; ++j)
            if (state->p[j] == state->p[i])
                return 0;
        sum = (uint8_t) (sum + state->o[i]);
    }
    return sum % 3U == 0;
}

static uint8_t search_five_step(uint32_t rank)
{
    int l = 0, r = FIVE_STEP_NUM + 1;

    while (r - l > 1) {
        int lr = (l + r) >> 1;
        if (five_step_ranks[lr] > rank)
            r = lr;
        else
            l = lr;
    }
    if (five_step_ranks[l] == rank)
        goto FIND;
    
    return 0;

FIND:
    return get_five_step_distance(l);
        
}

static void solve(state_t *state)
{
    stack_entry_t stack[12];
    uint8_t sp;
    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};

    uint8_t in_five_step = search_five_step(rank_state(state));
    if (in_five_step) {
        printf("%d\n", in_five_step);
        return;
    }

    for (uint8_t allow_step = 1;allow_step <= 6;allow_step++) {
        sp = 0;
        stack[sp] = (stack_entry_t) {*state, 0xFF, 0};
        while (!(sp == 0 && stack[sp].next_step == 0xFF)) {
            if (sp == allow_step) {
                uint8_t in_five_step = search_five_step(rank_state(&stack[sp].now_state));
                if (in_five_step) {
                    printf("%d\n", allow_step + in_five_step);
                    return;
                }
            pop_stack:
                do {
                    sp--;
                    stack[sp].next_step = next_turn_form[stack[sp].next_step];
                } while (sp > 0 && stack[sp].next_step == 0xFF);

            } else {
                if (get_face(stack[sp].next_step) == get_face(stack[sp].step))
                    stack[sp].next_step = next_turn_form[stack[sp].next_step];
                uint8_t next_step = stack[sp].next_step;
                
                if (next_step == 0xFF)
                    goto pop_stack;

                stack[sp + 1].now_state = apply_move(stack[sp].now_state, stack[sp].next_step);
                stack[sp + 1].step = next_step;
                stack[sp + 1].next_step = 0;
                sp++;
            }
        }
    }
}

static int parse_state(const char *input, state_t *state)
{
    for (int i = 0; i < 14; ++i) {
        int limit = i < 7 ? 7 : 3;
        if (input[i] < '1' || input[i] > '0' + limit)
            return 0;
        (i < 7 ? state->p : state->o)[i % 7] = (uint8_t) (input[i] - '1');
    }
    return input[14] == '\0' && valid(state);
}

static int output_failed(void)
{
    return fflush(stdout) != 0 || ferror(stdout);
}

int main(int argc, char **argv)
{
    state_t state;
    uint8_t diameter;
    if (argc != 2 || !parse_state(argv[1], &state)) {
        /* C99 5.1.2.2.1 lets argv[0] be null when argc is 0. */
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 && argv[0] ? argv[0] : "solver");
        return 2;
    }
    
    solve(&state);

    return output_failed();
}
