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

#ifdef __ASSEMBLER__
/* The preprocessor puts a whole expansion on one line, hence the ';'. A name
 * becomes a pointer, as in C, with its string placed in .rodata.str.
 */
#define ASM_BYTE_ROW(...) .byte __VA_ARGS__ ;
#define ASM_NAME_POINTER(name) \
    .word 1f ; .pushsection .rodata.str, "a" ; 1: .asciz name ; .popsection ;
#else
#define C_ROW(...) {__VA_ARGS__},
#define C_NAME(name) name,
#endif

#define FIVE_STEP_NUM 12224
#define get_five_step_first_move(i) ((five_step_first_move[(i) >> 1] >> (((i) & 1) << 2)) & 0xF)

#define ELEVEN_STEP_NUM 2644
#define get_eleven_step_first_move(i) ((eleven_step_first_move[(i) >> 1] >> (((i) & 1) << 2)) & 0xF)
