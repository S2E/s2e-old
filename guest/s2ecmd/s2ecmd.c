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
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#include <s2e.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef WIN32
#include <windows.h>
#endif

typedef void (*cmd_handler_t)(const char **args);

typedef struct _cmd_t {
    char *name;
    cmd_handler_t handler;
    unsigned args_count;
    char *description;
}cmd_t;

void handler_kill(const char **args)
{
    int status = atoi(args[0]);
    const char *message = args[1];
    s2e_kill_state(status, message);
}

void handler_message(const char **args)
{
    s2e_message(args[0]);
}

void handler_wait(const char **args)
{
    s2e_message("Waiting for S2E...");
    while (!s2e_version()) {
#ifdef WIN32
       Sleep(1000);
#else
       sleep(1);
#endif
    }
    s2e_message("Done waiting for S2E.");
}

void handler_symbwrite(const char **args)
{
    int n_bytes = -1;
    int i;

    n_bytes = atoi(args[0]);
    if (n_bytes < 0) {
        fprintf(stderr, "number of bytes may not be negative\n");
        return;
    } else if (n_bytes == 0) {
        return;
    }

    char* buffer = malloc(n_bytes+1);
    memset(buffer, 0, n_bytes + 1);
    s2e_make_symbolic(buffer, n_bytes, "buffer");

    for (i = 0; i < n_bytes; ++i) {
        putchar(buffer[i]);
    }

    free(buffer);

    return;
}


#define COMMAND(c, args, desc) {#c, handler_##c, args, desc}

static cmd_t s_commands[] = {
    COMMAND(kill, 2, "Kill the current state with the specified numeric status and message"),
    COMMAND(message, 1, "Display a message"),
    COMMAND(wait, 0, "Wait for S2E mode"),
    COMMAND(symbwrite, 1, "Write n symbolic bytes to stdout"),
    {NULL, NULL, 0}
};

void print_commands()
{
    unsigned i=0;
    printf("%-15s  %s %s\n\n", "Command name", "Argument count", "Description");
    while(s_commands[i].handler) {
        printf("%-15s  %d              %s\n", s_commands[i].name, s_commands[i].args_count,
                s_commands[i].description);
        ++i;
    }
}

int find_command(const char *cmd)
{
    unsigned i=0;
    while(s_commands[i].handler) {
        if (!strcmp(s_commands[i].name, cmd)) {
            return i;
        }
        ++i;
    }
    return -1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_commands();
        return -1;
    }

    const char *cmd = argv[1];
    int cmd_index = find_command(cmd);

    if (cmd_index == -1) {
        printf("Command %s not found\n", cmd);
        return -1;
    }

    argc -= 2;
    ++argv;
    ++argv;

    if (argc != s_commands[cmd_index].args_count) {
        printf("Invalid number of arguments supplied (%d instead of %d)\n", argc, s_commands[cmd_index].args_count);
        return -1;
    }

    s_commands[cmd_index].handler((const char**)argv);

    return 0;
}
