/* Copyright (C) 2008 The Android Open Source Project
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
#ifndef ANDROID_VM_INFO_H
#define ANDROID_VM_INFO_H

#include "android/utils/ini.h"
#include "android/vm/hw-config.h"

/* An Android Virtual Machine (AVM for short) corresponds to a
 * directory containing all kernel/disk images for a given virtual
 * device, as well as information about its hardware capabilities,
 * SDK version number, skin, etc...
 *
 * Each AVM has a human-readable name and is backed by a root
 * configuration file and a content directory. For example, an
 *  AVM named 'foo' will correspond to the following:
 *
 *  - a root configuration file named ~/.android/vm/foo.ini
 *    describing where the AVM's content can be found
 *
 *  - a content directory like ~/.android/vm/foo/ containing all
 *    disk image and configuration files for the virtual device.
 *
 * the 'foo.ini' file should contain at least one line of the form:
 *
 *    rootPath=<content-path>
 *
 * it may also contain other lines that cache stuff found in the
 * content directory, like hardware properties or SDK version number.
 *
 * it is possible to move the content directory by updating the foo.ini
 * file to point to the new location. This can be interesting when your
 * $HOME directory is located on a network share or in a roaming profile
 * (Windows), given that the content directory of a single virtual device
 * can easily use more than 100MB of data.
 *
 */

/* a macro used to define the list of disk images managed by the
 * implementation. This macro will be expanded several times with
 * varying definitions of _AVM_IMG
 */
#define  AVM_IMAGE_LIST \
    _AVM_IMG(KERNEL,"kernel-qemu","kernel") \
    _AVM_IMG(RAMDISK,"ramdisk.img","ramdisk") \
    _AVM_IMG(SYSTEM,"system.img","system") \
    _AVM_IMG(USERDATA,"userdata.img","user data") \
    _AVM_IMG(CACHE,"cache.img","cache") \
    _AVM_IMG(SDCARD,"sdcard.img","SD Card") \

/* define the enumared values corresponding to each AVM image type
 * examples are: AVM_IMAGE_KERNEL, AVM_IMAGE_SYSTEM, etc..
 */
#define _AVM_IMG(x,y,z)   AVM_IMAGE_##x ,
typedef enum {
    AVM_IMAGE_LIST
    AVM_IMAGE_MAX /* do not remove */
} AvmImageType;
#undef  _AVM_IMG

/* AvmInfo is an opaque structure used to model the information
 * corresponding to a given AVM instance
 */
typedef struct AvmInfo  AvmInfo;

/* various flags used when creating an AvmInfo object */
typedef enum {
    /* use to force a data wipe */
    AVMINFO_WIPE_DATA = (1 << 0),
    /* use to ignore the cache partition */
    AVMINFO_NO_CACHE  = (1 << 1),
    /* use to wipe cache partition, ignored if NO_CACHE is set */
    AVMINFO_WIPE_CACHE = (1 << 2),
    /* use to ignore ignore SDCard image (default or provided) */
    AVMINFO_NO_SDCARD = (1 << 3),
} AvmFlags;

typedef struct {
    unsigned     flags;
    const char*  skinName;
    const char*  skinRootPath;
    const char*  forcePaths[AVM_IMAGE_MAX];
} AvmInfoParams;

/* Creates a new AvmInfo object from a name. Returns NULL if name is NULL
 * or contains characters that are not part of the following list:
 * letters, digits, underscores, dashes and periods
 */
AvmInfo*  avmInfo_new( const char*  name, AvmInfoParams*  params );

/* A special function used to setup an AvmInfo for use when starting
 * the emulator from the Android build system. In this specific instance
 * we're going to create temporary files to hold all writable image
 * files, and activate all hardware features by default
 *
 * 'androidBuildRoot' must be the absolute path to the root of the
 * Android build system (i.e. the 'android' directory)
 *
 * 'androidOut' must be the target-specific out directory where
 * disk images will be looked for.
 */
AvmInfo*  avmInfo_newForAndroidBuild( const char*     androidBuildRoot,
                                      const char*     androidOut,
                                      AvmInfoParams*  params );

/* Frees an AvmInfo object and the corresponding strings that may be
 * returned by its getXXX() methods
 */
void        avmInfo_free( AvmInfo*  i );

/* Return the name of the Android Virtual Machine
 */
const char*  avmInfo_getName( AvmInfo*  i );

/* Try to find the path of a given image file, returns NULL
 * if the corresponding file could not be found. the string
 * belongs to the AvmInfo object.
 */
const char*  avmInfo_getImageFile( AvmInfo*  i, AvmImageType  imageType );

/* Returns 1 if the corresponding image file is read-only
 */
int          avmInfo_isImageReadOnly( AvmInfo*  i, AvmImageType  imageType );

/* lock an image file if it is writable. returns 0 on success, or -1
 * otherwise. note that if the file is read-only, it doesn't need to
 * be locked and the function will return success.
 */
int          avmInfo_lockImageFile( AvmInfo*  i, AvmImageType  imageType, int  abortOnError);

/* Manually set the path of a given image file. */
void         avmInfo_setImageFile( AvmInfo*  i, AvmImageType  imageType, const char*  imagePath );

/* Returns the path of the skin directory */
/* the string belongs to the AvmInfo object */
const char*  avmInfo_getSkinPath( AvmInfo*  i );

/* Returns the name of the virtual machine's skin */
const char*  avmInfo_getSkinName( AvmInfo*  i );

/* Returns the root skin directory for this machine */
const char*  avmInfo_getSkinDir ( AvmInfo*  i );

/* Reads the AVM's hardware configuration into 'hw'. returns -1 on error, 0 otherwise */
int          avmInfo_getHwConfig( AvmInfo*  i, AndroidHwConfig*  hw );

/* Returns a *copy* of the path used to store trace 'foo'. result must be freed by caller */
char*        avmInfo_getTracePath( AvmInfo*  i, const char*  traceName );

/* */

#endif /* ANDROID_VM_INFO_H */
