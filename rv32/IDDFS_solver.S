#include "lookup_form.h"

#ifndef INPUT
#define INPUT "21345671111111"
#endif

/* Ripes environment calls: the number goes in a7, the argument in a0. */
#define PRINT_INT 1
#define PRINT_STR 4
#define EXIT 10
#define PRINT_CHAR 11
#define PRINT_HEX 34
#define EXIT2 93 /* exit code in a0 */

#define STATE_SIZE 14

    .section .text.start, "ax"
    .globl _start
_start:
    .option push
    .option norelax
    la   gp, __global_pointer$
    .option pop
    la   sp, __stack_top
parse_state:
    la   t0, input
    addi t0, t0, 6
    la   t4, input_state
    addi t4, t4, 6
    addi t5, x0, 0  # rotate sum

parse_state_loop1:
    lbu  t1, 0(t0)
    lbu  t2, 7(t0)

    # check range
    addi t3, x0, '1'
    bltu t1, t3, fail_parsing
    bltu t2, t3, fail_parsing
    addi t3, x0, '7'
    bltu t3, t1, fail_parsing
    addi t3, x0, '3'
    bltu t3, t2, fail_parsing

    # update o part
    addi t3, t2, -'1'
    sb   t3, 7(t4)
    # update rotate sum
    add  t5, t5, t3
    la   t3, mod3_form
    add  t3, t3, t5
    lbu  t5, 0(t3)

    # check permutation unique
    la   t2, input
    addi t2, t2, 6
    beq  t2, t0, parse_state_valid_loop_end
parse_state_valid_loop:
    lbu  t3, 0(t2)
    beq  t3, t1, fail_parsing
    addi t2, t2, -1
    bne  t2, t0, parse_state_valid_loop
parse_state_valid_loop_end:
    # update p part
    addi t3, t1, -'1'
    sb   t3, 0(t4)
    # next iterate
    addi t4, t4, -1
    addi t0, t0, -1
    la   t3, input
    bgeu t0, t3, parse_state_loop1

    # check the rotate sum
    bne  t5, x0, fail_parsing

    j    parsing_done
fail_parsing:
    j    exit

parsing_done:

solve:
    # s11 use as state_stack sp
    la   a0, input_state
    jal  ra, rank_state

    li   a7, PRINT_INT
    ecall

exit:
    li   a0, 0
    li   a7, EXIT2
    ecall
exit1:  
    j    exit1

.macro rank_state_for label1, label2
\label1:
    lbu  t3, 0(t4) # state->p[i]
    bgeu t3, t2, \label2
    addi t0, t0, 1
\label2:
    addi t4, t4, 1
    bne  t4, t5, \label1
.endm

rank_state:
    addi t0, x0, 0 # p
    addi t5, a0, 7 # state->p + CUBIES

    # loop1
    lbu  t2, 0(a0) # state->p[0]
    addi t4, a0, 1 
    rank_state_for rank_state_loop1, rank_state_loop1_no_plus
    slli t2, t0, 2
    slli t0, t0, 1
    add  t0, t0, t2

    # loop2
    lbu  t2, 1(a0) # state->p[1]
    addi t4, a0, 2
    rank_state_for rank_state_loop2, rank_state_loop2_no_plus
    slli t2, t0, 2
    add  t0, t0, t2

    # loop3
    lbu  t2, 2(a0) # state->p[2]
    addi t4, a0, 3
    rank_state_for rank_state_loop3, rank_state_loop3_no_plus
    slli t0, t0, 2

    # loop4
    lbu  t2, 3(a0) # state->p[3]
    addi t4, a0, 4
    rank_state_for rank_state_loop4, rank_state_loop4_no_plus
    slli t2, t0, 1
    add  t0, t0, t2

    # loop5
    lbu  t2, 4(a0) # state->p[4]
    addi t4, a0, 5
    rank_state_for rank_state_loop5, rank_state_loop5_no_plus
    slli t0, t0, 1

    # loop6
    lbu  t2, 5(a0) # state->p[5]
    addi t4, a0, 6
    rank_state_for rank_state_loop6, rank_state_loop6_no_plus

    # loop7
    addi t5, a0, 13 # state->o + 6
    addi t4, a0, 7  # state->o
    addi a0, x0, 0  # o
rank_state_loop7:
    lbu  t3, 0(t4)  # state->o[i]
    slli t2, a0, 1
    add  a0, a0, t2
    add  a0, a0, t3
    addi t4, t4, 1
    bne  t4, t5, rank_state_loop7

    add  a0, a0, t0
    slli t1, t0, 3
    add  a0, a0, t1
    slli t1, t0, 4
    add  a0, a0, t1
    slli t1, t0, 6
    add  a0, a0, t1
    slli t1, t0, 7
    add  a0, a0, t1
    slli t1, t0, 9
    add  a0, a0, t1

    jr ra

    .section .rodata
input:
    .asciz INPUT
    .balign 4       # align to 4 easy to copy
mod3_form:
    .byte 0, 1, 2, 0, 1, 2, 0, 1, 2, 3
    
    .section .bss
    .balign 4
input_state:
    .space STATE_SIZE
    .balign 4
