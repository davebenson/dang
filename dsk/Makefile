# just so :make works in vi

BUILT_SOURCES = dsk-ascii-chartable.inc
TEST_PROGRAMS = tests/test-dns-protocol
PROGRAMS = programs/dsk-dns-lookup programs/dsk-netcat
all: $(BUILT_SOURCES) $(TEST_PROGRAMS) $(PROGRAMS)

tests/test-dns-protocol: tests/test-dns-protocol.c libdsk.a
	gcc -W -Wall -g -o $@ $^
programs/%: programs/%.c libdsk.a
	gcc -W -Wall -g -o $@ $^
libdsk.a: dsk-dns-protocol.o dsk-error.o dsk-object.o dsk-common.o dsk-udp-socket.o dsk-dispatch.o dsk-hook.o \
	  dsk-cmdline.o dsk-ip-address.o dsk-dns-client.o dsk-mem-pool.o \
	  dsk-fd.o dsk-unicode.o dsk-client-stream.o
	ar cru $@ $^

%.o: %.c
	gcc -W -Wall -g -c -O0 $^

dsk-ascii-chartable.inc: mk-ascii-chartable.pl
	./mk-ascii-chartable.pl > dsk-ascii-chartable.inc

clean:
	rm *.o libdsk.a tests/test-dns-protocol

# HACK
missing-refs:
	$(MAKE) all 2>&1 | grep 'undefined reference' | sed -e 's/.*`//;' -e "s/'.*//" | sort -u
