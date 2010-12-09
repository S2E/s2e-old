LDFLAGS=-m32
TESTS=empty-main fib simple-global1 inline1


empty-main: empty-main.o
fib: fib.o

simple-global1: simple-global1.c
#	gcc -Wl,-no_compact_linkedit -Wl,-keep_relocs -fno-pic -m32  -O0 -o $@ $<
	gcc -Wl,--emit-relocs -fno-pic -m32  -O0 -o $@ $<
	objdump -S $@ > $@.s

fib:fib.c
	gcc -m32 -o $@ $<

empty-main: empty-main.c
	gcc -m32 -o $@ $<

inline1: inline1.c
	gcc -m32 -o $@ $<
	objdump -S $@ > $@.s

all: $(TESTS)

clean:
	rm -f $(TESTS)
