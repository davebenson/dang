# just so :make works in vi
tests/test-dns-protocol: tests/test-dns-protocol.c libdsk.a
	gcc -W -Wall -g -o $@ $^
libdsk.a: dsk-dns-protocol.o dsk-error.o dsk-object.o dsk-common.o dsk-udp-socket.o \
	  dsk-cmdline.o
	ar cru $@ $^

%.o: %.c
	gcc -W -Wall -g -c -O0 $^

clean:
	rm *.o libdsk.a tests/test-dns-protocol
