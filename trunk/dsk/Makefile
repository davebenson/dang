# just so :make works in vi
libdsk.a: dsk-dns-protocol.o dsk-error.o dsk-object.o dsk-common.o
	ar cru $@ $^

%.o: %.c
	gcc -g -c -O2 $^

clean:
	rm *.o libdsk.a
