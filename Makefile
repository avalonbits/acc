# acc -- a C compiler for the Agon Light, ported from tinycc.
#
# Three builds come out of this tree, and they are not alternatives -- the
# first two are how the third is kept honest:
#
#   acc-i386   host compiler, i386 target. Unchanged tinycc semantics, so
#              tinycc's own test suite still applies to it. This is what says
#              whether a change to the shared core broke something.
#   acc        host compiler, eZ80 target. The cross-compiler: eZ80 type
#              sizes and ABI, running where there is memory to debug it.
#   acc.bin    the same compiler built by agondev to run on the Agon itself.
#              Added once the cross-compiler works; see docs/porting-notes.md.

CC       ?= cc
CFLAGS   ?= -O2 -g
WARN      = -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function \
            -Wno-string-plus-int
CPPFLAGS  = -Isrc -DONE_SOURCE=1

BIN = bin
GEN = src/tccdefs_.h

.PHONY: all clean test
all: $(BIN)/acc-i386

$(BIN)/acc: src/tcc.c $(GEN) | $(BIN)
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) -DTCC_TARGET_EZ80 -o $@ $<

$(BIN)/acc-i386: src/tcc.c $(GEN) | $(BIN)
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) -DTCC_TARGET_I386 -o $@ $<

# tccdefs_.h is the built-in macro set, baked into the binary as a string so
# the compiler does not read it off the SD card at startup. c2str is tinycc's
# own converter, built from conftest.c.
$(GEN): include/tccdefs.h src/conftest.c | $(BIN)
	$(CC) -DC2STR src/conftest.c -o $(BIN)/c2str
	$(BIN)/c2str $< $@

$(BIN):
	@mkdir -p $(BIN)

test: all
	@test/run.sh

clean:
	$(RM) -r $(BIN) $(GEN)
