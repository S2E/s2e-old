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
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "s2e.h"


const char *g_target_dir = NULL;
const char *g_file = NULL;

/* file is a path relative to the HostFile's base directory */
int copy_file(const char *directory, const char *guest_file)
{
    char *path = malloc(strlen(directory) + strlen(guest_file) + 1 + 1);
    if (!path) {
        fprintf(stderr, "Could not allocate memory for file path\n");
        exit(1);
    }

    const char *file = basename((char*)guest_file);
    if (!file) {
        fprintf(stderr, "Could not allocate memory for file basename\n");
        exit(1);
    }

    sprintf(path, "%s/%s", directory, file);

    unlink(path);

    if (mkdir(directory, S_IRWXU)<0 && (errno != EEXIST)) {
        fprintf(stderr, "Could not create directory %s (%s)\n", directory,
                strerror(errno));
        exit(-1);
    }

#ifdef _WIN32
    int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRWXU);
#else
    int fd = creat(path, S_IRWXU);
#endif

    if(fd == -1) {
        fprintf(stderr, "cannot create file %s\n", path);
        exit(1);
    }

    int s2e_fd = s2e_open(guest_file);
    if(s2e_fd == -1) {
        fprintf(stderr, "s2e_open of %s failed\n", guest_file);
        exit(1);
    }

    int fsize = 0;
    char buf[1024*64];
    memset(buf, 0, sizeof(buf));

    while(1) {
        int ret = s2e_read(s2e_fd, buf, sizeof(buf));
        if(ret == -1) {
            fprintf(stderr, "s2e_read failed\n");
            exit(1);
        } else if(ret == 0) {
            break;
        }

        int ret1 = write(fd, buf, ret);
        if(ret1 != ret) {
            fprintf(stderr, "can not write to file\n");
            exit(1);
        }

        fsize += ret;
    }

    printf("... file %s of size %d was transfered successfully\n",
            file, fsize);

    s2e_close(s2e_fd);
    close(fd);
    free(path);

    return 0;
}

int parse_arguments(int argc, const char **argv)
{
    unsigned i = 1;
    while(i < argc) {
        if (!strcmp(argv[i], "--target-dir")) {
            if (++i >= argc) { return -1; }
            g_target_dir = argv[i++];
            continue;
        } else {
            g_file = argv[i++];
        }
    }

    return 0;
}

int validate_arguments()
{
    if (!g_target_dir) {
        g_target_dir = getcwd(NULL, 0);
        if (!g_target_dir) {
            fprintf(stderr, "Could not allocate memory for current directory\n");
            exit(1);
        }
    }

    if (!g_file) {
        return -1;
    }

    return 0;
}

void print_usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s [options] file_name\n\n", prog_name);

    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --target-dir : where to place the downloaded file [default: working directory]\n");
}

int main(int argc, const char** argv)
{
    if(parse_arguments(argc, argv) < 0) {
        print_usage(argv[0]);
        exit(1);
    }

    if(validate_arguments() < 0) {
        print_usage(argv[0]);
        exit(1);
    }


    printf("Waiting for S2E mode...\n");
    while(s2e_version() == 0) /* nothing */;
    printf("... S2E mode detected\n");

    copy_file(g_target_dir, g_file);

    return 0;
}

