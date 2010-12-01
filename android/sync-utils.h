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

/*
 * Contains helper routines dealing with syncronous access to a non-blocking
 * sokets.
 */

#ifndef ANDROID_SYNC_UTILS_H
#define ANDROID_SYNC_UTILS_H

typedef struct SyncSocket SyncSocket;

SyncSocket* syncsocket_connect(int fd, SockAddress* sockaddr, int timeout);

void syncsocket_close(SyncSocket* ssocket);

void syncsocket_free(SyncSocket* ssocket);

int syncsocket_start_read(SyncSocket* ssocket);

int syncsocket_stop_read(SyncSocket* ssocket);

int syncsocket_read_absolute(SyncSocket* ssocket,
                             void* buf,
                             int size,
                             int64_t deadline);

int syncsocket_read(SyncSocket* ssocket, void* buf, int size, int timeout);

#endif  // ANDROID_SYNC_UTILS_H
