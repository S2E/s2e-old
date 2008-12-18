/* Copyright (C) 2007-2008 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
#ifndef _ANDROID_GLOBALS_H
#define _ANDROID_GLOBALS_H

#include "android/vm/info.h"
#include "android/vm/hw-config.h"

/* this structure is setup when loading the virtual machine
 * after that, you can read the 'flags' field to determine
 * wether a data or cache wipe has been in effect.
 */
extern AvmInfoParams     android_vmParams[1];

/* a pointer to the android virtual machine information
 * object, which can be queried for the paths of various
 * image files or the skin
 */
extern AvmInfo*          android_vmInfo;

/* the hardware configuration for this specific virtual machine */
extern AndroidHwConfig   android_hw[1];

#endif /* _ANDROID_GLOBALS_H */
