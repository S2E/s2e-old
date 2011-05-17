#include <stdio.h>
#include <pthread.h>

/**
 * Some of the functions are defined in the runtime, but
 * we need to make sure their declaration is available at
 * translate time.
 */

extern "C" {

int __attribute__((noinline))  libc__fputs(const char * a, FILE * b);
int __attribute__((noinline)) libc__pthread_create(pthread_t  *  thread, pthread_attr_t * attr, void *
                                                       (*start_routine)(void *), void * arg);
int __attribute__((noinline)) libc__pthread_join(pthread_t thread, void **value_ptr);


int __attribute__((noinline))  libc__fputs(const char * a, FILE * b)
{
    return fputs(a, b);
}


int __attribute__((noinline)) libc__pthread_join(pthread_t thread, void **value_ptr)
{
    return pthread_join(thread, value_ptr);
}

/* Force the declarations to appear in the bitcode file */
void __dummy_function() {
    libc__pthread_create(NULL, NULL, NULL, NULL);
}


}
