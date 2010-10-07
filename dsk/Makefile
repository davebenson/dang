# just so :make works in vi

BUILT_SOURCES = dsk-ascii-chartable.inc dsk-digit-chartables.inc \
                dsk-http-ident-chartable.inc dsk-byte-name-table.inc \
		dsk-base64-char-table.inc dsk-base64-value-table.inc \
		dsk-pattern-char-classes.inc
TEST_PROGRAMS = tests/test-dns-protocol tests/test-client-server-0 \
		tests/test-dispatch \
	        tests/test-http-client-stream-0 \
		tests/test-http-server-stream-0 \
		tests/test-http-loopback \
		tests/test-date-0 \
		tests/test-unicode \
		tests/test-checksum \
		tests/test-xml-parser-0 \
		tests/test-xml-validation-suite \
		tests/test-xml-binding-0
PROGRAMS = programs/dsk-dns-lookup programs/dsk-netcat programs/dsk-host \
           programs/dsk-octet-filter programs/dsk-make-xml-binding
all: $(BUILT_SOURCES) $(PROGRAMS) build-tests

build-tests: test-build-dependencies $(TEST_PROGRAMS)

EXTRA_PROGRAMS = programs/make-validation-test-data
extra: $(EXTRA_PROGRAMS)

full: all extra

# For minimum size:
#CC_FLAGS =  -W -Wall -g -Os -DDSK_DEBUG=0 -DDSK_DISABLE_ASSERTIONS=1

# For standard initial size:
CC_FLAGS =  -W -Wall -g -O0 -DDSK_DEBUG=1
LINK_FLAGS = -g -lz

tests/%: tests/%.c libdsk.a
	gcc $(CC_FLAGS) $(LINK_FLAGS) -o $@ $^
programs/%: programs/%.c libdsk.a
	gcc $(CC_FLAGS) $(LINK_FLAGS) -o $@ $^
libdsk.a: dsk-inlines.o \
          dsk-dns-protocol.o dsk-error.o dsk-object.o dsk-common.o dsk-udp-socket.o dsk-dispatch.o dsk-hook.o \
	  dsk-cmdline.o dsk-ip-address.o dsk-dns-client.o dsk-mem-pool.o \
	  dsk-fd.o dsk-octet-io.o dsk-octet-fd.o dsk-octet-pipe.o \
	  dsk-unicode.o dsk-client-stream.o \
	  dsk-buffer.o dsk-main.o dsk-octet-connection.o dsk-octet-listener.o \
	  dsk-octet-filter.o dsk-octet-filter-source.o \
	  dsk-octet-listener-socket.o dsk-cleanup.o \
	  dsk-http-client-stream.o dsk-http-server-stream.o \
	  dsk-memory-source.o dsk-memory-sink.o \
	  dsk-http-internals.o \
	  dsk-http-header-parsing.o dsk-http-protocol.o \
	  dsk-http-header-printing.o \
	  dsk-date.o \
	  dsk-xml-parser.o dsk-xml.o \
	  dsk-xml-binding.o \
	  dsk-xml-binding-parser.o \
	  dsk-zlib.o \
	  dsk-base64.o dsk-base64-encoder.o dsk-base64-decoder.o \
	  dsk-c-quoter.o dsk-c-unquoter.o \
	  dsk-octet-filter-chain.o dsk-octet-filter-identity.o \
	  dsk-hex-encoder.o dsk-hex-decoder.o dsk-whitespace-trimmer.o \
	  dsk-url-encoder.o dsk-url-decoder.o \
	  dsk-byte-doubler.o dsk-byte-undoubler.o \
	  dsk-print.o \
	  dsk-checksum.o \
	  dsk-file-util.o dsk-pattern.o
	ar cru $@ $^

%.o: %.c
	gcc $(CC_FLAGS) -c $^

dsk-ascii-chartable.inc: mk-ascii-chartable.pl
	./mk-ascii-chartable.pl > dsk-ascii-chartable.inc
dsk-digit-chartables.inc: mk-digit-chartables.pl
	./mk-digit-chartables.pl > dsk-digit-chartables.inc
dsk-http-ident-chartable.inc: mk-http-ident-chartable.pl
	./mk-http-ident-chartable.pl > dsk-http-ident-chartable.inc
dsk-byte-name-table.inc: mk-byte-name-table.pl
	./mk-byte-name-table.pl > dsk-byte-name-table.inc
dsk-base64-char-table.inc: mk-base64-to-char-table.pl
	./$^ > $@
dsk-base64-value-table.inc: mk-base64-to-value-table.pl
	./$^ > $@
dsk-pattern-char-classes.inc: mk-pattern-char-classes.pl
	./$^ > $@

tests/generated/xml-binding-test.h tests/generated/xml-binding-test.c: \
	tests/xml-binding-ns/my.test programs/dsk-make-xml-binding
	@test -d tests/generated || mkdir tests/generated
	programs/dsk-make-xml-binding --namespace=my.test \
		--search-dotted=tests/xml-binding-ns \
		--output-basename=tests/generated/xml-binding-test

test-build-dependencies: \
	tests/generated/xml-binding-test.h tests/generated/xml-binding-test.c

clean:
	rm *.o libdsk.a
	rm -f $(TEST_PROGRAMS)
	rm -f $(BUILT_SOURCES)
	rm -f $(PROGRAMS)

check: $(TEST_PROGRAMS)
	@nfail=0; ntest=0; \
	for p in $(TEST_PROGRAMS) ; do \
	  echo "--- $$p ---" ; \
	  if $(TEST_RUN_PREFIX) $$p ; then \
	    echo "$$p SUCCESS" ; \
	  else \
	    { echo "$$p FAIL" ; nfail=$$(($$nfail + 1)) ; } ; \
	  fi; \
	  ntest=$$(($$ntest + 1)) ; \
	done ; \
	if test $$nfail = 0 ; then \
	  echo "Success! $$ntest tests passed." ; \
	else \
	  echo "$$nfail out of $$ntest tests failed." ; \
	fi
check-valgrind:
	$(MAKE) check TEST_RUN_PREFIX='valgrind --show-reachable=yes --leak-check=full --leak-resolution=high'

# HACK
missing-refs:
	$(MAKE) all 2>&1 | grep 'undefined reference' | sed -e 's/.*`//;' -e "s/'.*//" | sort -u
