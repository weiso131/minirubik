# minirubik

An optimal C99 solver for the 2×2×2 Rubik’s Cube. It builds a breadth-first
table for all 3,674,160 states and solves every valid position in at most 11
half-turn-metric moves.

## Why a cube is a graph

Ernő Rubik created the original cube in 1974 to demonstrate how parts can move
independently without breaking the whole. A 3×3 cube has 20 moving pieces and
about 4.3 × 10¹⁹ reachable arrangements. The smaller 2×2 cube keeps the eight
corners and removes the edges and fixed centers. [Philo Li’s formula-free
introduction](https://philoli.com/zh/blog/solve-rubiks-cube-without-formulas/)
offers the key intuition: every turn is reversible, turns can be composed, and
their order matters—`R U` is generally not `U R`.

Human solvers use those facts to move a few pieces while restoring the rest;
the commutator `A B A⁻¹ B⁻¹` is the standard example. This program uses the
same group structure differently: it treats every valid arrangement as a node,
every face turn as an edge, and searches the entire graph once. It does not use
the article’s 3×3 Roux stages or a library of memorized algorithms.

The solver gives the eight corner positions the numbers `0–7`. The 2.5D
walkthrough below shows where those numbers are on the physical cube.

## How it works

1. Fix one corner to remove whole-cube rotations.
2. Rank the remaining corner permutation and six independent orientations into
   a dense integer.
3. Breadth-first search outward from solved using `R`, `B`, and `D`, including
   inverse and half turns.
4. Store one move toward solved for every state; following those moves gives an
   optimal solution of at most 11 moves.

## Build and run

```sh
make
make check
make prove   # optional: Frama-C WP proof, needs frama-c and alt-ergo
./solver 21345671111111
```

`make` builds two binaries. `solver` is the documented one, with contracts, a
`--self-test` mode, and diagnostics on stderr. `mini` is a golfed variant that
solves the same input and prints the same line, kept as a readability contrast;
it has no `--self-test` and prints nothing on failure, and it trades roughly
eight times the runtime and three times the memory for its brevity.

The 14-digit argument describes the scramble and the printed line is the
solution. Both formats are explained below.

### Checking a solver against every state

`god_test` runs a solver executable on every state and checks each answer
against a breadth-first table of optimal move counts:

```sh
./god_test SOLVER [STEP [JOBS]]
make god_check                 # ./god_test ./IDDFS_solver, all states
make god_check GOD_STEP=1000   # every 1000th state, well under a second
```

- `SOLVER` is run as `SOLVER PPPPPPPOOOOOOO`, once per tested state. It must
  exit with status 0 and print moves that solve the state in the optimal
  number of moves; any optimal solution is accepted.
- `STEP` tests only the ranks divisible by `STEP`. The default, 1, covers all
  3,674,160 states.
- `JOBS` is the number of worker processes, by default one per online
  processor. Each worker takes its own share of the ranks and reports its
  totals to the parent through a pipe when it is done.

Up to ten failures per worker are printed to stderr in full, and the exit
status is 1 if any state failed. The summary also times the runs, grouped by
optimal move count:

```
moves   states    run us  solve us
    9    18870     527.9     139.7
   10     6330     859.2     471.1
   11       29    2439.5    2051.3
startup only: 388.2 us mean over 2300 runs
```

`run us` is the mean wall time of one whole solver process. `startup only` is
the same measurement on an input the solver rejects, sampled once every 16
states, and `solve us` is `run us` minus that. These are rough figures for
comparing changes, not a benchmark: short searches sit inside the noise and
can come out slightly negative.

### Running on RV32I with Ripes

`IDDFS_solver.c` also builds as a bare-metal RV32I program, with no C library
and no multiply or divide. It needs `riscv-none-elf-gcc` on the `PATH`:

```sh
make IDDFS_solver.elf                            # state 21345671111111
make IDDFS_solver.elf RV32_STATE=76543213113131  # any other state
```

There is no command line on the target, so the state is assembled into the
image as a string in `rv32/start.S`; `RV32_STATE` sets it, and changing it
rebuilds the image. `rv32/link.ld` lays the image out, including a fixed
512-byte stack, inside the assignment's 128 KiB budget for everything that is
not code.

`ripes_test.sh` runs an image on [Ripes](https://github.com/mortbopet/Ripes)
in command-line mode and reports what it cost:

```sh
./ripes_test.sh IDDFS_solver.elf                 # Ripes-*.AppImage in this directory
./ripes_test.sh IDDFS_solver.elf /path/to/Ripes  # some other Ripes
PROC=RV32_5S ./ripes_test.sh IDDFS_solver.elf    # another processor model
./ripes_test.sh --help
```

```
image:                IDDFS_solver.elf on RV32_ISS
output:               R D R B2 R B D2 B D R B'
exit code:            0
instructions retired: 10549852
.text:                   1956 bytes
.rodata:                  216 bytes
.data:                  55025 bytes
.bss:                       0 bytes
.stack:                   512 bytes
total:                  57709 bytes
```

The instruction count is Ripes's `--iret`, and the sizes are read from the
image's sections. Ripes runs with no ISA extensions enabled, but it does not
reject a `mul` inside an ELF image; that is enforced when the image is built,
by `-march=rv32i` and by linking without libgcc.

### Reading the 14-digit input

The program receives one 14-digit code with no spaces. For explanation, split
it into two groups:

```diagram
2134567 1111111
└── P ─┘ └── O ─┘
  cubies   twists
```

Imagine seven numbered seats and seven students. A position is a seat fixed in
space; a cubie is the physical corner that can move to another seat. In the
solved cube, cubie 1 sits in position 1, cubie 2 in position 2, and so on.
The real cube has no printed numbers; `0–7` are labels used only by this solver.

#### Step 1: Hold the cube in one direction

Keep `FRONT` facing you and `UP` pointing upward. Position `0` is the corner
nearest the upper-left of the front face. It is an anchor for describing the
other corners; the physical cubie is not glued in place.

```diagram
                              BACK
                    ·───────────────·
                   ╱               ╱│
                  ╱        UP     ╱ │
                 ╱               ╱  │
              [0]───────────────·   │
               │                │   │
               │     FRONT      │ R │
               │                │   ·
               │                │  ╱
               │                │ ╱
               │                │╱
               ·────────────────·
```

`R` marks the narrow `RIGHT` face.

#### Step 2: Separate the front and back layers

A 2×2×2 cube has only corner cubies. Looking from the fixed direction, four
corner positions touch the front face and four touch the back face. Each
bracketed number below names one whole corner, not one colored sticker:

```diagram
 FRONT LAYER                          BACK LAYER

 upper-left   upper-right             upper-left   upper-right
     [0]────────[1]                       [7]────────[4]
      │          │                         │          │
      │          │       front ↔ back      │          │
     [3]────────[2]                       [6]────────[5]
 down-left    down-right               down-left    down-right
```

The front layer runs clockwise from its upper-left corner as `0, 1, 2, 3`.
The back layer is drawn as if seen through the cube from the front: `7` is
upper-left, followed clockwise by `4, 5, 6`.

#### Step 3: Join the two layers into positions 0–7

Slide the back square up and to the right, the same direction the cube recedes
in Step 1, to get the complete 2.5D position map. The back edges are drawn
through the front face rather than hidden behind it:

```diagram
                           BACK
                      [7]────────[4]
                     ╱ │        ╱ │
                  [0]──│─────[1]  │
                   │   │      │   │
                   │  [6]─────│──[5]
                   │ ╱        │ ╱
                  [3]────────[2]
                      FRONT
```

The seven characters of `P` describe positions `1, 2, 3, 4, 5, 6, 7` in that
order; the anchor at position `0` is left out.

#### Step 4: Put the cubies into those positions

Compare the position map on the left with the filled cube on the right. Read
`P = 2134567` from left to right to fill the positions. The arrows below the
figure identify the two positions that change.

```diagram
 POSITION MAP                             AFTER P = 2134567
 (fixed seats)                            (cubies now in seats)

     [7]────────[4]                           [7]────────[4]
    ╱ │        ╱ │                           ╱ │        ╱ │
 [0]──│─────[1]  │                        [0]──│─────[2]  │
  │   │      │   │                         │   │      │   │
  │  [6]─────│──[5]                        │  [6]─────│──[5]
  │ ╱        │ ╱                           │ ╱        │ ╱
 [3]────────[2]                           [3]────────[1]
     FRONT                                    FRONT

 position:     1 2 3 4 5 6 7
 P says:       2 1 3 4 5 6 7
               │ │ └───────── cubies 3–7 stay in their matching seats
               │ └─────────── put cubie 1 in position 2: [2] becomes [1]
               └───────────── put cubie 2 in position 1: [1] becomes [2]
```

So the first two digits, `21`, exchange the two corners on the front-right
edge. The remaining digits, `34567`, leave the other five movable corners
where they were. `P` must contain every digit from `1` through `7` exactly
once; otherwise a cubie would be missing or duplicated.

The seven seats named by `P` are:

| Position | Corner of the cube |
| :---: | :--- |
| 1 | front, upper, right |
| 2 | front, down, right |
| 3 | front, down, left |
| 4 | back, upper, right |
| 5 | back, down, right |
| 6 | back, down, left |
| 7 | back, upper, left |

The second group, `O = 1111111`, describes the twist of the cubie in each of
those same seven positions:

| Digit | Meaning |
| :---: | :--- |
| 1 | not twisted |
| 2 | twisted by +120° |
| 3 | twisted by −120° |

Here every orientation digit is `1`, so the two corners change places without
being twisted. For a valid cube, convert orientation digits to `0`, `1`, and
`2`; their sum must be divisible by three. The solved code is
`12345671111111`. `make check` uses the exchanged-corner example above.

## Reading the solution

```sh
$ ./solver 21345671111111
B' R' D2 R' B R B' R D2 B R'
```

Each token is one face turn. Apply them left to right; after the last one the
cube is solved.

| Token | Meaning |
| :---: | :--- |
| `R` | turn the `RIGHT` face 90° clockwise |
| `B` | turn the `BACK` face 90° clockwise |
| `D` | turn the `DOWN` face 90° clockwise |

Clockwise means clockwise as seen by someone looking directly at that face from
outside the cube, so you have to walk around to the back to read `B` and look up
from underneath to read `D`. Two suffixes modify a turn:

| Suffix | Meaning |
| :---: | :--- |
| none | 90° clockwise |
| `'` | 90° counterclockwise, the inverse |
| `2` | 180°, direction does not matter |

`R`, `B`, and `D` are the only faces that appear, because turning `UP`, `FRONT`,
or `LEFT` would move the anchor at position `0`. A turn counts as one move
whichever suffix it carries, which is the half-turn metric; under that metric no
position needs more than 11 moves. Solving an already-solved cube prints an
empty line.

See [`report.md`](report.md) for the model, algorithm, diagrams, and Frama-C
validation notes.
