#!/bin/sh
# Run an RV32I ELF image on Ripes and report its retired instructions and
# memory use. See usage() below.

usage() {
    cat <<END
usage: $0 IMAGE.elf [RIPES]

Run IMAGE.elf on Ripes in command-line mode and print what it wrote, its exit
code, the instructions it retired, and the size of each of its sections
(.text, .rodata, .data, .bss, .stack) with their total.

  IMAGE.elf  the program to run, e.g. the output of "make IDDFS_solver.elf"
  RIPES      the Ripes executable (default: the Ripes-*.AppImage next to
             this script)

Environment:
  PROC       processor model (default: RV32_ISS)
  SIZE       "size" tool of the RISC-V toolchain
             (default: riscv-none-elf-size)

Exit status: 0 if Ripes ran the image, 1 if it produced no result, 2 for a
usage error. The image's own exit code is only printed.
END
}

case ${1-} in
-h | --help)
    usage
    exit 0
    ;;
esac

root=$(dirname "$0")
elf=$1
ripes=${2:-$(ls "$root"/Ripes-*.AppImage 2>/dev/null | head -n 1)}
proc=${PROC:-RV32_ISS}
size=${SIZE:-riscv-none-elf-size}

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    usage >&2
    exit 2
fi
[ -f "$elf" ] || { echo "$elf: no such file" >&2; exit 2; }
[ -n "$ripes" ] && [ -x "$ripes" ] || {
    echo "Ripes not found; pass its path as the second argument" >&2
    exit 2
}

# Ripes pads what the program prints with NUL bytes.
run() {
    "$ripes" --mode cli --src "$elf" -t elf --proc "$proc" --iret 2>/dev/null |
        tr -d '\000'
}

section() {
    "$size" -A "$elf" |
        awk -v name="$1" '$1 == name { bytes = $2 } END { print bytes + 0 }'
}

output=$(run)
iret=$(printf '%s\n' "$output" | sed -n '/instructions retired/{n;p;}')
code=$(printf '%s\n' "$output" | sed -n 's/^Program exited with code: //p')
if [ -z "$iret" ] || [ -z "$code" ]; then
    echo "no result from Ripes; it printed:" >&2
    printf '%s\n' "$output" >&2
    exit 1
fi

text=$(section .text)
rodata=$(section .rodata)
data=$(section .data)
bss=$(section .bss)
stack=$(section .stack)

echo "image:                $elf on $proc"
echo "output:               $(printf '%s\n' "$output" | sed -n 1p)"
echo "exit code:            $code"
echo "instructions retired: $iret"
printf '%-21s %7d bytes\n' .text: "$text" .rodata: "$rodata" .data: "$data" \
    .bss: "$bss" .stack: "$stack" \
    total: $((text + rodata + data + bss + stack))
