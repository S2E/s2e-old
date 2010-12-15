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

#ifndef SNAPSHOTS_H_
#define SNAPSHOTS_H_

#include "config/config.h"

#if CONFIG_ANDROID_SNAPSHOTS

/* Prints a table with information on the snapshot stored in the file
 * 'snapstorage', then exit()s.
 */
void snapshot_print_and_exit( const char *snapstorage );


extern int android_snapshot_update_time;
extern int android_snapshot_update_time_request;

#endif

#endif /* SNAPSHOTS_H_ */
