extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <tcg/tcg.h>
#include <tcg/tcg-llvm.h>
}

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define STACK_SIZE  0x10000
#define STATE_SIZE 0x11000

extern "C" {
int __main(uint64_t *env);
void __attribute__((noinline)) instruction_marker(uint64_t pc);
void __attribute__((noinline)) call_marker(uint64_t target, int isInlinable);
void __attribute__((noinline)) return_marker();

}

void instruction_marker(uint64_t pc)
{
    fprintf(stderr, "PC: 0x%llx\n", pc);
}

void call_marker(uint64_t target, int isInlinable)
{
    if (isInlinable) {
        fprintf(stderr, "Jumping to PC: %#llx\n", target);
    }else {
        fprintf(stderr, "Calling PC: %#llx\n", target);
    }
}

void return_marker()
{
    fprintf(stderr, "Function is returning\n");
}

uint32_t __ldl_mmu(target_ulong addr, int mmu_idx)
{
    fprintf(stderr, "Loading address %#x\n", addr);
    return *(uint32_t*)addr;
}

void __stl_mmu(target_ulong addr, uint32_t val, int mmu_idx)
{
    fprintf(stderr, "Storing address %#x=%#x\n", addr, val);
    *(uint32_t*)addr = val;
}


extern "C" {
int main(int argc, char **argv)
{
    CPUState *penv;

    //XXX: Ugly hack!
    //Since we compile this module in 32-bit, the compiler thinks the CPUState is smaller...
    //So we allocate it manually, to make it big enough
    penv = (CPUState*)malloc(STATE_SIZE);

    uint32_t *stack = (uint32_t*)malloc(STACK_SIZE);

    stack[STACK_SIZE/sizeof(uint32) - 3] = (uint32_t)argv;
    stack[STACK_SIZE/sizeof(uint32) - 4] = argc;
    stack[STACK_SIZE/sizeof(uint32) - 5] = 0xDEADBEEF; //dummy return address
    penv->regs[R_ESP] = (uint32_t)&stack[STACK_SIZE/sizeof(uint32) - 5];

    int ret = __main((uint64_t*)&penv);
    printf("Main returned %d\n", penv->regs[R_EAX]);

    free(stack);
    free(penv);
    return ret;
}
}
