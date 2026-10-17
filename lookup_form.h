/* Also included by rv32/IDDFS_solver.S, which can use the constants but not
 * the C declarations; the assembler needs none to refer to the tables.
 */
#ifndef __ASSEMBLER__
#include <stdint.h>

extern uint32_t five_step_ranks[];
extern uint8_t five_step_first_move[];
extern uint32_t eleven_step_ranks[];
extern uint8_t eleven_step_first_move[];
#endif

#define CUBIES 7
#define PERMUTATIONS 5040
#define ORIENTATIONS 729
#define STATES (PERMUTATIONS * ORIENTATIONS)
#define MOVES 9

/* The small tables are written once here as lists and expanded twice: by
 * IDDFS_solver.c into C initialisers and by rv32/IDDFS_solver.S into
 * directives. X is applied to each row, or to each name.
 */
#define STEP_NAMES(X) \
    X("R") X("B") X("D") X("R2") X("B2") X("D2") X("R'") X("B'") X("D'")

/* Each destination takes a cubie from source[step][destination]. The rows of
 * a half turn and of a counter-clockwise turn are the quarter turn of that
 * face composed two and three times.
 */
#define SOURCE_ROWS(X)                                                \
    X(1, 4, 2, 0, 3, 5, 6) X(0, 1, 2, 4, 5, 6, 3) X(0, 2, 5, 3, 1, 4, 6) \
    X(4, 3, 2, 1, 0, 5, 6) X(0, 1, 2, 5, 6, 3, 4) X(0, 5, 4, 3, 2, 1, 6) \
    X(3, 0, 2, 4, 1, 5, 6) X(0, 1, 2, 6, 3, 4, 5) X(0, 4, 1, 3, 5, 2, 6)

#define TWIST_ROWS(X)                                                 \
    X(1, 2, 0, 2, 1, 0, 0) X(0, 0, 0, 1, 2, 1, 2) X(0, 0, 0, 0, 0, 0, 0) \
    X(0, 0, 0, 0, 0, 0, 0) X(0, 0, 0, 0, 0, 0, 0) X(0, 0, 0, 0, 0, 0, 0) \
    X(1, 2, 0, 2, 1, 0, 0) X(0, 0, 0, 1, 2, 1, 2) X(0, 0, 0, 0, 0, 0, 0)

/* The LED matrix of the Ripes GUI, one word 0x00RRGGBB per LED, row by row.
 * The renderer draws the cube on it as an unfolded net, one facelet as a
 * FACELET_SIZE square.
 */
#define LED_BASE 0xf0000000
#define LED_WIDTH 35
#define LED_HEIGHT 25
#define FACELET_SIZE 3

/* The faces of the cubie numbered c, in the order of its orientation, are
 * cube_pos[c][]: the faces it sits on when the cube is solved, which are its
 * colours. Face 5 is up, 0 front, 1 right, 2 back, 3 left and 4 down. The
 * order runs the way twist[][] turns a cubie, so that a cubie with
 * orientation o shows its face o + k on facelet k of its position.
 */
#define CUBE_POS_ROWS(X)                                              \
    X(5, 1, 0) X(4, 0, 1) X(4, 3, 0) X(5, 2, 1) X(4, 1, 2) X(4, 2, 3) \
    X(5, 3, 2)

/* Where facelet k of position i is drawn: face_bias[i][k] is the x and y of
 * its top left LED, with k in the same order as in cube_pos.
 */
#define FACE_BIAS_ROWS(X)                                           \
    X(10, 3, 14, 7, 10, 7) X(10, 14, 10, 10, 14, 10)                 \
    X(7, 14, 3, 10, 7, 10) X(10, 0, 21, 7, 17, 7)                    \
    X(10, 17, 17, 10, 21, 10) X(7, 17, 24, 10, 0, 10)                \
    X(7, 0, 0, 7, 24, 7)

/* The colour of each face, by face number. */
#define FACE_COLORS(X)                                                 \
    X(0x0000ff) X(0x00ff00) X(0x00ffff) X(0xff0000) X(0xff00ff) \
    X(0xffff00)

/* The fixed up-front-left corner, which no move turns: x, y and face of each
 * of its facelets. It is drawn once, before the first state.
 */
#define FIXED_CORNER_ROWS(X) X(7, 3, 5) X(7, 7, 0) X(3, 7, 3)

#ifdef __ASSEMBLER__
/* The preprocessor puts a whole expansion on one line, hence the ';'. A name
 * becomes a pointer, as in C, with its string placed in .rodata.str.
 */
#define ASM_BYTE_ROW(...) .byte __VA_ARGS__ ;
#define ASM_WORD_ROW(...) .word __VA_ARGS__ ;
#define ASM_NAME_POINTER(name) \
    .word 1f ; .pushsection .rodata.str, "a" ; 1: .asciz name ; .popsection ;
#else
#define C_ROW(...) {__VA_ARGS__},
#define C_XY_ROW(x0, y0, x1, y1, x2, y2) {{x0, y0}, {x1, y1}, {x2, y2}},
#define C_WORD(word) word,
#define C_NAME(name) name,
#endif

#define FIVE_STEP_NUM 12224
#define get_five_step_first_move(i) ((five_step_first_move[(i) >> 1] >> (((i) & 1) << 2)) & 0xF)

#define ELEVEN_STEP_NUM 2644
#define get_eleven_step_first_move(i) ((eleven_step_first_move[(i) >> 1] >> (((i) & 1) << 2)) & 0xF)
