CC ?= cc
CFLAGS ?= -O3 -std=c99 -Wall -Wextra -Wpedantic
FRAMA_C ?= frama-c
# Bare-metal RV32I build of IDDFS_solver, with the reference flags from the
# assignment. RV32_STATE is the cube state assembled into the image.
RV32_CC ?= riscv-none-elf-gcc
RV32_CFLAGS ?= -O2 -march=rv32i -mabi=ilp32
RV32_STATE ?= 21345671111111
CLANG_FORMAT := $(shell command -v clang-format-20 2>/dev/null || \
	command -v clang-format 2>/dev/null)
C_SOURCES := $(wildcard *.c *.h)
SAMPLE_STATE := 21345671111111
SAMPLE_SOLUTION := B' R' D2 R' B R B' R D2 B R'
VECTORS := tests/solutions.txt
# One per rejection path: short, long, cubie digit low, cubie digit high,
# orientation digit low, orientation digit high, non-digit, duplicate, parity.
INVALID_STATES := 1234567111111 123456711111111 02345671111111 82345671111111 \
	12345671111110 12345671111114 1234567111111a 11345671111111 12345671111112

.PHONY: all check god_check prove clean indent FORCE

all: solver mini IDDFS_solver god_test

solver: solver.c
	$(CC) $(CFLAGS) $< -o $@

mini: mini.c
	$(CC) $(CFLAGS) $< -o $@

# lookup_form_generator runs a BFS over every 2x2x2 state and prints the
# distance <= 5 table as C source. It is piped straight into the compiler, so
# the table never exists as a file.
lookup_form_generator: lookup_form_generator.c
	$(CC) $(CFLAGS) $< -o $@

IDDFS_solver: IDDFS_solver.c lookup_form.h lookup_form_generator
	./lookup_form_generator | $(CC) $(CFLAGS) IDDFS_solver.c -x c - -o $@

# Holds the state the RV32I images were last built for, and is rewritten only
# when RV32_STATE changes, so that changing it rebuilds them.
.rv32_state: FORCE
	@echo '$(RV32_STATE)' | cmp -s - $@ || echo '$(RV32_STATE)' >$@

# Nothing is linked in besides rv32/start.S: no libc, and no libgcc either,
# so a call to a compiler helper such as __mulsi3 or memcpy fails the link
# instead of slipping in.
IDDFS_solver.elf: IDDFS_solver.c lookup_form.h lookup_form_generator \
		rv32/start.S rv32/link.ld .rv32_state
	./lookup_form_generator | $(RV32_CC) $(RV32_CFLAGS) -ffreestanding \
		-nostdlib -static -DRIPES -DSTATE='"$(RV32_STATE)"' -T rv32/link.ld \
		rv32/start.S IDDFS_solver.c -x c - -o $@

# The hand-written assembly version. rv32/IDDFS_solver.S is the whole program
# (it has its own _start); the only C linked in is the generated table.
IDDFS_solver_asm.elf: rv32/IDDFS_solver.S lookup_form.h lookup_form_generator \
		rv32/link.ld .rv32_state
	./lookup_form_generator | $(RV32_CC) $(RV32_CFLAGS) -ffreestanding \
		-nostdlib -static -I. -DSTATE='"$(RV32_STATE)"' -T rv32/link.ld \
		rv32/IDDFS_solver.S -x c - -o $@

god_test: god_test.c
	$(CC) $(CFLAGS) $< -o $@

# Check IDDFS_solver against the BFS distance of every state. GOD_STEP=n
# tests only every n-th rank; GOD_JOBS sets the worker count.
GOD_STEP ?= 1
god_check: god_test IDDFS_solver
	./god_test ./IDDFS_solver $(GOD_STEP) $(GOD_JOBS)

check: solver mini $(VECTORS)
	./solver --self-test
	@expected=$$(mktemp); actual=$$(mktemp); \
		trap 'rm -f "$$expected" "$$actual"' 0 1 2 15; \
		count=0; \
		while IFS='|' read -r state solution; do \
			case "$$state" in ""|\#*) continue ;; esac; \
			printf '%s\n' "$$solution" >"$$expected"; \
			for binary in ./solver ./mini; do \
				$$binary "$$state" >"$$actual"; \
				status=$$?; \
				test $$status -eq 0 || { \
					echo "$$binary $$state: exit status $$status"; exit 1; }; \
				cmp -s "$$actual" "$$expected" || { \
					echo "$$binary $$state: output mismatch"; \
					echo "  expected: $$solution"; \
					printf '  got:      '; cat "$$actual"; \
					echo "  ($$(wc -c <"$$expected") bytes expected, \
$$(wc -c <"$$actual") produced)"; exit 1; }; \
			done; \
			count=$$((count + 1)); \
		done <$(VECTORS); \
		echo "$$count solution vectors matched by solver and mini"
	@for binary in ./solver ./mini; do \
		for bad in $(INVALID_STATES); do \
			$$binary "$$bad" >/dev/null 2>&1; \
			status=$$?; \
			test $$status -eq 2 || { \
				echo "$$binary $$bad: expected status 2, got $$status"; exit 1; }; \
		done; \
		$$binary >/dev/null 2>&1; \
		status=$$?; \
		test $$status -eq 2 || { \
			echo "$$binary with no argument: expected status 2, got $$status"; \
			exit 1; }; \
		$$binary $(SAMPLE_STATE) $(SAMPLE_STATE) >/dev/null 2>&1; \
		status=$$?; \
		test $$status -eq 2 || { \
			echo "$$binary with two arguments: expected status 2, got $$status"; \
			exit 1; }; \
		$$binary $(SAMPLE_STATE) >&- 2>/dev/null; \
		status=$$?; \
		test $$status -eq 1 || { \
			echo "$$binary with stdout closed: expected status 1, got $$status"; \
			exit 1; }; \
	done
	@./solver --self-test >&- 2>/dev/null; \
		status=$$?; \
		test $$status -eq 1 || { \
			echo "solver --self-test with stdout closed: expected 1, got $$status"; \
			exit 1; }
	@echo "invalid input rejected with status 2, unwritable stdout with status 1"

prove: solver.c
	@log=$$(mktemp); trap 'rm -f "$$log"' 0 1 2 15; \
		$(FRAMA_C) -wp -wp-fct quarter_turn,rank_state,valid,parse_state \
		-wp-rte -rte-verbose 0 -wp-prover alt-ergo -wp-timeout 20 \
		-wp-cache none solver.c >"$$log" 2>&1; rc=$$?; \
		grep -Fvx -e '[wp] Warning: Skipped RTE guards: unaligned pointers (\aligned not supported)' \
		-e '[wp] Warning: Skipped RTE guards: invalid function pointer calls (\valid_function not supported)' "$$log"; \
		test $$rc -eq 0 && awk '$$1 == "[wp]" && $$2 == "Proved" && $$3 == "goals:" && $$4 > 0 && $$4 == $$6 { ok = 1 } END { exit !ok }' "$$log" && \
		! grep -Eq '(^|[[:space:]])(Timeout|Unknown|Failed):' "$$log"

indent:
ifeq ($(CLANG_FORMAT),)
	$(error clang-format 20 not found)
endif
	@$(CLANG_FORMAT) --version | grep -q 'version 20' || \
		{ echo "error: clang-format version 20 required"; exit 1; }
	$(CLANG_FORMAT) -i $(C_SOURCES)

clean:
	$(RM) solver mini IDDFS_solver lookup_form_generator god_test \
		IDDFS_solver.elf IDDFS_solver_asm.elf .rv32_state
