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
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "s2e.h"

int main(int argc, const char** argv)
{
    if(argc != 2) {
        fprintf(stderr, "Usage: %s file_name\n", argv[0]);
        exit(1);
    }

    printf("Waiting for S2E mode...\n");
    while(s2e_version() == 0) /* nothing */;
    printf("... S2E mode detected\n");

    printf("Copying file %s from the host...\n", argv[1]);

    unlink(argv[1]);

#ifdef _WIN32
    int fd = open(argv[1], O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRWXU);
#else
    int fd = creat(argv[1], S_IRWXU);
#endif

    if(fd == -1) {
        fprintf(stderr, "cannot create file %s\n", argv[1]);
        exit(1);
    }

    int s2e_fd = s2e_open(argv[1]);
    if(s2e_fd == -1) {
        fprintf(stderr, "s2e_open of %s failed\n", argv[1]);
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

    s2e_close(s2e_fd);
    close(fd);

    printf("... file %s of size %d was transfered successfully\n",
            argv[1], fsize);

    return 0;
}

