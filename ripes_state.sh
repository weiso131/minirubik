#!/bin/sh
# Change the cube state inside an RV32I ELF image without rebuilding it.
# See usage() below.

usage() {
    cat <<END
usage: $0 IMAGE.elf STATE [OUT.elf]

Overwrite the cube state assembled into IMAGE.elf with STATE. The state is
the string at the symbol "input"; its bytes are replaced in the file, so
nothing is assembled or linked again.

  IMAGE.elf  an image with an "input" symbol, e.g. the output of
             "make IDDFS_solver_asm.elf"
  STATE      the new state, at most 14 characters. It is not checked, so
             that states the program must reject can be tried too.
  OUT.elf    write the changed image here and leave IMAGE.elf alone
             (default: change IMAGE.elf itself)

Environment:
  READELF    "readelf" tool of the RISC-V toolchain
             (default: riscv-none-elf-readelf)

Exit status: 0 if the state was written, 1 if the image has no usable
"input" symbol, 2 for a usage error.
END
}

case ${1-} in
-h | --help)
    usage
    exit 0
    ;;
esac

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
    usage >&2
    exit 2
fi

elf=$1
state=$2
out=${3:-$elf}
readelf=${READELF:-riscv-none-elf-readelf}
# The image reserves room for a 14-character state and its terminating NUL.
state_max=14

[ -f "$elf" ] || { echo "$elf: no such file" >&2; exit 2; }
[ ${#state} -le $state_max ] || {
    echo "$state: longer than $state_max characters" >&2
    exit 2
}

address=$("$readelf" -sW "$elf" | awk '$8 == "input" { print $2; exit }')
[ -n "$address" ] || { echo "$elf: no \"input\" symbol" >&2; exit 1; }
address=$((0x$address))

# The file offset of an address comes from the loaded segment that holds it.
offset=$("$readelf" -lW "$elf" | awk '$1 == "LOAD" { print $2, $3, $5 }' |
    while read -r file_offset start file_size; do
        start=$((start))
        if [ $address -ge $start ] &&
            [ $((address + state_max + 1)) -le $((start + file_size)) ]; then
            echo $((file_offset + address - start))
            break
        fi
    done)
[ -n "$offset" ] || {
    echo "$elf: \"input\" is not inside a loaded segment" >&2
    exit 1
}

[ "$out" = "$elf" ] || cp "$elf" "$out" || exit 1
printf '%s\0' "$state" |
    dd of="$out" bs=1 seek="$offset" conv=notrunc status=none
