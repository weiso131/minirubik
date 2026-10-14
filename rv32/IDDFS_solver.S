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

#define STATE_SIZE 16

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
    addi s11, x0, 0
    la   s10, state_stack
    addi t0, x0, 9
    sb   t0, 14(s10)  # stack[0].step = MOVES;
    addi a0, s10, 0
    jal  ra, rank_state

search_eleven_step:
    addi a1, x0, 0 # l
    li   a2, 2644  # r
    la   a3, eleven_step_ranks
    addi a4, x0, 1
search_eleven_step_loop:
    add  a7, a1, a2 
    srli a7, a7, 1  # lr = (l + r) / 2
    slli a5, a7, 2  # size is 4 bytes
    add  a6, a5, a3 
    lw   a5, 0(a6)  # eleven_step_ranks[lr]
    bgeu a0, a5, search_eleven_step_else
    addi a2, a7, 0
    j    search_eleven_step_loop_continue
search_eleven_step_else:
    addi a1, a7, 0
search_eleven_step_loop_continue:
    sub  a5, a2, a1
    bltu a4, a5, search_eleven_step_loop # while (r - l > 1)

    slli a2, a1, 2  # size is 4 bytes
    add  a2, a2, a3
    lw   a3, 0(a2)  # eleven_step_ranks[l]
    bne  a3, a0, search_eleven_step_neq
# is eleven step state
    la   t0, eleven_step_first_move
    addi t1, a1, 0
    srli t1, t1, 1
    add  t1, t1, t0
    lbu  a2, 0(t1)    # five_step_first_move[(i) >> 1]
    andi a1, a1, 1    # (i) & 1
    slli a1, a1, 2    # ((i) & 1) << 2
    srl  a2, a2, a1   # (five_step_first_move[(i) >> 1] >> (((i) & 1) << 2))
    andi a2, a2, 0xF  # a2: step
    sb   a2, 14(s10)  # stack[0].step = step;
    
    la   s5, step_names # config
    la   s6, space
    slli t1, a2, 2
    add  t0, t1, s5
    lw   a0, 0(t0)      # don't use check 5 step after this
    li   a7, PRINT_STR
    ecall
    addi a0, s6, 0
    li   a7, PRINT_STR
    ecall

    addi a0, s10, 16 # apply_move to state_stack[1] first
    addi a1, s10, 0
    # a2 is already set to move value
    jal  apply_move

    # than copy to state_stack[0]
    lw   t0, 16(s10)
    lw   t1, 20(s10)
    lw   t2, 24(s10)
    lhu  t3, 28(s10)
    sw   t0, 0(s10)
    sw   t1, 4(s10)
    sw   t2, 8(s10)
    sh   t3, 12(s10)

    j    not_five_step
search_eleven_step_neq:

# check 5 step
    jal  ra, search_five_step
    bge  a0, x0, print_solution

not_five_step:
# start IDDFS

    addi s9, x0, 1 # uint8_t allow_step = 1
    la   s8, mod3_form
    addi s7, x0, 9 # s7: MOVES
IDDFS_loop:
    addi s11, x0, 0 
    sb   s11, 15(s10) # stack[sp].next_step = 0
IDDFS_allow_step_loop:
    bne  s11, s9, IDDFS_get_next_step
IDDFS_check_if_solve:
    slli a0, s11, 4
    add  a0, a0, s10 # stack + sp
    jal  ra, rank_state
    jal  ra, search_five_step
    bge  a0, x0, print_solution # if (search_five_step(rank_state(&stack[sp].now_state)) >= 0)
IDDFS_pop_stack_loop:
    addi s11, s11, -1
    slli t0, s11, 4
    add  t0, t0, s10
    lbu  t1, 15(t0)
    addi t1, t1, 1
    sb   t1, 15(t0) # stack[sp].next_step
    beq  s11, x0, IDDFS_check_allow_step_loop_end
    bne  t1, s7, IDDFS_allow_step_loop
    j    IDDFS_pop_stack_loop
IDDFS_check_allow_step_loop_end:
    beq  s7, t1, IDDFS_allow_step_loop_end
    /* if (sp == 0 && stack[sp].next_step == MOVES) break */
