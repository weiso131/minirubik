#!/bin/sh
# Run an RV32I image on several Ripes processor models and check that they
# agree. See usage() below.

usage() {
    cat <<END
usage: $0 STATE...

Solve each STATE with ripes_solve.sh once on every processor model in PROCS
and print what each run wrote, its exit code and the instructions it
retired. Every model must give the same output and exit code for a state;
the models differ only in how they run the program, not in what it does.

  STATE      a cube state to solve; give as many as needed

Environment:
  PROCS      the processor models, separated by spaces
             (default: "RV32_ISS RV32_5S")
  TIMEOUT    how long one run may take (default: 60, since the pipelined
             models are slower to simulate than RV32_ISS)
  ELF, RIPES passed on to ripes_solve.sh (see ripes_solve.sh --help)

Exit status: 0 if every model agreed on every state, 1 if not, 2 for a usage
error.
END
}

case ${1-} in
-h | --help)
    usage
    exit 0
    ;;
esac

if [ $# -lt 1 ]; then
    usage >&2
    exit 2
fi

root=$(dirname "$0")
procs=${PROCS:-RV32_ISS RV32_5S}
export TIMEOUT=${TIMEOUT:-60}

work=$(mktemp -d) || exit 1
trap 'rm -rf "$work"' 0 1 2 15

status=0
for state in "$@"; do
    echo "$state"
    first=
    for proc in $procs; do
        : >"$work/iret"
        output=$(PROC=$proc IRET_LOG=$work/iret "$root"/ripes_solve.sh "$state")
        code=$?
        iret=$(cut -d ' ' -f 2 <"$work/iret")
        printf '  %-18s exit %3d  iret %9s  "%s"\n' "$proc" "$code" \
            "${iret:--}" "$output"
        # The first model is what the others have to match.
        if [ -z "$first" ]; then
            first=$code:$output
        elif [ "$code:$output" != "$first" ]; then
            echo "  DIFFERENT from ${procs%% *}"
            status=1
        fi
        [ "$code" -ne 124 ] || status=1
    done
done
exit $status
