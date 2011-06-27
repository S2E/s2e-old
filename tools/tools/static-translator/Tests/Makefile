LDFLAGS=-m32
TESTS=empty-main fib simple-global1 inline1 calltester thread1


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

calltester: calltester.c
	gcc -fno-pic -m32 -o $@ $<
	objdump -S $@ > $@.s

lib1: lib1.c
	gcc -mmacosx-version-min=10.4 -read_only_relocs suppress -fno-pic -m32 -o $@ $< 
	objdump -S $@ > $@.s

thread1: thread1.c
	gcc -mmacosx-version-min=10.4 -read_only_relocs suppress -fno-pic -m32 -o $@ $<
	objdump -S $@ > $@.s


inline1: inline1.c
	gcc -m32 -o $@ $<
	objdump -S $@ > $@.s


asm1.bc: asm1.c
	llvm-gcc -m64 -c -emit-llvm -o $@ $<

asm1.ll: asm1.bc
	llvm-dis -f asm1.bc

asm1: asm1.c asm1.bc asm1.ll
	gcc -m64 -o $@ $<

all: $(TESTS)

clean:
	rm -f $(TESTS)
