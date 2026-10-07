/* Memory layout for the bare-metal RV32I build of IDDFS_solver.
 *
 * Code starts at address 0 and data at 0x10000000, the same split Ripes uses
 * for its own assembler output. Everything that is not code goes into "data",
 * whose length is the assignment's static data budget, so going over
 * 128 KiB of .rodata + .data + .bss + stack is a link error.
 */
OUTPUT_ARCH(riscv)
ENTRY(_start)

/* Nothing dictates where the stack goes; _start points sp wherever it likes.
 * It is given a fixed block right after .bss so that the image's whole
 * memory use can be read off its section sizes. The program has no
 * recursion, so its deepest call chain is fixed: 336 bytes, the sum of the
 * frames that -fstack-usage reports for main, print_solution and apply_move.
 * Nothing checks that at run time, so recompute it if those functions change.
 */
STACK_SIZE = 512;

MEMORY
{
    text (rx) : ORIGIN = 0x00000000, LENGTH = 1M
    data (rw) : ORIGIN = 0x10000000, LENGTH = 128K
}

SECTIONS
{
    .text :
    {
        KEEP(*(.text.start))
        *(.text .text.*)
    } > text

    .rodata :
    {
        *(.rodata .rodata.* .srodata .srodata.*)
    } > data

    .data :
    {
        *(.data .data.*)
        /* gp sits mid-way through the small data so that gp-relative
         * addressing reaches 2 KiB on either side of it.
         */
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.*)
    } > data

    .bss (NOLOAD) :
    {
        *(.sbss .sbss.* .bss .bss.* COMMON)
    } > data

    .stack (NOLOAD) : ALIGN(16)
    {
        . += STACK_SIZE;
        __stack_top = .;
    } > data

    /DISCARD/ :
    {
        *(.comment .note .note.* .eh_frame .riscv.attributes)
    }
}
