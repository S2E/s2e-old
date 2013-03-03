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
 * All contributors are listed in the S2E-AUTHORS file.
 */

#include <s2e.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef WIN32
#include <windows.h>
#define SLEEP(x) Sleep((x) * 1000)
#else
#define SLEEP(x) sleep(x)
#endif

typedef void (*cmd_handler_t)(const char **args);

typedef struct _cmd_t {
    char *name;
    cmd_handler_t handler;
    unsigned args_count;
    char *description;
} cmd_t;

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
       SLEEP(1);
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

    char *buffer = calloc(1, n_bytes + 1);
    s2e_make_symbolic(buffer, n_bytes, "buffer");

    for (i = 0; i < n_bytes; ++i) {
        putchar(buffer[i]);
    }

    free(buffer);

    return;
}

void handler_symbfile(const char **args)
{
    const char *filename = args[0];

    int fd = open(filename, O_RDWR);
    if (fd < 0) {
        s2e_kill_state_printf(-1, "symbfile: could not open %s\n", filename);
        return;
    }

    /* Determine the size of the file */
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        s2e_kill_state_printf(-1, "symbfile: could not determine the size of %s\n", filename);
        return;
    }

    char buffer[0x1000];

    unsigned current_chunk = 0;
    unsigned total_chunks = size / sizeof(buffer);
    if (size % sizeof(buffer)) {
        ++total_chunks;
    }

    /**
     * Replace slashes in the filename with underscores.
     * It should make it easier for plugins to generate
     * concrete files, while preserving info about the original path
     * and without having to deal with the slashes.
     **/
    char cleaned_name[512];
    strncpy(cleaned_name, filename, sizeof(cleaned_name));
    for (unsigned i = 0; cleaned_name[i]; ++i) {
        if (cleaned_name[i] == '/') {
            cleaned_name[i] = '_';
        }
    }

    off_t offset = 0;
    do {
        /* Read the file in chunks of 4K and make them concolic */
        char symbvarname[512];

        if (lseek(fd, offset, SEEK_SET) < 0) {
            s2e_kill_state_printf(-1, "symbfile: could not seek to position %d", offset);
            return;
        }

        ssize_t totransfer = size > sizeof(buffer) ? sizeof(buffer) : size;

        /* Read the data */
        ssize_t read_count = read(fd, buffer, totransfer);
        if (read_count < 0) {
            s2e_kill_state_printf(-1, "symbfile: could not read from file %s", filename);
            return;
        }

        /**
         * Make the buffer concolic.
         * The symbolic variable name encodes the original file name with its path
         * as well as the chunk id contained in the buffer.
         * A test case generator should therefore be able to reconstruct concrete
         * files easily.
         */
        snprintf(symbvarname, sizeof(symbvarname), "__symfile___%s___%d_%d_symfile__",
                 cleaned_name, current_chunk, total_chunks);
        s2e_make_concolic(buffer, read_count, symbvarname);

        /* Write it back */
        if (lseek(fd, offset, SEEK_SET) < 0) {
            s2e_kill_state_printf(-1, "symbfile: could not seek to position %d", offset);
            return;
        }

        ssize_t written_count = write(fd, buffer, read_count);
        if (written_count < 0) {
            s2e_kill_state_printf(-1, "symbfile: could not write to file %s", filename);
            return;
        }

        if (read_count != written_count) {
            /* XXX: should probably retry...*/
            s2e_kill_state_printf(-1, "symbfile: could not write the read amount");
            return;
        }

        offset += read_count;
        size -= read_count;
        ++current_chunk;

    } while (size > 0);

    close(fd);
}

void handler_exemplify(const char **args)
{
    const unsigned int BUF_SIZE = 32;
    char buffer[BUF_SIZE];
    unsigned int i;
    memset(buffer, 0, sizeof(buffer));

    while (fgets(buffer, sizeof(buffer), stdin)) {
        for (i = 0; i < BUF_SIZE; ++i) {
            if (s2e_is_symbolic(&buffer[i], sizeof(buffer[i]))) {
                s2e_get_example(&buffer[i], sizeof(buffer[i]));
            }
        }

        fputs(buffer, stdout);
        memset(buffer, 0, sizeof(buffer));
    }
}


#define COMMAND(c, args, desc) { #c, handler_##c, args, desc }

static cmd_t s_commands[] = {
    COMMAND(kill, 2, "Kill the current state with the specified numeric status and message"),
    COMMAND(message, 1, "Display a message"),
    COMMAND(wait, 0, "Wait for S2E mode"),
    COMMAND(symbwrite, 1, "Write n symbolic bytes to stdout"),
    COMMAND(symbfile, 1, "Makes the specified file concolic. The file should be stored in a ramdisk."),
    COMMAND(exemplify, 0, "Read from stdin and write an example to stdout"),
    { NULL, NULL, 0 }
};

void print_commands(void)
{
    unsigned i = 0;
    printf("%-15s  %s %s\n\n", "Command name", "Argument count", "Description");
    while(s_commands[i].handler) {
        printf("%-15s  %d              %s\n", s_commands[i].name,
               s_commands[i].args_count, s_commands[i].description);
        ++i;
    }
}

int find_command(const char *cmd)
{
    unsigned i = 0;
    while(s_commands[i].handler) {
        if (!strcmp(s_commands[i].name, cmd)) {
            return i;
        }
        ++i;
    }
    return -1;
}

int main(int argc, const char **argv)
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
        printf("Invalid number of arguments supplied (%d instead of %d)\n",
               argc, s_commands[cmd_index].args_count);
        return -1;
    }

    s_commands[cmd_index].handler(argv);

    return 0;
}
