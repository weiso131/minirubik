#include "lookup_form.h"

/* Everything that needs the C library is confined to this block. With RIPES
 * defined the program is built without one and prints through the Ripes
 * environment calls instead.
 */
#ifdef RIPES
static void print_string(const char *string)
{
    register const char *a0 __asm__("a0") = string;
    register int a7 __asm__("a7") = 4; /* PrintStr */
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}

static void print_usage(const char *program)
{
    (void) program;
    print_string("usage: IDDFS_solver PPPPPPPOOOOOOO\n");
}

/* A Ripes environment call has no way to report a failed write. */
static int output_failed(void)
{
    return 0;
}
#else
#include <stdio.h>

static void print_string(const char *string)
{
    fputs(string, stdout);
}

static void print_usage(const char *program)
{
    fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n", program);
}

static int output_failed(void)
{
    return fflush(stdout) != 0 || ferror(stdout);
}
#endif

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9
};

/* Padded to 16 bytes and word aligned so that a state is copied as four
 * words. p[7] and o[7] are never read as cubies.
 */
typedef struct {
    uint8_t p[CUBIES + 1], o[CUBIES + 1];
} __attribute__((aligned(4))) state_t;

/* A word that may alias the bytes of a state_t. */
typedef uint32_t __attribute__((may_alias)) state_word_t;

#define get_face(step) (step & 0x3)

#define get_turn(step) ((step >> 2) & 0x3)

uint8_t next_turn_form[] = {0x1, 0x2, 0x4, 0xFF, 0x5, 0x6, 0x8, 0xFF, 0x9, 0xa, 0xFF};

uint8_t mod3_form[] = {0, 1, 2, 0, 1};

typedef struct {
    state_t now_state;
    uint8_t step;
    uint8_t next_step;
} stack_entry_t;

/* Indexed by step (face | turn << 2); 3 and 7 are not steps. */
static const char *const step_names[] = {"R",  "B",  "D",  "", "R2", "B2",
                                         "D2", "",   "R'", "B'", "D'"};
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

/* States are never assigned as whole structs: the compiler turns that into
 * a call to memcpy, and a copy of known length is cheaper spelled out.
 */
static void state_copy(state_t *to, const state_t *from)
{
    ((state_word_t *) to)[0] = ((const state_word_t *) from)[0];
    ((state_word_t *) to)[1] = ((const state_word_t *) from)[1];
    ((state_word_t *) to)[2] = ((const state_word_t *) from)[2];
    ((state_word_t *) to)[3] = ((const state_word_t *) from)[3];
}

/* to and from must be different states. */
static void quarter_turn(state_t *to, const state_t *from, uint8_t face)
{
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t cubie = source[face][i];
        to->p[i] = from->p[cubie];
        to->o[i] = mod3_form[from->o[cubie] + twist[face][i]];
    }
}

/* to and from must be different states. */
static void apply_move(state_t *to, const state_t *from, uint8_t move)
{
    uint8_t turns = get_turn(move), face = get_face(move);
    quarter_turn(to, from, face);
    for (uint8_t i = 0; i < turns; ++i) {
        state_t previous;
        state_copy(&previous, to);
        quarter_turn(to, &previous, face);
    }
}

static uint32_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;
    for (uint8_t i = (uint8_t) 1U; i < CUBIES; ++i)
        p += state->p[i] < state->p[0];
    p = (p << 1) + (p << 2);

    for (uint8_t i = (uint8_t) 2U; i < CUBIES; ++i)
        p += state->p[i] < state->p[1];
    p = p + (p << 2);

    for (uint8_t i = (uint8_t) 3U; i < CUBIES; ++i)
        p += state->p[i] < state->p[2];
    p = (p << 2);

    for (uint8_t i = (uint8_t) 4U; i < CUBIES; ++i)
        p += state->p[i] < state->p[3];
    p = p + (p << 1);

    for (uint8_t i = (uint8_t) 5U; i < CUBIES; ++i)
        p += state->p[i] < state->p[4];
    p = p << 1;

    for (uint8_t i = (uint8_t) 6U; i < CUBIES; ++i)
        p += state->p[i] < state->p[5];

    
    for (uint8_t i = 0; i < 6; ++i)
        o = (o + (o << 1)) + state->o[i];
    return (p + (p << 3) + (p << 4) + (p << 6) + (p << 7) + (p << 9)) + o;
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
        sum = mod3_form[sum + state->o[i]];
    }
    return sum == 0;
}

/* Index of rank in five_step_ranks, or -1 if it is more than 5 moves away. */
static int search_five_step(uint32_t rank)
{
    int l = 0, r = FIVE_STEP_NUM;

    while (r - l > 1) {
        int lr = (l + r) >> 1;
        if (five_step_ranks[lr] > rank)
            r = lr;
        else
            l = lr;
    }
    if (five_step_ranks[l] == rank)
        return l;

    return -1;
}

/* Print the steps on stack[1..sp], then follow five_step_first_move from
 * stack[sp].now_state, which must be in the table, down to the solved state.
 */
static void print_solution(const stack_entry_t *stack, uint8_t sp)
{
    state_t state;
    const char *separator = "";

    state_copy(&state, &stack[sp].now_state);

    for (uint8_t i = 1; i <= sp; ++i) {
        print_string(separator);
        print_string(step_names[stack[i].step]);
        separator = " ";
    }
    for (uint32_t rank = rank_state(&state); rank; rank = rank_state(&state)) {
        uint8_t step = get_five_step_first_move(search_five_step(rank));
        print_string(separator);
        print_string(step_names[step]);
        separator = " ";
        state_t previous;
        state_copy(&previous, &state);
        apply_move(&state, &previous, step);
    }
    print_string("\n");
}

static void solve(state_t *state)
{
    stack_entry_t stack[12];
    uint8_t sp;

    state_copy(&stack[0].now_state, state);
    if (search_five_step(rank_state(state)) >= 0) {
        print_solution(stack, 0);
        return;
    }

    for (uint8_t allow_step = 1;allow_step <= 6;allow_step++) {
        sp = 0;
        stack[sp].step = 0xFF;
        stack[sp].next_step = 0;
        while (!(sp == 0 && stack[sp].next_step == 0xFF)) {
            if (sp == allow_step) {
                if (search_five_step(rank_state(&stack[sp].now_state)) >= 0) {
                    print_solution(stack, sp);
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

                apply_move(&stack[sp + 1].now_state, &stack[sp].now_state, next_step);
                stack[sp + 1].step = next_step;
                stack[sp + 1].next_step = 0;
                sp++;
            }
        }
    }
}

static int parse_state(const char *input, state_t *state)
{
    for (int i = 0;i < 7;++i) {
        if (input[i] < '1' || input[i] > '7' || \
            input[i + 7] < '1' || input[i + 7] > '3')
            return 0;
        state->p[i] = (uint8_t) (input[i] - '1');
        state->o[i] = (uint8_t) (input[i + 7] - '1');
    }
    return input[14] == '\0' && valid(state);
}

int main(int argc, char **argv)
{
    state_t state;
    if (argc != 2 || !parse_state(argv[1], &state)) {
        /* C99 5.1.2.2.1 lets argv[0] be null when argc is 0. */
        print_usage(argc > 0 && argv[0] ? argv[0] : "solver");
        return 2;
    }
    
    solve(&state);

    return output_failed();
}
