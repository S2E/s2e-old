/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "outputchannel.h"
#include "qemu-common.h"

struct OutputChannel {
    void*               opaque;   /* caller-specific information */
    OutputChannelPrintf printf;   /* callback function to do the printing */
    unsigned int        written;  /* number of bytes written to the channel */
};

OutputChannel* output_channel_alloc(void* opaque, OutputChannelPrintf cb)
{
    OutputChannel* oc = qemu_mallocz(sizeof(*oc));
    oc->printf  = cb;
    oc->opaque  = opaque;
    oc->written = 0;

    return oc;
}

int output_channel_printf(OutputChannel* oc, const char* fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = oc->printf(oc->opaque, fmt, ap);
    va_end(ap);

    /* Don't count errors and no-ops towards number of bytes written */
    if (ret > 0) {
        oc->written += ret;
    }

    return ret;
}

void output_channel_free(OutputChannel* oc)
{
    free(oc);
}

unsigned int output_channel_written(OutputChannel* oc)
{
    return oc->written;
}
