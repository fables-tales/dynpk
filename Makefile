CFLAGS :=
LDFLAGS :=

all: libaudit.so wrap

libaudit.so: audit.o
	gcc -shared -Wl,-soname,audit -o libaudit.so audit.o

audit.o: audit.c
	gcc -fPIC -g -c -Wall audit.c -o audit.o

ifneq ($(GLIBC_RPMS),)
LDFLAGS += -L glibc/lib -L glibc/usr/lib
LIBC := glibc/lib/libc.so.6

$(LIBC): $(GLIBC_RPMS)
	rm -rf glibc
	mkdir glibc
	( cd glibc; \
	for r in $(abspath $(GLIBC_RPMS)) ; \
	do \
	    rpm2cpio $$r | cpio -i --make-directories; \
	done ; )
endif

wrap: wrap.c $(LIBC)
	$(CC) $(CFLAGS) $(LDFLAGS) -g -o wrap -static -Wall -Wl,--gc-sections  wrap.c

.PHONY: clean

clean:
	-rm -f *.o *.so wrap
	-rm -rf glibc

