
# NOTE: 'config.mak' is generated by running 'configure'.
include config.mak

CFLAGS = -g -Wall -W -O0 $(CFG_CFLAGS) $(EXTRA_CFLAGS)
LDFLAGS = $(CFG_LIBS) $(EXTRA_LDFLAGS)

# The generated code triggers some harmless warnings which are a bit annoying
# to look at.  (Modifying lemon might be a better solution)
DEFAULT_PARSER_CFLAGS = $(CFLAGS) -Wno-sign-compare -Wno-unused-parameter

# NOTES:
#  CFLAGS/LDFLAGS for coverage test: -fprofile-arcs -ftest-coverage

OBJFILES = \
default-parser.o \
dang-init.o \
dang-main.o \
dang.o \
dang-compile-context.o \
dang-code-position.o \
dang-debug.o \
dang-closure-factory.o \
dang-compile-result.o \
dang-enum.o \
dang-expr.o \
dang-expr-annotations.o \
dang-file.o \
dang-function.o \
dang-builder.o \
dang-function-family.o \
dang-gsl.o \
dang-imports.o \
dang-insn.o \
dang_insn_dump.o \
dang_insn_pack.o \
dang-math.o \
dang-metafunctions.o \
dang-module.o \
dang-namespace.o \
dang-object.o \
dang-parser.o \
dang-run-file.o \
dang-signature.o \
dang-string-functions.o \
dang-struct.o \
dang-template.o \
dang-tensor.o \
dang-thread.o \
dang-token.o \
dang-tokenizer.o \
dang-tree.o \
dang-union.o \
dang-untyped-function.o \
dang-utf8.o \
dang-util.o \
dang-value.o \
dang-value-compile-types.o \
dang-value-function.o \
dang-var-table.o \
dang_builder_compile.o \
dang_function_new_simple_c.o \
dang_function_concat_peek.o \
dang_compile.o \
dang_compile_function_invocation.o \
dang_compile_create_closure.o \
dang_compile_member_access.o \
dang_compile_obey_flags.o \
dang_syntax_check.o

all: dang

dang: $(OBJFILES)
	$(CC) -o $@ $^ $(LDFLAGS)

check: all
	./run-tests

dang-parser.o: default-parser.c default-parser.h
dang-tokenizer.o: multi-char-ops.inc single-char-ops.inc dang-tokenizer.c
default-parser.o: config.h
dang-metafunctions.o: generated-metafunction-table.inc

config.h: configure
	./configure

# generated files
default-parser.c default-parser.h: utils/lemon/lemon default-parser.lemon
	utils/lemon/lemon default-parser.lemon
default-parser.o: default-parser.c
	$(CC) -c $(DEFAULT_PARSER_CFLAGS)  default-parser.c

single-char-ops.inc: utils/make-char-tab
	utils/make-char-tab '{}()[];,' > single-char-ops.inc
multi-char-ops.inc: utils/make-char-tab
	utils/make-char-tab '!@#%%$$^&*<>?/:-=+|~.' > multi-char-ops.inc
generated-metafunction-table.inc: utils/make-mf-tab dang-metafunctions.c
	grep '^DANG_BUILTIN_METAFUNCTION' dang-metafunctions.c | perl -pe 's/.*\(//; s/\).*//;' | LANG=C sort | utils/make-mf-tab > generated-metafunction-table.inc

CLEANFILES = utils/lemon/lemon \
default-parser.c \
default-parser.h \
default-parser.out \
single-char-ops.inc \
multi-char-ops.inc \
generated-metafunction-table.inc \
doc/dang.dvi \
doc/dang.aux \
doc/dang.log \
doc/texput.log \
dang \
$(OBJFILES)

clean:
	rm -f $(CLEANFILES)

VERSION=0.0.0.0.12
distcheck:
	mkdir dang-$(VERSION)
	mkdir dang-$(VERSION)/utils
	mkdir dang-$(VERSION)/utils/lemon
	mkdir dang-$(VERSION)/doc
	mkdir dang-$(VERSION)/tests
	mkdir dang-$(VERSION)/tests/module-path
	cp `cat source-file-list` dang-$(VERSION)
	cp doc/dang.latex dang-$(VERSION)/doc
	cp utils/make-char-tab.c utils/make-mf-tab.c utils/lemon/lempar.c dang-$(VERSION)/utils
	cp utils/lemon/lemon.c utils/lemon/lempar.c dang-$(VERSION)/utils/lemon
	cp tests/*.dang tests/*.dang.output dang-$(VERSION)/tests
	cp tests/module-path/*.dang dang-$(VERSION)/tests/module-path
	tar czfv dang-$(VERSION).tar.gz.tmp dang-$(VERSION)
	cd dang-$(VERSION) && ./configure
	make -C dang-$(VERSION) check
	rm -rf dang-$(VERSION)
	mv dang-$(VERSION).tar.gz.tmp dang-$(VERSION).tar.gz
	@echo "*** Success!!! dang-$(VERSION).tar.gz is ready for the masses!"

# HACK
missing-refs:
	$(MAKE) all 2>&1 | grep 'undefined reference' | sed -e 's/.*`//;' -e "s/'.*//" | sort -u

utils/lemon/lemon: utils/lemon/lemon.c
	$(CC) -o $@ $^
utils/make-char-tab: utils/make-char-tab.c
	$(CC) -o $@ $^
	