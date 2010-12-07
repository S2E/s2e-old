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
void instruction_marker(uint64_t pc);
}

void instruction_marker(uint64_t pc)
{
    fprintf(stderr, "PC: 0x%llx\n", pc);
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
