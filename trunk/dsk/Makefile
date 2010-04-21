# just so :make works in vi
all: dsk-ascii-chartable.inc tests/test-dns-protocol programs/dsk-dns-lookup
tests/test-dns-protocol: tests/test-dns-protocol.c libdsk.a
	gcc -W -Wall -g -o $@ $^
programs/dsk-dns-lookup: programs/dsk-dns-lookup.c libdsk.a
	gcc -W -Wall -g -o $@ $^
libdsk.a: dsk-dns-protocol.o dsk-error.o dsk-object.o dsk-common.o dsk-udp-socket.o dsk-dispatch.o dsk-hook.o \
	  dsk-cmdline.o dsk-ip-address.o dsk-dns-client.o dsk-mem-pool.o \
	  dsk-fd.o dsk-unicode.o
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
