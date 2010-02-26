# just so :make works in vi
tests/test-dns-protocol: tests/test-dns-protocol.c libdsk.a
	gcc -g -o $@ $^
libdsk.a: dsk-dns-protocol.o dsk-error.o dsk-object.o dsk-common.o
	ar cru $@ $^

%.o: %.c
	gcc -g -c -O0 $^

clean:
	rm *.o libdsk.a tests/test-dns-protocol
