/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Written by Jonas Wagner <jonas.wagner@epfl.ch>
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <s2e.h>

// ***********************************************
// Helper functions (adapted from klee_init_env.c)
// ***********************************************

static void __emit_error(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

/* Helper function that converts a string to an integer, and
   terminates the program with an error message is the string is not a
   proper number */
static long int __str_to_int(char *s, const char *error_msg) {
    long int res = 0;
    char c;

    if (!*s) __emit_error(error_msg);

    while ((c = *s++)) {
        if (c == '\0') {
            break;
        } else if (c>='0' && c<='9') {
            res = res*10 + (c - '0');
        } else {
            __emit_error(error_msg);
        }
    }
    return res;
}

static int __streq(const char *a, const char *b) {
    while (*a == *b) {
        if (!*a)
            return 1;
        a++;
        b++;
    }
    return 0;
}

static char *__get_sym_str(int numChars, char *name) {
    char *s = malloc(numChars+1);
    s2e_make_symbolic(s, numChars, name);
    s[numChars] = '\0';
    return s;
}

static void __add_arg(int *argc, char **argv, char *arg, int argcMax) {
    if (*argc==argcMax) {
        __emit_error("too many arguments for klee_init_env");
    } else {
        argv[*argc] = arg;
        (*argc)++;
    }
}

// Returns the base name (without directory) of a path
static const char* __base_name(const char* path) {
    const char* lastSlash = strrchr(path, '/');
    if (lastSlash != NULL) {
        return lastSlash + 1;  // The character after the slash
    }
    return path;
}

void __s2e_init_env(int* argcPtr, char*** argvPtr) {
    int argc = *argcPtr;
    char** argv = *argvPtr;

    int new_argc = 0, n_args;
    char* new_argv[1024];
    unsigned max_len, min_argvs, max_argvs;
    char** final_argv;
    char sym_arg_name[5] = "arg";
    unsigned sym_arg_num = 0;
    int k=0, i;
    int concolic_mode = 0;

    sym_arg_name[4] = '\0';

    // Recognize --help when it is the sole argument.
    if (argc == 2 && __streq(argv[1], "--help")) {
        __emit_error("s2e_init_env\n\n\
                     usage: (s2e_init_env) [options] [program arguments]\n\
                         -select-process           - Enable forking in the current process only\n\
                         -select-process-userspace - Enable forking in userspace-code of the\n\
                         current process only\n\
                         -select-process-code      - Enable forking in the code section of the\n\
                         current binary only\n\
                         -concolic                 - Augment existing concrete arguments with symbolic values\n\
                         -sym-arg <N>              - Replace by a symbolic argument with length N\n\
                         -sym-arg <N>              - Replace by a symbolic argument with length N\n\
                         -sym-args <MIN> <MAX> <N> - Replace by at least MIN arguments and at most\n\
                         MAX arguments, each with maximum length N\n\n");
    }

    s2e_enable_forking();

    while (k < argc) {
        if (__streq(argv[k], "--concolic") || __streq(argv[k], "-concolic")) {
            concolic_mode = 1;
            ++k;
        }
        else if (__streq(argv[k], "--sym-arg") || __streq(argv[k], "-sym-arg")) {
            const char *msg = "--sym-arg expects an integer argument <max-len>";
            if (++k == argc)
                __emit_error(msg);

            max_len = __str_to_int(argv[k++], msg);
            sym_arg_name[3] = '0' + sym_arg_num++;
            __add_arg(&new_argc, new_argv,
                      __get_sym_str(max_len, sym_arg_name),
                      1024);
        }
        else if (__streq(argv[k], "--sym-args") || __streq(argv[k], "-sym-args")) {
            const char *msg =
                    "--sym-args expects three integer arguments <min-argvs> <max-argvs> <max-len>";

            if (k+3 >= argc)
                __emit_error(msg);

            k++;
            min_argvs = __str_to_int(argv[k++], msg);
            max_argvs = __str_to_int(argv[k++], msg);
            max_len = __str_to_int(argv[k++], msg);

            n_args = s2e_range(min_argvs, max_argvs+1, "n_args");
            for (i=0; i < n_args; i++) {
                sym_arg_name[3] = '0' + sym_arg_num++;
                __add_arg(&new_argc, new_argv,
                          __get_sym_str(max_len, sym_arg_name),
                          1024);
            }
        }
        else if (__streq(argv[k], "--select-process") || __streq(argv[k], "-select-process")) {
            k++;
            s2e_codeselector_enable_address_space(0);
        }
        else if (__streq(argv[k], "--select-process-userspace") || __streq(argv[k], "-select-process-userspace")) {
            k++;
            s2e_codeselector_enable_address_space(1);
        }
        else if (__streq(argv[k], "--select-process-code") || __streq(argv[k], "-select-process-code")) {
            char name[512];
            uint64_t loadBase, size;

            k++;

            if (s2e_get_module_info(NULL, name, sizeof(name)-1, &loadBase, &size) < 0) {
                __emit_error("s2e_get_module_info: could not load process information\n");
                return;
            }

            s2e_moduleexec_add_module("init_env_module", __base_name(name), 0);
            s2e_codeselector_select_module("init_env_module");
            //XXX: Also figure out the real native base and the address of the entry point.
            s2e_rawmon_loadmodule2(__base_name(name), loadBase, loadBase, 0, size, 0);
        }
        else {
            /* simply copy arguments */
            if (concolic_mode) {
                sym_arg_name[3] = '0' + k;
                s2e_make_concolic(argv[k], strlen(argv[k]), sym_arg_name);
            }
            __add_arg(&new_argc, new_argv, argv[k++], 1024);
        }
    }

    final_argv = (char**) malloc((new_argc+1) * sizeof(*final_argv));
    memcpy(final_argv, new_argv, new_argc * sizeof(*final_argv));
    final_argv[new_argc] = 0;

    *argcPtr = new_argc;
    *argvPtr = final_argv;

    //When we return, forking must be enabled in all three cases
}

// ****************************
// Overriding __libc_start_main
// ****************************

// The type of __libc_start_main
typedef int (*T_libc_start_main)(
        int *(main) (int, char**, char**),
        int argc,
        char ** ubp_av,
        void (*init) (void),
        void (*fini) (void),
        void (*rtld_fini) (void),
        void (*stack_end)
        );

int __libc_start_main(
        int *(main) (int, char **, char **),
        int argc,
        char ** ubp_av,
        void (*init) (void),
        void (*fini) (void),
        void (*rtld_fini) (void),
        void *stack_end)
        __attribute__ ((noreturn));

int __libc_start_main(
        int *(main) (int, char **, char **),
        int argc,
        char ** ubp_av,
        void (*init) (void),
        void (*fini) (void),
        void (*rtld_fini) (void),
        void *stack_end) {

    __s2e_init_env(&argc, &ubp_av);

    T_libc_start_main orig_libc_start_main = (T_libc_start_main)dlsym(RTLD_NEXT, "__libc_start_main");
    (*orig_libc_start_main)(main, argc, ubp_av, init, fini, rtld_fini, stack_end);

    exit(1); // This is never reached
}
