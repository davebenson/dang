# just so :make works in vi

BUILT_SOURCES = dsk-ascii-chartable.inc dsk-digit-chartables.inc \
                dsk-http-ident-chartable.inc dsk-byte-name-table.inc
TEST_PROGRAMS = tests/test-dns-protocol tests/test-client-server-0 \
	        tests/test-http-client-stream-0 tests/test-xml-parser-0
PROGRAMS = programs/dsk-dns-lookup programs/dsk-netcat programs/dsk-host
all: $(BUILT_SOURCES) $(TEST_PROGRAMS) $(PROGRAMS)

tests/%: tests/%.c libdsk.a
	gcc -W -Wall -g -o $@ $^
programs/%: programs/%.c libdsk.a
	gcc -W -Wall -g -o $@ $^
libdsk.a: dsk-dns-protocol.o dsk-error.o dsk-object.o dsk-common.o dsk-udp-socket.o dsk-dispatch.o dsk-hook.o \
	  dsk-cmdline.o dsk-ip-address.o dsk-dns-client.o dsk-mem-pool.o \
	  dsk-fd.o dsk-octet-io.o dsk-octet-fd.o dsk-unicode.o dsk-client-stream.o \
	  dsk-buffer.o dsk-main.o dsk-octet-connection.o dsk-octet-listener.o \
	  dsk-octet-listener-socket.o dsk-cleanup.o \
	  dsk-http-client-stream.o dsk-memory-source.o dsk-memory-sink.o \
	  dsk-http-header-parsing.o dsk-http-protocol.o \
	  dsk-http-header-printing.o \
	  dsk-date.o \
	  dsk-xml-parser.o dsk-xml.o \
	  dsk-print.o
	ar cru $@ $^

%.o: %.c
	gcc -W -Wall -g -c -O0 $^

dsk-ascii-chartable.inc: mk-ascii-chartable.pl
	./mk-ascii-chartable.pl > dsk-ascii-chartable.inc
dsk-digit-chartables.inc: mk-digit-chartables.pl
	./mk-digit-chartables.pl > dsk-digit-chartables.inc
dsk-http-ident-chartable.inc: mk-http-ident-chartable.pl
	./mk-http-ident-chartable.pl > dsk-http-ident-chartable.inc
dsk-byte-name-table.inc: mk-byte-name-table.pl
	./mk-byte-name-table.pl > dsk-byte-name-table.inc


clean:
	rm *.o libdsk.a tests/test-dns-protocol

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
