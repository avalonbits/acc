# acc -- a C compiler for the Agon Light.
#
# Two builds, and the first is how the second is debugged:
#
#   acc      on the host, targeting the eZ80. Where it is developed.
#   acc.bin  built by agondev, to run on the Agon. See Makefile.agon.


CC      ?= cc
CFLAGS  ?= -O2 -g
WARN     = -Wall -Wextra -Wno-unused-parameter
LEX_SRC  = src/names.c src/source.c src/macro.c src/directive.c src/lex.c
SRC      = src/obj.c src/archive.c src/names.c src/source.c src/macro.c src/directive.c src/lex.c src/float.c src/sym.c src/insn.c src/vstack.c src/arith.c src/wide.c src/branch.c src/runtime.c src/finish.c src/relax.c src/func.c src/lvalue.c src/image.c src/reloc.c src/diag.c src/expr.c src/inline.c src/type.c src/init.c src/decl.c src/stmt.c src/link.c src/main.c \
           src/fmt.c
HDR      = src/acc.h src/types.h src/diag.h src/names.h src/lex.h src/sym.h src/gen.h src/out.h src/obj.h src/lex_int.h src/out_int.h src/obj_int.h src/gen_int.h src/parse_int.h src/timing.h src/ctype.h src/runtime.h src/version.h src/fmt.h

# -fsigned-char because char is signed on the eZ80, so the host build should
# read a source file the same way the target build does.
#
# -fno-sanitize-recover makes undefined behaviour stop the compiler instead of
# printing a line and carrying on, so a test cannot pass over the top of one.
SAN      = -fsanitize=address,undefined -fno-sanitize-recover=all \
           -DACC_CHECK_VSTACK \
           -fsigned-char -O1 -g

BIN = bin

# The library acc links programs against, written in C and built by acc
# itself -- which is what makes it something the Agon can build for itself,
# and what keeps it honest: every line of it is a line acc has to compile.
LIBSRC = lib/str.c lib/strdup.c lib/stdio.c lib/printf.c lib/scanf.c lib/printf_float.c \
         lib/abort.c lib/exit.c lib/assert.c lib/ctype.c \
         lib/math.c lib/mathround.c lib/mathscale.c lib/mathfmod.c \
         lib/sqrt.c lib/cbrt.c lib/hypot.c \
         lib/exp.c lib/log.c lib/pow.c \
         lib/trig.c lib/atan.c lib/mathhyp.c lib/erf.c lib/gamma.c lib/fma.c \
         lib/file.c \
         lib/stdlib.c lib/stdlib2.c lib/atexit.c lib/strtol.c lib/strtod.c lib/errno.c lib/fenv.c lib/locale.c lib/signal.c lib/wctype.c lib/inttypes.c lib/wcs.c lib/mb.c lib/wcsto.c lib/wfile.c lib/wprintf.c lib/wscanf.c lib/qsort.c lib/mos.c lib/vdp_screen.c lib/vdp_graphics.c lib/vdp_bitmap.c lib/vdp_buffer.c lib/vdp_audio.c lib/keyboard.c lib/gpio.c lib/timer.c \
         lib/time.c lib/strftime.c
LIBHDR = include/stddef.h include/string.h include/stdio.h include/stdint.h \
         include/stdbool.h include/stdlib.h include/time.h \
         include/ctype.h include/assert.h include/math.h \
         include/errno.h include/limits.h include/fenv.h include/locale.h include/setjmp.h \
         include/signal.h include/wctype.h include/inttypes.h include/tgmath.h \
         include/wchar.h \
         include/agon/mos.h include/agon/vdp.h include/agon/keyboard.h \
         include/agon/gpio.h include/agon/joystick.h include/agon/timer.h \
         include/ez80f92.h include/agon/vdp/screen.h include/agon/vdp/graphics.h \
         include/agon/vdp/bitmap.h include/agon/vdp/buffer.h include/agon/vdp/audio.h \
         lib/vdp_emit.h
LIBOBJ = $(LIBSRC:lib/%.c=$(BIN)/lib/%.o)

# And what of the library is eZ80 assembly, assembled by zap as the runtime
# is (below): <string.h>, whose functions the block instructions do in a
# fifth of the time C takes, but for strtok, strcoll and strxfrm.
LIBASM    = lib/strlen.s lib/strcmp.s lib/strchr.s lib/strcpy.s lib/strncpy.s \
            lib/strncat.s lib/strstr.s lib/strspn.s lib/strncasecmp.s lib/mem.s
