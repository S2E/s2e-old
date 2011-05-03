extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <map>

//#define DEBUG

#ifdef DEBUG
#define DBGPRINT(x) (fprintf x)
#else
#define DBGPRINT(x)
#endif

//Type of a translated function, as produced by the
//static translator
typedef void (*x2l_entry_point)(uint64_t *opaque);

/////////////////////////////////////////////////////
//RUNTIME CALL RESOLUTION
//The runtime intercepts all calls to native program counters,
//translates these program counters to the addresses of
//translated LLVM functions, and invokes the translated function.

//List of pointers to translated LLVM functions
extern x2l_entry_point __x2l_g_functionlist_ptr[];

//List of "pointers" to native functions, as defined
//by the original binary
extern uint64_t __x2l_g_functionlist_val[];

//Number of functions in the binary
extern unsigned __x2l_g_functionCount;

//Mapping from a native program counter to the corresponding
//translated LLVM function
typedef std::map<uint64_t, x2l_entry_point> functionmap_t;
static functionmap_t s_functions;

/////////////////////////////////////////////////////



#define STACK_SIZE  0x10000
#define STATE_SIZE 0x11000

extern "C" {
int __main(uint64_t *env);
void __attribute__((noinline)) instruction_marker(uint64_t pc);
void __attribute__((noinline)) call_marker(uint64_t target, int isInlinable, uint64_t *opaque);
void __attribute__((noinline)) return_marker();
uint32_t __attribute__((noinline)) __ldl_mmu(target_ulong addr, int mmu_idx);
void __attribute__((noinline)) __stl_mmu(target_ulong addr, uint32_t val, int mmu_idx);
int __attribute__((noinline)) libc__pthread_create(pthread_t  *  thread, pthread_attr_t * attr, void *
                                                   (*start_routine)(void *), void * arg);

}

static void initialize_function_map(void)
{
    for (unsigned i = 0; i<__x2l_g_functionCount; ++i) {
        s_functions[__x2l_g_functionlist_val[i]] = __x2l_g_functionlist_ptr[i];
    }
}



static void *x2e_findFunction(uint32_t target)
{
    functionmap_t::const_iterator it = s_functions.find(target);
    if (it == s_functions.end()) {
        return NULL;
    }
    return (void*)(*it).second;
}

void instruction_marker(uint64_t pc)
{
    DBGPRINT((stdout, "PC: 0x%llx\n", pc));
}

void call_marker(uint64_t target, int isInlinable, uint64_t *opaque)
{
    CPUState *state = *(CPUState**)opaque;
    if (isInlinable) {
        DBGPRINT((stdout, "Jumping to PC: %#llx\n", target));
    }else {
        DBGPRINT((stdout, "Calling PC: %#llx ESP=%x \n", target, state->regs[R_ESP]));
        functionmap_t::const_iterator it = s_functions.find(target);
        if (it == s_functions.end()) {
            fprintf(stdout, "Called invalid function\n");
            exit(-1);
        }
        (*it).second(opaque);
    }
}

void return_marker()
{
    DBGPRINT((stdout, "Function is returning\n"));
}

uint32_t __ldl_mmu(target_ulong addr, int mmu_idx)
{    
    uint32_t val = *(uint32_t*)addr;
    DBGPRINT((stdout, "Loading address %#x=%#x\n", addr, val));
    return val;
}

void __stl_mmu(target_ulong addr, uint32_t val, int mmu_idx)
{
    DBGPRINT((stdout, "Storing address %#x=%#x\n", addr, val));
    *(uint32_t*)addr = val;
}


/////////////////////////////////////////////////////
//PTHREAD threading support
/////////////////////////////////////////////////////

typedef  void* (*thread_routine_t)(void *);
typedef  void (*transalted_thread_routine_t)(uint64_t *);

typedef struct {
    void *parameter;
    transalted_thread_routine_t routine;
}x2l_thread_t;

//This function wraps the original thread routine.
//It sets up the execution environment for the original thread.
static void *__x2e_thread_routine(void *opaque)
{
    x2l_thread_t *threadParameters = (x2l_thread_t*)opaque;
    uint32_t *stack = new uint32_t[STACK_SIZE];

    //XXX: Ugly hack!
    //Since we compile this module in 32-bit, the compiler thinks the CPUState is smaller...
    //So we allocate it manually, to make it big enough
    CPUState *penv = (CPUState*)malloc(STATE_SIZE);

    stack[STACK_SIZE/sizeof(uint32) - 4] = (uint32_t)threadParameters->parameter;
    stack[STACK_SIZE/sizeof(uint32) - 5] = 0xDEADBEEF; //dummy return address
    penv->regs[R_ESP] = (uint32_t)&stack[STACK_SIZE/sizeof(uint32) - 5];

    threadParameters->routine((uint64_t*)&penv);

    void *ret = (void*)penv->regs[R_EAX];

    delete penv;
    delete [] stack;
    delete threadParameters;

    return ret;
}

int __attribute__((noinline)) libc__pthread_create(pthread_t  *  thread, pthread_attr_t * attr, void *
                                                   (*start_routine)(void *), void * arg) {

    fprintf(stdout, "pthread_create %p %p %p %p\n", thread, attr, start_routine, arg);
    transalted_thread_routine_t x2l_routine =
            (transalted_thread_routine_t)x2e_findFunction((uint64_t)start_routine);

    //XXX: what if the code gives us the pointer to a valid LLVM function
    //(e.g., after relocation/patching...)

    if (!x2l_routine) {
        //The translated code is trying to call arbitrary code...
        fprintf(stdout, "Could not find thread routine %p\n", start_routine);
        exit(-1);
    }

    //Allocate space for the execution environment
    x2l_thread_t *threadDesc = new x2l_thread_t;
    threadDesc->parameter = arg;
    threadDesc->routine = x2l_routine;

    //Create the thread
    int ret = pthread_create(thread, attr, __x2e_thread_routine, threadDesc);
    if (ret) {
        fprintf(stdout, "pthread_create failed: %d\n", ret);
    }
    return ret;
}



extern "C" {

//Defined in the bitcode library.
//XXX: We should move these things to the runtime?
int __attribute__((noinline))  libc__fputs(const char * a, FILE * b);

int main(int argc, char **argv)
{
    CPUState *penv;

    initialize_function_map();

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
    fflush(stdout);
    printf("Main returned %d\n", penv->regs[R_EAX]);

    free(stack);
    free(penv);
    return ret;
}
}