IDDFS_get_next_step:
    # t5: next_step
    # t6: stack + sp
    slli t6, s11, 4
    add  t6, s10, t6 # stack + sp
    lbu  t5, 15(t6) # stack[sp].next_step
    lbu  t2, 14(t6) # stack[sp].step
    add  t1, t5, s8 # mod3_form + stack[sp].next_step
    add  t2, t2, s8 # mod3_form + stack[sp].step
    lbu  t3, 0(t1)
    lbu  t4, 0(t2)
    bne  t3, t4, IDDFS_not_skip
    addi t5, t5, 1
    sb   t5, 15(t6)
    beq  t5, s7, IDDFS_pop_stack_loop
IDDFS_not_skip:
    addi a0, t6, 16 # stack[sp + 1].now_state
    addi a1, t6, 0
    addi a2, t5, 0
    jal  ra, apply_move
    sb   t5, 30(t6)
    sb   x0, 31(t6)
    addi s11, s11, 1
    j    IDDFS_allow_step_loop
IDDFS_allow_step_loop_end:
    addi t0, x0, 6
    addi s9, s9, 1
    bne  s9, t0, IDDFS_loop # ;allow_step <= 5;allow_step++
# IDDFS_loop end

    j    exit

print_solution:
    # previous: s1
    # rank    : s2
    # step    : s3
    # five_step_first_move : s4
    # step_names: s5
    # space   : s6
    la   s6, space
    slli s11, s11, 4
    add  s1, s10, s11 # state_t *previous = &stack[sp].now_state;
    la   s4, five_step_first_move
    la   s5, step_names

    # five step left part
    addi a0, s1, 0
    jal  ra, rank_state
    addi s2, a0, 0    # rank = rank_state(&stack[sp].now_state)
    xori s11, s11, 16

print_solution_loop_5_step:
    addi a0, s2, 0
    jal  ra, search_five_step
    # get_five_step_first_move
    addi a1, a0, 0
    srli a1, a1, 1    # i >> 1
    
    add  a1, a1, s4
    lbu  s3, 0(a1)    # five_step_first_move[(i) >> 1]
    andi a0, a0, 1    # (i) & 1
    slli a0, a0, 2    # ((i) & 1) << 2
    srl  s3, s3, a0   # (five_step_first_move[(i) >> 1] >> (((i) & 1) << 2))
    andi s3, s3, 0xF  # s3: step

    slli t1, s3, 2
    add  t0, t1, s5
    lw   a0, 0(t0)
    li   a7, PRINT_STR
    ecall
    addi a0, s6, 0
    li   a7, PRINT_STR
    ecall

    add  a0, s10, s11
    addi a1, s1, 0
    addi a2, s3, 0
    jal  apply_move

    add  s1, s10, s11 # previous = &stack[sp].now_state;

    addi a0, s1, 0
    jal  ra, rank_state
    addi s2, a0, 0

    xori s11, s11, 16
    bne  s2, x0, print_solution_loop_5_step # rank != 0

exit:
    li   a0, 0
    li   a7, EXIT2
    ecall
exit1:  
    j    exit1

.macro rank_state_for label1, label2
\label1:
    lbu  a4, 0(a5) # state->p[i]
    bgeu a4, a3, \label2
    addi a1, a1, 1
\label2:
    addi a5, a5, 1
    bne  a5, a6, \label1
.endm

rank_state:
    addi a1, x0, 0 # p
    addi a6, a0, 7 # state->p + CUBIES

    # loop1
    lbu  a3, 0(a0) # state->p[0]
    addi a5, a0, 1 
    rank_state_for rank_state_loop1, rank_state_loop1_no_plus
    slli a3, a1, 2
    slli a1, a1, 1
    add  a1, a1, a3

    # loop2
    lbu  a3, 1(a0) # state->p[1]
    addi a5, a0, 2
    rank_state_for rank_state_loop2, rank_state_loop2_no_plus
    slli a3, a1, 2
    add  a1, a1, a3

    # loop3
    lbu  a3, 2(a0) # state->p[2]
    addi a5, a0, 3
    rank_state_for rank_state_loop3, rank_state_loop3_no_plus
    slli a1, a1, 2

    # loop4
    lbu  a3, 3(a0) # state->p[3]
    addi a5, a0, 4
    rank_state_for rank_state_loop4, rank_state_loop4_no_plus
    slli a3, a1, 1
    add  a1, a1, a3

    # loop5
    lbu  a3, 4(a0) # state->p[4]
    addi a5, a0, 5
    rank_state_for rank_state_loop5, rank_state_loop5_no_plus
    slli a1, a1, 1

    # loop6
    lbu  a3, 5(a0) # state->p[5]
    addi a5, a0, 6
    rank_state_for rank_state_loop6, rank_state_loop6_no_plus

    # loop7
    addi a6, a0, 13 # state->o + 6
    addi a5, a0, 7  # state->o
    addi a0, x0, 0  # o