LIBASMOBJ = $(LIBASM:lib/%.s=$(BIN)/lib/%.o)

.PHONY: all clean test unit agon

# acc-asan is part of the ordinary build and not only of `test`, because
# every scripted test defaults to running it and none of them rebuild it.
# Left out, `make && ACC=bin/acc-asan test/relax.sh` runs whatever the last
# full `make test` happened to leave behind -- a compiler without the change
# under test, reporting green. It is worst when checking that a new test
# bites: breaking the line it covers and watching the test still pass reads
# as "the test does not cover this" when the truth is "that binary is old".
all: $(BIN)/acc $(BIN)/acc-asan $(BIN)/opt-acc $(BIN)/opt-acc-asan $(BIN)/libc.a

# acc's runtime is eZ80 assembly, lib/rt/*.s, assembled by zap into acc's
# object format and put in the library with the C: see lib/rt/README.md.
# zap is built here from its source, a checkout of
# https://github.com/avalonbits/zap -- ZAP_SRC, or ZAP for a zap already built.
ZAP_SRC ?= $(HOME)/code/zap
ZAP     ?= $(BIN)/zap
ZAPSRCS  = $(addprefix $(ZAP_SRC)/src/,zap.c symtab.c scan.c expr.c macro.c \
             directive.c insn.c object.c buf_reader.c value.c conv.c isa_table.c) \
           $(ZAP_SRC)/test/stubs/agon_stubs.c

$(BIN)/zap: | $(BIN)
	@[ -f $(ZAP_SRC)/src/zap.c ] || { echo "no zap source at $(ZAP_SRC): set ZAP_SRC, or ZAP" >&2; exit 1; }
	$(CC) -std=gnu11 -O2 -fsigned-char -w -include $(ZAP_SRC)/test/stubs/host_types.h \
	    -I$(ZAP_SRC)/src -I$(ZAP_SRC)/test/stubs -o $@ $(ZAPSRCS)

