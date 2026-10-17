#include <stddef.h>

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


/* Padded to 16 bytes and word aligned so that a state is copied as four
 * words. p[7] and o[7] are never read as cubies.
 */
typedef struct {
    uint8_t p[CUBIES + 1], o[CUBIES + 1];
} __attribute__((aligned(4))) state_t;

/* A word that may alias the bytes of a state_t. */
typedef uint32_t __attribute__((may_alias)) state_word_t;

/* A step is turn * 3 + face, in the order R B D R2 B2 D2 R' B' D', so its
 * face is the step mod 3 and the step to try after it is the step plus one.
 * MOVES is the step of the stack's root, no move with a face no move has,
 * and the next_step of an entry with no step left to try.
 */
#define get_face(step) (mod3_form[step])

/* x mod 3 for x < MOVES. The last entry is the face of the root's step. */
uint8_t mod3_form[] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 3};

typedef struct {
    state_t now_state;
    uint8_t step;
    uint8_t next_step;
} stack_entry_t;

static const char *const step_names[MOVES] = {STEP_NAMES(C_NAME)};
static const uint8_t source[MOVES][CUBIES] = {SOURCE_ROWS(C_ROW)};
static const uint8_t twist[MOVES][CUBIES] = {TWIST_ROWS(C_ROW)};

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
static void apply_move(state_t *to, const state_t *from, uint8_t move)
{
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t cubie = source[move][i];
        to->p[i] = from->p[cubie];
        to->o[i] = mod3_form[from->o[cubie] + twist[move][i]];
    }
}

/* With RENDER defined, the Ripes GUI build draws every state on the way to the
 * solution on the LED matrix: the given one first, then one after each step
 * printed. The images measured are built without it.
 */
#ifdef RENDER
#ifndef RIPES
#error "RENDER draws on the Ripes LED matrix, so it needs RIPES"
#endif
static volatile uint32_t *const led = (volatile uint32_t *) LED_BASE;
static const uint8_t cube_pos[CUBIES][3] = {CUBE_POS_ROWS(C_ROW)};
static const uint8_t face_bias[CUBIES][3][2] = {FACE_BIAS_ROWS(C_XY_ROW)};
static const uint32_t colors[6] = {FACE_COLORS(C_WORD)};
static const uint8_t fixed_corner[3][3] = {FIXED_CORNER_ROWS(C_ROW)};

/* y * LED_WIDTH spelled out as 32 + 2 + 1, since RV32I cannot multiply. */
#if LED_WIDTH != 35
#error "fill() multiplies by 35"
#endif
static void fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb)
{
    volatile uint32_t *row = led + x + (y << 5) + (y << 1) + y;
    for (uint32_t j = 0; j < h; j++, row += LED_WIDTH)
        for (uint32_t i = 0; i < w; i++)
            row[i] = rgb;
}

static void draw_fixed_corner(void)
{
    fill(fixed_corner[0][0], fixed_corner[0][1], FACELET_SIZE, FACELET_SIZE,
         colors[fixed_corner[0][2]]);
    fill(fixed_corner[1][0], fixed_corner[1][1], FACELET_SIZE, FACELET_SIZE,
         colors[fixed_corner[1][2]]);
    fill(fixed_corner[2][0], fixed_corner[2][1], FACELET_SIZE, FACELET_SIZE,
         colors[fixed_corner[2][2]]);
}

/* Facelet k of position i shows face (o + k) mod 3 of the cubie there. */
static void draw_cube(const state_t *state)
{
    const uint8_t *tmp_p;
    uint8_t tmp_o;

    for (int i = 0; i < 7; i++) {
        tmp_p = cube_pos[state->p[i]];
        tmp_o = state->o[i];
        fill(face_bias[i][0][0], face_bias[i][0][1], 3, 3, colors[tmp_p[mod3_form[tmp_o]]]);
        fill(face_bias[i][1][0], face_bias[i][1][1], 3, 3, colors[tmp_p[mod3_form[tmp_o + 1]]]);
        fill(face_bias[i][2][0], face_bias[i][2][1], 3, 3, colors[tmp_p[mod3_form[tmp_o + 2]]]);
    }
}
#define DRAW_STATE(state) draw_cube(state)
#else
#define DRAW_STATE(state) ((void) 0)
#endif

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

/* Index of rank in eleven_step_ranks, or -1 if it is not 11 moves away. */
static int search_eleven_step(uint32_t rank)
{
    int l = 0, r = ELEVEN_STEP_NUM;

    while (r - l > 1) {
        int lr = (l + r) >> 1;
        if (eleven_step_ranks[lr] > rank)
            r = lr;
        else
            l = lr;
    }
    if (eleven_step_ranks[l] == rank)
        return l;

    return -1;
}

/* Print first_step unless it is NULL and the steps on stack[1..sp], then
 * follow five_step_first_move from stack[sp].now_state, which must be in the
 * table, down to the solved state.
 */
static void print_solution(const stack_entry_t *stack,
                           uint8_t sp,
                           const char *first_step)
{
    state_t state;
    const char *separator = "";

    state_copy(&state, &stack[sp].now_state);

    if (first_step) {
        print_string(first_step);
        separator = " ";
        DRAW_STATE(&stack[0].now_state);
    }

    for (uint8_t i = 1; i <= sp; ++i) {
        print_string(separator);
        print_string(step_names[stack[i].step]);
        separator = " ";
        DRAW_STATE(&stack[i].now_state);
    }
    for (uint32_t rank = rank_state(&state); rank; rank = rank_state(&state)) {
        uint8_t step = get_five_step_first_move(search_five_step(rank));
        print_string(separator);
        print_string(step_names[step]);
        separator = " ";
        state_t previous;
        state_copy(&previous, &state);
        apply_move(&state, &previous, step);
        DRAW_STATE(&state);
    }
    print_string("\n");
}

static void solve(state_t *state)
{
    stack_entry_t stack[6];
    uint8_t sp;
    uint32_t rank = rank_state(state);
    stack[0].step = MOVES;
    /* The step from a state 11 moves away to the one the search starts from,
     * or NULL if the search starts from the given state.
     */
    const char *first_step = NULL;
    int eleven_step;

    state_copy(&stack[0].now_state, state);
    if (search_five_step(rank) >= 0) {
        print_solution(stack, 0, first_step);
        return;
    }

    eleven_step = search_eleven_step(rank);
    if (eleven_step >= 0) {
        uint8_t step = get_eleven_step_first_move(eleven_step);
        first_step = step_names[step];
        apply_move(&stack[0].now_state, state, step);
        stack[0].step = step;
    }

    for (uint8_t allow_step = 1;allow_step <= 5;allow_step++) {
        sp = 0;
        stack[sp].next_step = 0;
        while (!(sp == 0 && stack[sp].next_step == MOVES)) {
            if (sp == allow_step) {
                if (search_five_step(rank_state(&stack[sp].now_state)) >= 0) {
                    print_solution(stack, sp, first_step);
                    return;
                }
            pop_stack:
                do {
                    sp--;
                    stack[sp].next_step++;
                } while (sp > 0 && stack[sp].next_step == MOVES);

            } else {
                if (get_face(stack[sp].next_step) == get_face(stack[sp].step))
                    stack[sp].next_step++;
                uint8_t next_step = stack[sp].next_step;
                
                if (next_step == MOVES)
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

#ifdef RENDER
    draw_fixed_corner();
#endif
    DRAW_STATE(&state);
    solve(&state);

    return output_failed();
}
