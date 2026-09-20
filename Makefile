# acc -- a C compiler for the Agon Light.
#
# Two builds, and the first is how the second is debugged:
#
#   acc      on the host, targeting the eZ80. Where it is developed.
#   acc.bin  built by agondev, to run on the Agon. See Makefile.agon.


CC      ?= cc
CFLAGS  ?= -O2 -g
WARN     = -Wall -Wextra -Wno-unused-parameter
SRC      = src/lex.c src/float.c src/sym.c src/gen.c src/out.c src/parse.c
HDR      = src/acc.h src/timing.h src/ctype.h src/rt_helpers.h

# -fsigned-char because char is signed on the eZ80, so the host build should
# read a source file the same way the target build does.
#
# -fno-sanitize-recover makes undefined behaviour stop the compiler instead of
# printing a line and carrying on, so a test cannot pass over the top of one.
SAN      = -fsanitize=address,undefined -fno-sanitize-recover=all \
           -DACC_CHECK_VSTACK \
           -fsigned-char -O1 -g

BIN = bin

.PHONY: all clean test unit agon
all: $(BIN)/acc

# The runtime helpers are assembly, and the table acc emits them from is
# generated rather than transcribed: getting a byte wrong in a page of opcodes
# is not something review catches. Regenerated only when the assembly is newer
# and agondev is installed -- the header is committed, so a host without the
# toolchain still builds.
AGONDEV ?= $(HOME)/agondev

src/rt_helpers.h: src/rt/helpers.s src/rt/embed.py
	@if [ -x $(AGONDEV)/bin/ez80-none-elf-as ]; then \
	    python3 src/rt/embed.py $< $@ $(AGONDEV)/bin; \
	else \
	    echo "[no agondev: keeping the committed $@]"; touch $@; \
	fi

$(BIN)/acc: $(SRC) $(HDR) | $(BIN)
	$(CC) $(CFLAGS) $(WARN) -Isrc -o $@ $(SRC)

# The compiler is where the bugs are, so the tests drive a sanitized build of
# it rather than a sanitized unit test beside it. It found the use-after-
# realloc in the symbol table that the ordinary build compiled straight past:
# glibc left the freed block readable and the answer came out right, while on
# the Agon the block was reused and 'main' came out undefined.
$(BIN)/acc-asan: $(SRC) $(HDR) | $(BIN)
	$(CC) $(SAN) $(WARN) -Isrc -o $@ $(SRC)

$(BIN):
	@mkdir -p $(BIN)

# unit first: it needs no emulator and no agondev, so it is the part that
# always runs and the part that fails fastest. Then abi.sh, which pins what
# agondev's calling convention actually is -- acc matches it deliberately, and
# a toolchain update that moved it would otherwise show up as a program
# crashing on the Agon. It skips with 77 when agondev is not installed, which
# is not a failure. abi-acc.sh is the other half: it reads the code acc
# generates and checks it against the same table. The differential tests
# cannot do that job -- acc links with nothing, so a convention it gets
# consistently wrong agrees with itself everywhere.
test: all unit $(BIN)/acc-asan agon
	@test/abi.sh || [ $$? -eq 77 ]
	@test/helpers.sh || [ $$? -eq 77 ]
	@test/flags.sh || [ $$? -eq 77 ]
	@test/buffer.sh
	@test/include.sh
	@test/macro.sh
	@test/heap.sh || [ $$? -eq 77 ]
	@test/cycles.sh || [ $$? -eq 77 ]
	@test/abi-acc.sh
	@ACC=$(BIN)/acc-asan test/errors.sh
	@ACC=$(BIN)/acc-asan test/run.sh
	@ACC=$(BIN)/acc-asan test/self.sh
	@if [ -f $(BIN)/acc.bin ]; then test/target.sh || [ $$? -eq 77 ]; \
	 else echo "  [no Agon build: the target test is skipped]"; fi

# The Agon build, through its own makefile so there is one recipe for it.
#
# It is part of `make test` because target.sh is: every other test runs the
# host build, where an int is 32 bits and agondev's clang never sees the
# code. A compare that clang scheduled a flag write in front of made every
# global array with no initial value fail to compile, on the target only,
# with a whole suite passing on the host.
agon:
	@if [ -x $(AGONDEV)/bin/ez80-none-elf-clang ]; then \
	    $(MAKE) -s -f Makefile.agon; \
	else \
	    echo "[no agondev: the Agon build is skipped]"; \
	fi

unit: | $(BIN)
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -o $(BIN)/test_timing test/test_timing.c
	@$(BIN)/test_timing
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -DACC_HASH_STATS -o $(BIN)/test_hash test/test_hash.c src/lex.c src/float.c
	@$(BIN)/test_hash
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -o $(BIN)/test_ctype test/test_ctype.c
	@$(BIN)/test_ctype
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -o $(BIN)/test_out test/test_out.c src/out.c
	@$(BIN)/test_out
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -DACC_HASH_STATS -o $(BIN)/test_sym test/test_sym.c src/sym.c src/lex.c src/float.c
	@$(BIN)/test_sym
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -o $(BIN)/test_float test/test_float.c src/float.c -lm
	@$(BIN)/test_float

clean:
	$(RM) -r $(BIN)
