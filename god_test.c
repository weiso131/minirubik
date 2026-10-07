/* Exhaustive checker for a 2x2x2 solver executable.
 *
 * A BFS from the solved state gives the optimal move count of every state.
 * Each tested state is then handed to the solver as its only argument, and
 * the moves it prints must solve that state in exactly that many moves.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9,
    /* No state needs more than 11 moves. */
    DEPTHS = 12,
    OUTPUT_MAX = 255,
    /* Failures each worker prints in full; the rest are only counted. */
    REPORT_MAX = 10,
    /* One startup-cost sample is taken per this many tested states. */
    BASELINE_EVERY = 16
};

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

/* What a worker sends back to the parent through its pipe. Each worker
 * fills in its own copy and the parent adds them up after the workers are
 * done, so nothing here is ever shared between processes.
 */
typedef struct {
    uint32_t failed;
    /* Runs and their total wall time, split by optimal move count. */
    uint32_t tested[DEPTHS];
    uint64_t nanoseconds[DEPTHS];
    /* Runs on an input the solver rejects: process startup with no search. */
    uint32_t baseline_runs;
    uint64_t baseline_nanoseconds;
    uint64_t slowest;
    char slowest_input[2 * CUBIES + 1];
} result_t;

static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};
/* Each destination takes a cubie from source[face][destination]. */
static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

static state_t apply_move(state_t state, uint8_t move)
{
    uint8_t turns = (uint8_t) (move % 3U + 1U);
    for (uint8_t i = 0; i < turns; ++i)
        state = quarter_turn(state, (uint8_t) (move / 3U));
    return state;
}

static uint32_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (uint32_t) (CUBIES - i) + smaller;
    }
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state->o[i];
    return p * ORIENTATIONS + o;
}

static void unrank_state(uint32_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1U < (uint8_t) (CUBIES - i); ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
}

/* distance[rank] is the optimal move count of that state. */
static uint8_t *build_distance(void)
{
    uint8_t *distance = malloc(STATES);
    uint32_t *queue = malloc((size_t) STATES * sizeof *queue);
    static uint16_t permutation[3][PERMUTATIONS], orientation[3][ORIENTATIONS];
    uint32_t head = 0, tail = 1;
    state_t state;
    if (!distance || !queue) {
        free(distance);
        free(queue);
        return NULL;
    }
    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t) rank * ORIENTATIONS, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            permutation[face][rank] =
                (uint16_t) (rank_state(&next) / ORIENTATIONS);
        }
    }
    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            orientation[face][rank] =
                (uint16_t) (rank_state(&next) % ORIENTATIONS);
        }
    }
    memset(distance, UINT8_MAX, STATES);
    queue[0] = 0;
    distance[0] = 0;
    while (head < tail) {
        uint32_t here = queue[head++];
        uint16_t p = (uint16_t) (here / ORIENTATIONS);
        uint16_t o = (uint16_t) (here % ORIENTATIONS);
        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p, next_o = o;
            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation[face][next_p];
                next_o = orientation[face][next_o];
                uint32_t there = (uint32_t) next_p * ORIENTATIONS + next_o;
                if (distance[there] == UINT8_MAX) {
                    distance[there] = (uint8_t) (distance[here] + 1U);
                    queue[tail++] = there;
                }
            }
        }
    }
    free(queue);
    if (tail != STATES) {
        free(distance);
        return NULL;
    }
    return distance;
}

static int set_cloexec(int fd)
{
    int flags = fcntl(fd, F_GETFD);
    return flags < 0 ? -1 : fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static uint64_t now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t) t.tv_sec * 1000000000U + (uint64_t) t.tv_nsec;
}

/* Run "solver input" and capture its stdout as a string in output, which
 * holds OUTPUT_MAX + 1 bytes. *elapsed gets the nanoseconds from just before
 * the solver is spawned until it has been reaped. With quiet set, the
 * solver's stderr is discarded. Returns NULL on success, else what went wrong.
 */
