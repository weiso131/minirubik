#!/bin/sh
# Solve one cube state with an RV32I ELF image on Ripes, as a command that
# looks like the native solvers. See usage() below.

usage() {
    cat <<END
usage: $0 STATE

Put STATE into a copy of the image with ripes_state.sh, run the copy on
Ripes in command-line mode, and print only what the program wrote. This is
the "SOLVER PPPPPPPOOOOOOO" form that god_test runs, so the image can be
checked without being rebuilt for every state.

  STATE      the cube state to solve

Environment:
  ELF        the image to run (default: IDDFS_solver_asm.elf next to this
             script)
  RIPES      the Ripes executable (default: the Ripes-*.AppImage next to
             this script)
  PROC       processor model (default: RV32_ISS)
  TIMEOUT    stop the run after this long, e.g. 10 or 2m (default: 10)
  IRET_LOG   append "STATE INSTRUCTIONS" to this file, the instructions
             the run retired, if it ran to the end

Exit status: the image's own exit code, 124 if TIMEOUT ran out, 1 if Ripes
produced no result, 2 for a usage error.
END
}

case ${1-} in
-h | --help)
    usage
    exit 0
    ;;
esac

if [ $# -ne 1 ]; then
    usage >&2
    exit 2
fi

root=$(dirname "$0")
elf=${ELF:-$root/IDDFS_solver_asm.elf}
ripes=${RIPES:-$(ls "$root"/Ripes-*.AppImage 2>/dev/null | head -n 1)}
proc=${PROC:-RV32_ISS}

[ -f "$elf" ] || { echo "$elf: no such file" >&2; exit 2; }
[ -n "$ripes" ] && [ -x "$ripes" ] || {
    echo "Ripes not found; set RIPES to its path" >&2
    exit 2
}

work=$(mktemp -d) || exit 1
trap 'rm -rf "$work"' 0 1 2 15
copy=$work/image.elf
"$root"/ripes_state.sh "$elf" "$1" "$copy" || exit 1

# Ripes locks its settings file while it starts, so runs that share one wait
# for each other; under god_test that turns into timeouts. Each run gets a
# settings directory of its own instead. Ripes also pads what the program
# prints with NUL bytes.
output=$(XDG_CONFIG_HOME=$work timeout "${TIMEOUT:-10}" "$ripes" --mode cli \
    --src "$copy" -t elf --proc "$proc" --iret 2>/dev/null | tr -d '\000')
code=$(printf '%s\n' "$output" | sed -n 's/^Program exited with code: //p')
if [ -z "$code" ]; then
    # Ripes prints that line whenever the program ends, so without it the
    # run was cut short.
    printf '%s\n' "$output"
    exit 124
fi
# The count is on the line after the one that names it. A line this short
# is appended in one write, so parallel runs can share the log.
if [ -n "${IRET_LOG-}" ]; then
    iret=$(printf '%s\n' "$output" | sed -n '/instructions retired/{n;p;}')
    printf '%s %s\n' "$1" "$iret" >>"$IRET_LOG"
fi

# What the program wrote is everything before the line Ripes adds on exit.
printf '%s\n' "$output" | sed '/^Program exited with code: /,$d'
exit "$code"
