# The Game of Sim -- solver, front end and tests.

CC      ?= cc
CSTD     = -std=c11
WARN     = -Wall -Wextra -Wpedantic
OPT     ?= -O2
CFLAGS   = $(CSTD) $(WARN) $(OPT)
SAN      = -fsanitize=address,undefined -fno-omit-frame-pointer

# The shipped binary turns assertions off; they cost a linear scan per node.
# Tests and the benchmark keep them on so engine invariants are checked.
RELEASE  = $(CFLAGS) -DNDEBUG

.PHONY: all check test cli asan bench clean

all: sim

sim: main.c sim.c sim.h
	$(CC) $(RELEASE) -o $@ main.c sim.c

test_sim: test_sim.c sim.c sim.h
	$(CC) $(CFLAGS) -o $@ test_sim.c sim.c

simbench: bench.c sim.c sim.h
	$(CC) $(CFLAGS) -o $@ bench.c sim.c

simbench-sym: bench.c sim.c sim.h
	$(CC) $(CFLAGS) -DSIM_SYMMETRY=1 -o $@ bench.c sim.c

test_sim-sym: test_sim.c sim.c sim.h
	$(CC) $(CFLAGS) -DSIM_SYMMETRY=1 -o $@ test_sim.c sim.c

sim.asan: main.c sim.c sim.h
	$(CC) $(CSTD) $(WARN) -O1 -g $(SAN) -o $@ main.c sim.c

test_sim.asan: test_sim.c sim.c sim.h
	$(CC) $(CSTD) $(WARN) -O1 -g $(SAN) -o $@ test_sim.c sim.c

test_sim-sym.asan: test_sim.c sim.c sim.h
	$(CC) $(CSTD) $(WARN) -O1 -g $(SAN) -DSIM_SYMMETRY=1 -o $@ test_sim.c sim.c

## test  -- engine unit tests, both memo strategies
test: test_sim test_sim-sym
	./test_sim
	@echo "--- again, symmetry-reduced ---"
	./test_sim-sym

## cli   -- malformed-input tests against the front end
cli: sim
	./cli_test.sh ./sim

## asan  -- everything again under AddressSanitizer and UBSan
asan: test_sim.asan test_sim-sym.asan sim.asan
	./test_sim.asan
	@echo "--- again, symmetry-reduced ---"
	./test_sim-sym.asan
	./cli_test.sh ./sim.asan

## bench -- solve from the empty board, with and without symmetry reduction
bench: simbench simbench-sym
	@echo "--- plain ---"
	@./simbench
	@echo "--- symmetry reduced ---"
	@./simbench-sym

## check -- what CI runs
check: test cli asan
	@echo
	@echo "all checks passed"

clean:
	rm -rf sim test_sim simbench simbench-sym test_sim-sym *.asan *.o *.dSYM
