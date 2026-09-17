# acc -- a C compiler for the Agon Light.
#
# Two builds, and the first is how the second is debugged:
#
#   acc      on the host, targeting the eZ80. Where it is developed.
#   acc.bin  built by agondev, to run on the Agon. See Makefile.agon.
#
# The compiler that came before this one lives in old-acc/ and is not built
# from here.

CC      ?= cc
CFLAGS  ?= -O2 -g
WARN     = -Wall -Wextra -Wno-unused-parameter
SRC      = src/lex.c src/sym.c src/gen.c src/out.c src/parse.c
HDR      = src/acc.h

BIN = bin

.PHONY: all clean test
all: $(BIN)/acc

$(BIN)/acc: $(SRC) $(HDR) | $(BIN)
	$(CC) $(CFLAGS) $(WARN) -Isrc -o $@ $(SRC)

$(BIN):
	@mkdir -p $(BIN)

test: all
	@test/run.sh

clean:
	$(RM) -r $(BIN)