rank_state_loop7:
    lbu  a4, 0(a5)  # state->o[i]
    slli a3, a0, 1
    add  a0, a0, a3
    add  a0, a0, a4
    addi a5, a5, 1
    bne  a5, a6, rank_state_loop7

    add  a0, a0, a1
    slli a2, a1, 3
    add  a0, a0, a2
    slli a2, a1, 4
    add  a0, a0, a2
    slli a2, a1, 6
    add  a0, a0, a2
    slli a2, a1, 7
    add  a0, a0, a2
    slli a2, a1, 9
    add  a0, a0, a2

    jr ra

search_five_step:
    addi a1, x0, 0 # l
    li   a2, 12224 # r
    la   a3, five_step_ranks
    addi a4, x0, 1
search_five_step_loop:
    add  a7, a1, a2 
    srli a7, a7, 1  # lr = (l + r) / 2
    slli a5, a7, 2  # size is 4 bytes
    add  a6, a5, a3 
    lw   a5, 0(a6)  # five_step_ranks[lr]
    bgeu a0, a5, search_five_step_else
    addi a2, a7, 0
    j    search_five_step_loop_continue
search_five_step_else:
    addi a1, a7, 0
search_five_step_loop_continue:
    sub  a5, a2, a1
    bltu a4, a5, search_five_step_loop # while (r - l > 1)

    slli a2, a1, 2  # size is 4 bytes
    add  a2, a2, a3
    lw   a3, 0(a2)  # five_step_ranks[l]
    bne  a3, a0, search_five_step_neq
    addi a0, a1, 0  # ret = l
    jr   ra
search_five_step_neq:
    addi a0, x0, -1
    jr   ra

apply_move:
    # a0: to
    # a1: from
    # a2: move

    slli a3, a2, 2
    add  a3, a3, a2
    slli a2, a2, 1 
    add  a2, a2, a3 # move * 7

    # no large argument function

    la   a4, source

    add  a4, a4, a2    # source + move * 7
    la   a5, twist
    add  a5, a5, a2    # twist + move * 7
    la   a6, mod3_form

    # a0: to->p
    # a1: from->p

    # loop start
    .rept 7
    lbu  a7, 0(a4)  # uint8_t cubie = source[move][i];
    
    add  t0, a1, a7  # from->p[cubie]
    lbu  t1, 0(t0)
    sb   t1, 0(a0)   # to->p[i] = 
    
    add  a7, a7, a1
    lbu  t0, 7(a7)   # from->o[cubie]
    lbu  t1, 0(a5)   # twist[move][i]
    add  t0, t0, t1
    add  t0, t0, a6
    lbu  t1, 0(t0)   # mod3_form[from->o[cubie] + twist[move][i]];
    sb   t1, 7(a0)   # to->o[i] = 

    addi a2, a2, 1
    addi a0, a0, 1
    addi a4, a4, 1
    addi a5, a5, 1
    .endr
    jr   ra

    .section .rodata
input:
    .asciz INPUT
    .balign 4       # align to 4 easy to copy
mod3_form:
    .byte 0, 1, 2, 0, 1, 2, 0, 1, 2, 3
source:
    SOURCE_ROWS(ASM_BYTE_ROW)
twist:
    TWIST_ROWS(ASM_BYTE_ROW)
space:
    .asciz " "
    .balign 4
step_names:
    STEP_NAMES(ASM_NAME_POINTER)
    
    .section .bss
    .balign 4
state_stack:
input_state:
    .space STATE_SIZE * 6
