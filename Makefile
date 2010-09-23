all: libaudit.so wrap

libaudit.so: audit.o
	gcc -shared -Wl,-soname,audit -o libaudit.so audit.o

audit.o: audit.c
	gcc -fPIC -g -c -Wall audit.c -o audit.o

wrap: wrap.c
	$(CC) -g -o wrap -static -Wall -Wl,--gc-sections  wrap.c

.PHONY: clean

clean:
	-rm -f *.o *.so wrap


