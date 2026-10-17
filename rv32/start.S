/* Entry point of the bare-metal RV32I build.
 *
 * There is no command line on the target, so the cube state is the string
 * assembled in below and handed to main() as argv[1]. The build overrides it
 * with -DSTATE='"..."'. The string is at the symbol "input", as in
 * rv32/IDDFS_solver.S, so that ripes_state.sh can replace it in the image.
 *
 * sp starts at the top of the .stack block that rv32/link.ld reserves.
 * .bss is not cleared: Ripes starts all memory out as zero.
 */
#ifndef STATE
#define STATE "21345671111111"
#endif

    .section .text.start, "ax"
    .globl _start
_start:
    .option push
    .option norelax
    la   gp, __global_pointer$
    .option pop
    la   sp, __stack_top

    li   a0, 2
    la   a1, argv
    call main

    li   a7, 93                 /* Exit2: stop with a0 as the exit code */
    ecall
1:  j    1b

    .section .rodata
    .balign 4
argv:
    .word program_name, input, 0
program_name:
    .asciz "IDDFS_solver"
input:
    .asciz STATE