static const char *run_solver(const char *solver,
                              const char *input,
                              char *output,
                              uint64_t *elapsed,
                              int quiet)
{
    char *const argv[] = {(char *) solver, (char *) input, NULL};
    posix_spawn_file_actions_t actions;
    int fds[2], status, error, truncated = 0;
    size_t length = 0;
    uint64_t start;
    pid_t pid;

    output[0] = '\0';
    *elapsed = 0;
    if (pipe(fds) < 0)
        return "pipe failed";
    /* The solver keeps only the copy that dup2 puts on its stdout. */
    if (set_cloexec(fds[0]) < 0 || set_cloexec(fds[1]) < 0 ||
        posix_spawn_file_actions_init(&actions) != 0) {
        close(fds[0]);
        close(fds[1]);
        return "cannot set up the solver's stdout";
    }
    error = posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    if (!error && quiet)
        error = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO,
                                                 "/dev/null", O_WRONLY, 0);
    start = now();
    if (!error)
        error = posix_spawn(&pid, solver, &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    if (error) {
        close(fds[0]);
        return "cannot run the solver";
    }

    for (;;) {
        char overflow[256];
        ssize_t got = length < OUTPUT_MAX
                          ? read(fds[0], output + length, OUTPUT_MAX - length)
                          : read(fds[0], overflow, sizeof overflow);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            break;
        if (length < OUTPUT_MAX)
            length += (size_t) got;
        else
            truncated = 1;
    }
    output[length] = '\0';
    close(fds[0]);

    while (waitpid(pid, &status, 0) < 0)
        if (errno != EINTR)
            return "waitpid failed";
    *elapsed = now() - start;
    if (truncated)
        return "output too long";
    if (memchr(output, '\0', length))
        return "output contains a NUL byte";
    if (WIFSIGNALED(status))
        return "solver killed by a signal";
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return "solver exited with a nonzero status";
    return NULL;
}

/* Apply the printed moves to state. Returns NULL if they solve it in exactly
 * optimal moves, else what is wrong with them. The output is not modified.
 */
static const char *check_solution(state_t state,
                                  const char *output,
                                  uint8_t optimal)
{
    static const char spaces[] = " \t\r\n";
    unsigned count = 0;
    const char *token = output + strspn(output, spaces);

    while (*token) {
        size_t length = strcspn(token, spaces);
        uint8_t move = 0;
        while (move < MOVES && (strlen(move_names[move]) != length ||
                                memcmp(move_names[move], token, length) != 0))
            ++move;
        if (move == MOVES)
            return "unknown move";
        state = apply_move(state, move);
        ++count;
        token += length;
        token += strspn(token, spaces);
    }
    if (rank_state(&state) != 0)
        return "moves do not solve the state";
    if (count != optimal)
        return "solution is not optimal";
    return NULL;
}

/* Test every step-th rank starting at first; returns how that went. */
static result_t test_ranks(const char *solver,
                           const uint8_t *distance,
                           uint64_t first,
                           uint64_t step)
{
    result_t result;
    uint32_t tested = 0;

    memset(&result, 0, sizeof result);
    for (uint64_t rank = first; rank < STATES; rank += step) {
        char input[2 * CUBIES + 1], output[OUTPUT_MAX + 1];
        const char *problem;
        uint64_t elapsed;
        state_t state;

        unrank_state((uint32_t) rank, &state);
        for (uint8_t i = 0; i < CUBIES; ++i) {
            input[i] = (char) ('1' + state.p[i]);
            input[i + CUBIES] = (char) ('1' + state.o[i]);
        }
        input[2 * CUBIES] = '\0';

        /* The solver rejects this before searching, so the run costs only
         * what every run costs. Sampling it all the way through, rather than
         * once up front, keeps it under the same load as the real runs.
         */
        if (tested++ % BASELINE_EVERY == 0) {
            run_solver(solver, "00000000000000", output, &elapsed, 1);
            ++result.baseline_runs;
            result.baseline_nanoseconds += elapsed;
        }

        problem = run_solver(solver, input, output, &elapsed, 0);
        if (!problem)
            problem = check_solution(state, output, distance[rank]);
        ++result.tested[distance[rank]];
        result.nanoseconds[distance[rank]] += elapsed;
        if (elapsed > result.slowest) {
            result.slowest = elapsed;
            memcpy(result.slowest_input, input, sizeof input);
        }
        if (!problem)
            continue;
        if (result.failed++ < REPORT_MAX) {
            /* One write keeps lines from different workers apart. */
            char line[OUTPUT_MAX + 160];
            int length;
            output[strcspn(output, "\r\n")] = '\0';
            length = snprintf(line, sizeof line,
                              "FAIL %s (rank %lu, optimal %u): %s: \"%s\"\n",
                              input, (unsigned long) rank,
                              (unsigned) distance[rank], problem, output);
            if (length > 0 && write(STDERR_FILENO, line, (size_t) length) < 0)
                continue;
        }
    }
    return result;
}

static int read_all(int fd, void *buffer, size_t size)
{
    size_t done = 0;
    while (done < size) {
        ssize_t got = read(fd, (char *) buffer + done, size - done);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            return -1;
        done += (size_t) got;
    }
    return 0;
}

static int write_all(int fd, const void *buffer, size_t size)
{
    size_t done = 0;
    while (done < size) {
        ssize_t put = write(fd, (const char *) buffer + done, size - done);
        if (put < 0 && errno == EINTR)
            continue;
        if (put <= 0)
            return -1;
        done += (size_t) put;
    }
    return 0;
}

static long parse_count(const char *text, long limit)
{
    char *end;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    return errno || *end || end == text || value < 1 || value > limit ? -1
                                                                       : value;
}

