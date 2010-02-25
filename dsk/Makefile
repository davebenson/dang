# just so :make works in vi
libdsk.a: dsk-dns-protocol.o dsk-error.o dsk-object.o dsk-common.o
	ar cru $@ $^

%.o: %.c
	gcc -c -O2 $^
