#!/bin/sh
# Compare the instructions two RV32I images retire on Ripes, state by state.
# See usage() below.

usage() {
    cat <<END
usage: $0 [MOVES [STEP [JOBS]]]

Run ripes_iret.sh on the assembly image and then on the C image, over the
same states: those at exactly MOVES moves whose rank is divisible by STEP.
Both runs check every answer. Then pair the two counts of each state and
report how many states the assembly image is not faster on, the ratio of the
totals, the range of the per-state ratio and saving, and the costliest state
of each image.

  MOVES      the distance to test (default: 11)
  STEP       test only the ranks divisible by STEP (default: 1, all of them)
  JOBS       Ripes runs at a time (default: online processors)

Environment:
  ASM_ELF    the assembly image (default: IDDFS_solver_asm.elf next to this
             script)
  C_ELF      the C image (default: IDDFS_solver.elf next to this script)
  LOGDIR     keep the "STATE INSTRUCTIONS" lines of each image in
             LOGDIR/asm-MOVES-STEP.log and LOGDIR/c-MOVES-STEP.log
             (default: a temporary directory)
  RIPES, PROC, TIMEOUT
             passed on to ripes_solve.sh (see ripes_solve.sh --help)

Exit status: 0 if both images solved every state optimally and every state
was counted for both, 1 if not, 2 for a usage error.
END
}

case ${1-} in
-h | --help)
    usage
    exit 0
    ;;
esac

if [ $# -gt 3 ]; then
    usage >&2
    exit 2
fi

root=$(dirname "$0")
moves=${1:-11}
step=${2:-1}
jobs=${3:-$(nproc)}
asm_elf=${ASM_ELF:-$root/IDDFS_solver_asm.elf}
c_elf=${C_ELF:-$root/IDDFS_solver.elf}

for elf in "$asm_elf" "$c_elf"; do
    [ -f "$elf" ] || { echo "$elf: no such file" >&2; exit 2; }
done

if [ -n "${LOGDIR-}" ]; then
    dir=$LOGDIR
    mkdir -p "$dir" || exit 1
else
    dir=$(mktemp -d) || exit 1
    trap 'rm -rf "$dir"' 0 1 2 15
fi
asm_log=$dir/asm-$moves-$step.log
c_log=$dir/c-$moves-$step.log

# What the numbers depend on, so that a run can be told apart from another.
echo "distance $moves, every rank divisible by $step, $jobs jobs"
echo "commit: $(git -C "$root" describe --always --dirty 2>/dev/null || echo unknown)"
echo "Ripes: $(basename "${RIPES:-$(ls "$root"/Ripes-*.AppImage 2>/dev/null | head -n 1)}"), ${PROC:-RV32_ISS}"

status=0
echo
echo "=== assembly: $asm_elf"
ELF=$asm_elf LOG=$asm_log "$root"/ripes_iret.sh "$moves" "$step" "$jobs" || status=1
echo
echo "=== C: $c_elf"
ELF=$c_elf LOG=$c_log "$root"/ripes_iret.sh "$moves" "$step" "$jobs" || status=1

# join needs both logs sorted the same way; a state missing from either one
# was not counted there and makes the comparison incomplete.
LC_ALL=C sort "$asm_log" >"$asm_log.sorted"
LC_ALL=C sort "$c_log" >"$c_log.sorted"
asm_states=$(wc -l <"$asm_log")
c_states=$(wc -l <"$c_log")
pairs=$(LC_ALL=C join "$asm_log.sorted" "$c_log.sorted" | wc -l)
if [ "$pairs" -ne "$asm_states" ] || [ "$pairs" -ne "$c_states" ]; then
    echo "only $pairs states counted for both images" \
        "($asm_states assembly, $c_states C)" >&2
    status=1
fi

echo
echo "=== assembly against C over $pairs states"
LC_ALL=C join "$asm_log.sorted" "$c_log.sorted" | awk '
    {
        saved = $3 - $2
        ratio = $2 / $3
        asm += $2
        c += $3
        if (saved <= 0)
            slower++
        if (NR == 1 || saved < least_saved) {
            least_saved = saved
            least_state = $1
        }
        if (NR == 1 || saved > most_saved)
            most_saved = saved
        if (NR == 1 || ratio < least_ratio)
            least_ratio = ratio
        if (NR == 1 || ratio > most_ratio)
            most_ratio = ratio
        if ($2 > asm_most) {
            asm_most = $2
            asm_state = $1
        }
        if ($3 > c_most) {
            c_most = $3
            c_state = $1
        }
    }
    END {
        if (!NR)
            exit
        printf "states assembly is not faster on: %d\n", slower
        printf "total:     assembly %d, C %d, assembly/C %.4f\n", asm, c, asm / c
        printf "mean:      assembly %.0f, C %.0f\n", asm / NR, c / NR
        printf "per state: assembly/C %.4f to %.4f, saving %d to %d\n",
            least_ratio, most_ratio, least_saved, most_saved
        printf "           (least saving on %s)\n", least_state
        printf "costliest: assembly %d on %s, C %d on %s\n",
            asm_most, asm_state, c_most, c_state
    }'
rm -f "$asm_log.sorted" "$c_log.sorted"
[ -n "${LOGDIR-}" ] && echo "logs: $asm_log $c_log"
exit $status