int main(int argc, char **argv)
{
    enum { JOBS_MAX = 1024 };
    long step = 1, jobs = sysconf(_SC_NPROCESSORS_ONLN);
    result_t total;
    uint64_t start, wall, tested = 0, nanoseconds = 0;
    double baseline = 0;
    uint8_t *distance;
    pid_t *pids;
    int *pipes, broken = 0;

    memset(&total, 0, sizeof total);
    if (jobs < 1)
        jobs = 1;
    if (jobs > JOBS_MAX)
        jobs = JOBS_MAX;
    if (argc < 2 || argc > 4 ||
        (argc > 2 && (step = parse_count(argv[2], STATES)) < 0) ||
        (argc > 3 && (jobs = parse_count(argv[3], JOBS_MAX)) < 0)) {
        fprintf(stderr,
                "usage: %s SOLVER [STEP [JOBS]]\n"
                "  SOLVER  executable to test, run as: SOLVER PPPPPPPOOOOOOO\n"
                "  STEP    test only the ranks divisible by STEP (default 1)\n"
                "  JOBS    worker processes (default: online processors)\n",
                argc > 0 && argv[0] ? argv[0] : "god_test");
        return 2;
    }
    if (access(argv[1], X_OK) != 0) {
        perror(argv[1]);
        return 2;
    }

    distance = build_distance();
    pids = malloc((size_t) jobs * sizeof *pids);
    pipes = malloc((size_t) jobs * sizeof *pipes);
    if (!distance || !pids || !pipes) {
        fprintf(stderr, "cannot build the distance table\n");
        return 1;
    }

    /* Worker w takes the tested ranks w, w + jobs, w + 2 * jobs, ... counted
     * in units of step, and reports its totals through its own pipe. The
     * table is built before the fork, so every worker shares it.
     */
    fflush(NULL);
    start = now();
    for (long w = 0; w < jobs; ++w) {
        int fds[2];
        if (pipe(fds) < 0 || set_cloexec(fds[0]) < 0 ||
            set_cloexec(fds[1]) < 0 || (pids[w] = fork()) < 0) {
            perror("cannot start a worker");
            return 1;
        }
        if (pids[w] == 0) {
            result_t result;
            close(fds[0]);
            result = test_ranks(argv[1], distance, (uint64_t) (w * step),
                                (uint64_t) (jobs * step));
            _exit(write_all(fds[1], &result, sizeof result) != 0);
        }
        close(fds[1]);
        pipes[w] = fds[0];
    }

    for (long w = 0; w < jobs; ++w) {
        result_t result;
        int status, reported = read_all(pipes[w], &result, sizeof result) == 0;
        close(pipes[w]);
        while (waitpid(pids[w], &status, 0) < 0 && errno == EINTR)
            ;
        if (!reported || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "worker %ld died without reporting\n", w);
            broken = 1;
            continue;
        }
        total.failed += result.failed;
        total.baseline_runs += result.baseline_runs;
        total.baseline_nanoseconds += result.baseline_nanoseconds;
        for (int d = 0; d < DEPTHS; ++d) {
            total.tested[d] += result.tested[d];
            total.nanoseconds[d] += result.nanoseconds[d];
        }
        if (result.slowest > total.slowest) {
            total.slowest = result.slowest;
            memcpy(total.slowest_input, result.slowest_input,
                   sizeof total.slowest_input);
        }
    }
    wall = now() - start;

    /* "run us" is the mean time of one whole solver process. "solve us" is
     * that minus the mean startup-only run, i.e. the search itself.
     */
    if (total.baseline_runs)
        baseline = (double) total.baseline_nanoseconds / 1e3 /
                   total.baseline_runs;
    printf("moves   states    run us  solve us\n");
    for (int d = 0; d < DEPTHS; ++d) {
        double mean;
        if (!total.tested[d])
            continue;
        mean = (double) total.nanoseconds[d] / 1e3 / total.tested[d];
        printf("%5d %8lu %9.1f %9.1f\n", d, (unsigned long) total.tested[d],
               mean, mean - baseline);
        tested += total.tested[d];
        nanoseconds += total.nanoseconds[d];
    }
    if (tested) {
        double mean = (double) nanoseconds / 1e3 / (double) tested;
        printf("  all %8lu %9.1f %9.1f\n", (unsigned long) tested, mean,
               mean - baseline);
        printf("startup only: %.1f us mean over %lu runs\n", baseline,
               (unsigned long) total.baseline_runs);
        printf("slowest run: %.1f us on %s\n", (double) total.slowest / 1e3,
               total.slowest_input);
    }
    printf("wall time: %.1f ms with %ld jobs\n", (double) wall / 1e6, jobs);
    printf("%s: %lu states tested, %lu failed\n", argv[1],
           (unsigned long) tested, (unsigned long) total.failed);
    free(distance);
    free(pids);
    free(pipes);
    if (fflush(stdout) != 0 || ferror(stdout))
        return 1;
    return total.failed || broken;
}