RTSRC = $(wildcard lib/rt/*.s)
RTOBJ = $(RTSRC:lib/rt/%.s=$(BIN)/lib/rt_%.o)

$(BIN)/lib/rt_%.o: lib/rt/%.s $(ZAP) | $(BIN)/lib
	@$(ZAP) $< $@ -f acc >/dev/null || { $(ZAP) $< $@ -f acc; exit 1; }

$(LIBASMOBJ): $(BIN)/lib/%.o: lib/%.s $(ZAP) | $(BIN)/lib
	@$(ZAP) $< $@ -f acc >/dev/null || { $(ZAP) $< $@ -f acc; exit 1; }

# What this acc is, so that an object can say which compiler made it. The
# rule runs on every build -- a checksum of a few hundred kilobytes is
# nothing -- and only writes the header when the answer changes, so that
# nothing is recompiled for a file that was touched and not edited.
src/acc_build.h: $(SRC) $(HDR) src/build_id.sh
	@id=$$(src/build_id.sh 2>/dev/null) || id=0; [ -n "$$id" ] || id=0; \
	 printf '/* Generated by src/build_id.sh. Do not edit. */\n#define ACC_BUILD %s\n' \
	     "$$id" > $@.tmp
	@cmp -s $@.tmp $@ || mv $@.tmp $@
	@$(RM) $@.tmp

# The library a program is linked with unless a link names others first, as
# /lib/acc/libc.a is on the Agon: the one built here, with the runtime beside
# it. And the headers an #include finds after every -I, as /lib/acc/include
# is on the Agon: the ones here.
HOSTLIB = -DACC_LIBC='"$(CURDIR)/$(BIN)/libc.a"' \
          -DACC_INCLUDE_DIR='"$(CURDIR)/include"'

$(BIN)/acc: $(SRC) $(HDR) src/acc_build.h | $(BIN)
	$(CC) $(CFLAGS) $(WARN) $(HOSTLIB) -Isrc -o $@ $(SRC)

# opt-acc: the same sources with its own passes turned on, for the host only
# (docs/optimizer-plan.md). Makefile.agon never sees OPT_SRC.
OPT_SRC = src/prescan.c src/genlog.c src/ssa.c

# The wrappers genlog.c logs the parser's calls through, from gen.h.
src/genlog_calls.h: src/gen.h src/genlog.py
	@python3 src/genlog.py src/gen.h $@
$(BIN)/opt-acc: $(SRC) $(OPT_SRC) $(HDR) src/genlog.h src/acc_build.h src/genlog_calls.h | $(BIN)
	$(CC) $(CFLAGS) $(WARN) $(HOSTLIB) -DOPT_ACC -Isrc -o $@ $(SRC) $(OPT_SRC)

$(BIN)/lib:
	@mkdir -p $@

$(BIN)/lib/%.o: lib/%.c $(LIBHDR) $(BIN)/acc | $(BIN)/lib
	@$(BIN)/acc -c $< -o $@ -Iinclude >/dev/null

# Which objects the library is made of, in a file that is written only when
# the list changes. A member taken out of LIBSRC leaves every remaining
# prerequisite as old as it was, so without this the library would keep the
# member that is gone. Its recipe runs every time; the library is remade
# only when the file it writes is new.
$(BIN)/lib/members: FORCE | $(BIN)/lib
	@echo '$(LIBOBJ) $(LIBASMOBJ) $(RTOBJ)' | cmp -s - $@ || echo '$(LIBOBJ) $(LIBASMOBJ) $(RTOBJ)' > $@

FORCE:

# The objects of members that are gone are removed with them, so that what
# is in $(BIN)/lib is what is in the library -- test/lib.sh builds libraries
# of its own from that directory.
#
# The runtime is a library of its own beside it, rt.a, which a link reads
# first: every program calls the runtime, and its index is a few names where
# the C library's is a thousand, so a program that calls nothing in the C
# library does not read that one at all.
$(BIN)/libc.a: $(LIBOBJ) $(LIBASMOBJ) $(RTOBJ) $(BIN)/lib/members $(BIN)/acc
	@$(RM) $(filter-out $(LIBOBJ) $(LIBASMOBJ) $(RTOBJ),$(wildcard $(BIN)/lib/*.o))
	@$(BIN)/acc -a $@ $(LIBOBJ) $(LIBASMOBJ) >/dev/null
	@$(BIN)/acc -a $(BIN)/rt.a $(RTOBJ) >/dev/null
	@echo "[$@: $$(stat -c%s $@) bytes from $(words $(LIBOBJ) $(LIBASMOBJ)) objects]"
	@echo "[$(BIN)/rt.a: $$(stat -c%s $(BIN)/rt.a) bytes from $(words $(RTOBJ)) objects]"

# The compiler is where the bugs are, so the tests drive a sanitized build of
# it rather than a sanitized unit test beside it. It found the use-after-
# realloc in the symbol table that the ordinary build compiled straight past:
# glibc left the freed block readable and the answer came out right, while on
# the Agon the block was reused and 'main' came out undefined. Built by `all`:
# see the note there.
$(BIN)/acc-asan: $(SRC) $(HDR) | $(BIN)
	$(CC) $(SAN) $(WARN) $(HOSTLIB) -Isrc -o $@ $(SRC)

$(BIN)/opt-acc-asan: $(SRC) $(OPT_SRC) $(HDR) src/genlog.h src/genlog_calls.h | $(BIN)
	$(CC) $(SAN) $(WARN) $(HOSTLIB) -DOPT_ACC -Isrc -o $@ $(SRC) $(OPT_SRC)

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
test: all unit agon
	@test/abi.sh || [ $$? -eq 77 ]
	@test/helpers.sh || [ $$? -eq 77 ]
	@test/fmt_agon.sh || [ $$? -eq 77 ]
	@test/frames.sh || [ $$? -eq 77 ]
	@test/budget.sh || [ $$? -eq 77 ]
	@test/msgpack.sh
	@test/forget.sh
	@test/linkheap.sh
	@test/linkstream.sh
	@test/linkfixups.sh
	@test/manyargs.sh || [ $$? -eq 77 ]
	@test/abandon.sh
	@test/objmem.sh
	@test/spill.sh
	@test/startup.sh || [ $$? -eq 77 ]
	@test/agonpp.sh || [ $$? -eq 77 ]
	@test/flags.sh || [ $$? -eq 77 ]
	@test/keyboard.sh || [ $$? -eq 77 ]
	@test/buffer.sh
	@test/include.sh
	@ACC=$(BIN)/acc-asan test/macro.sh
	@test/build.sh
	@ACC=$(BIN)/acc-asan test/cpp89.sh
	@ACC=$(BIN)/acc-asan test/reloc.sh
	@ACC=$(BIN)/acc-asan test/object.sh
	@ACC=$(BIN)/acc-asan test/accobj.sh
	@ACC=$(BIN)/acc-asan test/bss.sh
	@ACC=$(BIN)/acc-asan test/dead.sh
	@ACC=$(BIN)/acc-asan test/branch.sh
	@ACC=$(BIN)/acc-asan test/codegen.sh
	@ACC=$(BIN)/acc-asan test/relax.sh
	@ACC=$(BIN)/acc-asan test/mos.sh || [ $$? -eq 77 ]
	@ACC=$(BIN)/acc-asan test/args.sh || [ $$? -eq 77 ]
	@ACC=$(BIN)/acc-asan test/lib.sh
	@test/defaults.sh
	@ACC=$(BIN)/acc-asan test/onestep.sh
	@ACC=$(BIN)/acc-asan test/usage.sh
	@test/conformance.sh --check || [ $$? -eq 77 ]
	@ACC=$(BIN)/opt-acc test/conformance.sh --check || [ $$? -eq 77 ]
	@ACC=$(BIN)/acc-asan test/printf.sh || [ $$? -eq 77 ]
	@ACC=$(BIN)/acc-asan test/floatrt.sh || [ $$? -eq 77 ]
	@ACC=$(BIN)/acc-asan test/hosted.sh || [ $$? -eq 77 ]
	@ACC=$(BIN)/acc-asan test/agonlib.sh || [ $$? -eq 77 ]
	@ACC=$(BIN)/acc-asan test/vdpreal.sh || [ $$? -eq 77 ]
	@test/heap.sh || [ $$? -eq 77 ]
	@test/cycles.sh || [ $$? -eq 77 ]
	@test/abi-acc.sh
	@ACC=$(BIN)/acc-asan test/errors.sh
	@ACC=$(BIN)/acc-asan test/run.sh
	@test/optacc.sh
	@ACC=$(BIN)/opt-acc-asan test/run.sh
	@ACC=$(BIN)/opt-acc-asan test/floatrt.sh || [ $$? -eq 77 ]
	@ACC=$(BIN)/opt-acc-asan test/printf.sh || [ $$? -eq 77 ]
	@OPTACC_SSA=1 ACC=$(BIN)/opt-acc-asan test/run.sh
	@OPTACC_SSA=1 ACC=$(BIN)/opt-acc-asan test/floatrt.sh || [ $$? -eq 77 ]
	@OPTACC_SSA=1 ACC=$(BIN)/opt-acc-asan test/printf.sh || [ $$? -eq 77 ]
	@OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_HOMES=2 ACC=$(BIN)/opt-acc-asan test/run.sh
	@ACC=$(BIN)/acc-asan test/self.sh
	@ACC=$(BIN)/acc-asan test/selfbuild.sh
	@if [ -f $(BIN)/acc.bin ]; then test/target.sh || [ $$? -eq 77 ]; \
	 else echo "  [no Agon build: the target test is skipped]"; fi
	@if [ -f $(BIN)/acc.bin ]; then test/release.sh || [ $$? -eq 77 ]; \
	 else echo "  [no Agon build: the release test is skipped]"; fi
	@if [ -f $(BIN)/acc.bin ]; then test/headers.sh || [ $$? -eq 77 ]; \
	 else echo "  [no Agon build: the headers test is skipped]"; fi

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
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -DACC_HASH_STATS -o $(BIN)/test_hash test/test_hash.c $(LEX_SRC) src/float.c
	@$(BIN)/test_hash
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -o $(BIN)/test_ctype test/test_ctype.c
	@$(BIN)/test_ctype
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -o $(BIN)/test_out test/test_out.c src/image.c src/reloc.c -Wl,--wrap=realloc
	@$(BIN)/test_out
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -DACC_HASH_STATS -o $(BIN)/test_sym test/test_sym.c src/sym.c $(LEX_SRC) src/float.c
	@$(BIN)/test_sym
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -o $(BIN)/test_float test/test_float.c src/float.c -lm
	@$(BIN)/test_float
	@$(CC) $(SAN) $(WARN) -Isrc -Itest -o $(BIN)/test_fmt test/test_fmt.c src/fmt.c
	@$(BIN)/test_fmt

clean:
	$(RM) -r $(BIN)
	$(RM) src/acc_build.h
