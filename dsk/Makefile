# just so :make works in vi
all: tests/test-dns-protocol programs/dsk-dns-lookup
tests/test-dns-protocol: tests/test-dns-protocol.c libdsk.a
	gcc -W -Wall -g -o $@ $^
programs/dsk-dns-lookup: programs/dsk-dns-lookup.c libdsk.a
	gcc -W -Wall -g -o $@ $^
libdsk.a: dsk-dns-protocol.o dsk-error.o dsk-object.o dsk-common.o dsk-udp-socket.o \
	  dsk-cmdline.o
	ar cru $@ $^

%.o: %.c
	gcc -W -Wall -g -c -O0 $^

clean:
	rm *.o libdsk.a tests/test-dns-protocol
