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
#ifndef _OUTPUTCHANNEL_H
#define _OUTPUTCHANNEL_H

#include <stdarg.h>

/* Callback function to print a printf-formatted string to a channel */
typedef int (*OutputChannelPrintf) (void* opaque, const char* fmt, va_list ap);

typedef struct OutputChannel OutputChannel;

/* Allocates a new output channel */
OutputChannel* output_channel_alloc(void* opaque, OutputChannelPrintf cb);

/* Prints a printf-formatted string to the output channel */
int output_channel_printf(OutputChannel* oc, const char* fmt, ...);

/* Frees an output channel */
void output_channel_free(OutputChannel* oc);

/* Returns the number of bytes written to the channel */
unsigned int output_channel_written(OutputChannel* oc);

#endif /* _OUTPUTCHANNEL_H */
