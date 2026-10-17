#!/bin/sh
# Check an RV32I image on Ripes against every state at one distance and
# report the instructions it retires on them. See usage() below.

usage() {
    cat <<END
usage: $0 [MOVES [STEP [JOBS]]]

Run god_test with ripes_solve.sh on the states whose optimal solution has
exactly MOVES moves, which checks every answer, and then report the fewest,
mean and most instructions retired over those runs, with the states that
retired the most.

  MOVES      the distance to test (default: 11)
  STEP       test only the ranks divisible by STEP (default: 1, all of them)
  JOBS       Ripes runs at a time (default: online processors)

Environment:
  ELF, RIPES, PROC, TIMEOUT
             passed on to ripes_solve.sh (see ripes_solve.sh --help)
  LOG        keep the "STATE INSTRUCTIONS" line of every run in this file
             (default: a temporary file)
  WORST      how many of the costliest states to list (default: 5)

Exit status: 0 if every state was solved optimally and counted, 1 if not,
2 for a usage error.
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
worst=${WORST:-5}

[ -x "$root"/god_test ] || { echo "$root/god_test not found; run make god_test" >&2; exit 2; }

if [ -n "${LOG-}" ]; then
    log=$LOG
    : >"$log" || exit 1
else
    log=$(mktemp) || exit 1
    trap 'rm -f "$log"' 0 1 2 15
fi

IRET_LOG=$log "$root"/god_test "$root"/ripes_solve.sh "$step" "$jobs" \
    "$moves" "$moves"
status=$?

# Each run that ran to the end left a line in the log, including the runs
# god_test makes on 00000000000000 to time startup alone; those are dropped.
grep -v '^00000000000000 ' "$log" >"$log.states"
mv "$log.states" "$log"
counted=$(wc -l <"$log")
echo "instructions retired over $counted runs:"
sort -k 2,2n "$log" | awk '
    NR == 1 { least = $2 }
    { sum += $2; most = $2 }
    END {
        if (NR)
            printf "  fewest %d, mean %.0f, most %d\n", least, sum / NR, most
    }'
echo "costliest states:"
sort -k 2,2nr "$log" | head -n "$worst" | sed 's/^/  /'
exit $((status != 0))
