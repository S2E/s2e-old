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
#include "android/vm/info.h"
#include "android_utils.h"
#include "android/utils/debug.h"
#include "android/utils/dirscanner.h"
#include "osdep.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* global variables - see android/globals.h */
AvmInfoParams   android_vmParams[1];
AvmInfo*        android_vmInfo;

/* for debugging */
#define  D(...)   VERBOSE_PRINT(init,__VA_ARGS__)
#define  DD(...)  VERBOSE_PRINT(vm_config,__VA_ARGS__)

/* technical note on how all of this is supposed to work:
 *
 * we assume the following SDK layout:
 *
 *  SDK/
 *    tools/
 *      emulator[.exe]
 *      libs/
 *        hardware-properties.ini
 *        ...
 *
 *    platforms/
 *      <platform1>/
 *        build.prop
 *        images/
 *          <default kernel/disk images>
 *        skins/
 *          default/    --> default skin
 *            layout
 *            <skin bitmaps>
 *          <skin2>/    --> another skin
 *            layout
 *            <skin bitmaps>
 *          <skin3>/    --> skin alias to <skin2>
 *            alias-<skin2>
 *
 *      <platform2>/
 *        build.prop
 *        images/
 *          <other default kernel/disk images>
 *
 *    add-ons/
 *      <partner1>/
 *        manifest.ini
 *        images/
 *          <replacement disk images>
 *
 *      <partner2>/
 *        manifest.ini
 *        <replacement disk images>
 *        hardware.ini
 *        skins/
 *           default/
 *              layout
 *              <skin bitmaps>
 *           <skin2>/
 *              layout
 *              <skin bitmaps>
 *
 *
 * we define a 'platform' as a directory that provides a complete
 * set of disk/kernel images, some skins, as well as a build.prop
 * file.
 *
 * we define an 'addon' as a directory that provides additionnal
 * or replacement files related to a given existing platform.
 * each add-on provides at the minimum a 'manifest.ini' file
 * that describes it (see below).
 *
 * important notes:
 *
 * - the build.prop file of a given platform directory contains
 *   a line that reads 'ro.build.version.sdk=<version>' where
 *   <version> is an integer corresponding to the corresponding
 *   official API version number as defined by Android.
 *
 *   each platform provided with the SDK must have a unique
 *   version number.
 *
 * - the manifest.ini of a given addon must contain lines
 *   that include:
 *
 *      name=<addOnName>
 *      vendor=<vendorName>
 *      api=<version>
 *
 *   where <version> is used to identify the platform the add-on
 *   refers to. Note that the platform's directory name is
 *   irrelevant to the matching algorithm.
 *
 *   each addon available must have a unique
 *   <vendor>:<name>:<sdk> triplet
 *
 * - an add-on can provide a hardware.ini file. If present, this
 *   is used to force the hardware setting of any virtual machine
 *   built from the add-on.
 *
 * - the file in SDK/tools/lib/hardware-properties.ini declares which
 *   hardware properties are supported by the emulator binary.
 *   these can appear in the config.ini file of a given virtual
 *   machine, or the hardware.ini of a given add-on.
 *
 * normally, a virtual machine corresponds to:
 *
 *  - a root configuration file, placed in ~/.android/vm/<foo>.ini
 *    where <foo> is the name of the virtual machine.
 *
 *  - a "content" directory, which contains disk images for the
 *    virtual machine (e.g. at a minimum, the userdata.img file)
 *    plus some configuration information.
 *
 *  - the root config file must have at least two lines like:
 *
 *      path=<pathToContentDirectory>
 *      target=<targetAddonOrPlatform>
 *
 *    the 'path' value must point to the location of
 *    the virtual machine's content directory. By default, this
 *    should be ~/.android/vm/<foo>/, though the user should be
 *    able to choose an alternative path at creation time.
 *
 *    the 'target' value can be one of:
 *
 *        android-<version>
 *        <vendor>:<name>:<version>
 *
 *    the first form is used to refer to a given platform.
 *    the second form is used to refer to a unique add-on.
 *    in both forms, <version> must be an integer that
 *    matches one of the available platforms.
 *
 *    <vendor>:<name>:<version> must match the triplet of one
 *    of the available add-ons
 *
 *    if the target value is incorrect, or if the content path
 *    is invalid, the emulator will abort with an error.
 *
 * - the content directory shall contain a 'config.ini' that
 *   contains hardware properties for the virtual machine
 *   (as defined by SDK/tools/lib/hardware-properties.ini), as
 *   well as additional lines like:
 *
 *      sdcard=<pathToDefaultSDCard>
 *      skin=<defaultSkinName>
 *      options=<additionalEmulatorStartupOptions>
 *
 *
 *  Finally, to find the skin to be used with a given virtual
 *  machine, the following logic is used:
 *
 *  - if no skin name has been manually specified on
 *    the command line, or in the config.ini file,
 *    look in $CONTENT/skin/layout and use it if available.
 *
 *  - otherwise, set SKINNAME to 'default' if not manually
 *    specified, and look for $ADDON/skins/$SKINNAME/layout
 *    and use it if available
 *
 *  - otherwise, look for $PLATFORM/skins/$SKINNAME/layout
 *    and use it if available.
 *
 *  - otherwise, look for $PLATFORM/skins/$SKINNAME/alias-<other>.
 *    if a file exist by that name, look at $PLATFORM/skins/<other>/layout
 *    and use it if available. Aliases are not recursives :-)
 */

/*  now, things get a little bit more complicated when working
 *  within the Android build system. In this mode, which can be
 *  detected by looking at the definition of the ANDROID_PRODUCT_OUT
 *  environment variable, we're going to simply pick the image files
 *  from the out directory, or from $BUILDROOT/prebuilt
 */

/* the name of the $SDKROOT subdirectory that contains all platforms */
#define  PLATFORMS_SUBDIR  "platforms"

/* the name of the $SDKROOT subdirectory that contains add-ons */
#define  ADDONS_SUBDIR     "add-ons"

/* this is the subdirectory of $HOME/.android where all
 * root configuration files (and default content directories)
 * are located.
 */
#define  ANDROID_VM_DIR    "vm"

/* certain disk image files are mounted read/write by the emulator
 * to ensure that several emulators referencing the same files
 * do not corrupt these files, we need to lock them and respond
 * to collision depending on the image type.
 *
 * the enumeration below is used to record information about
 * each image file path.
 *
 * READONLY means that the file will be mounted read-only
 * and this doesn't need to be locked. must be first in list
 *
 * MUSTLOCK means that the file should be locked before
 * being mounted by the emulator
 *
 * TEMPORARY means that the file has been copied to a
 * temporary image, which can be mounted read/write
 * but doesn't require locking.
 */
typedef enum {
    IMAGE_STATE_READONLY,     /* unlocked */
    IMAGE_STATE_MUSTLOCK,     /* must be locked */
    IMAGE_STATE_LOCKED,       /* locked */
    IMAGE_STATE_LOCKED_EMPTY, /* locked and empty */
    IMAGE_STATE_TEMPORARY,    /* copied to temp file (no lock needed) */
} AvmImageState;

struct AvmInfo {
    /* for the Android build system case */
    char      inAndroidBuild;
    char*     androidOut;
    char*     androidBuildRoot;

    /* for the normal virtual machine case */
    char*     machineName;
    char*     sdkRootPath;
    int       platformVersion;
    char*     platformPath;
    char*     addonTarget;
    char*     addonPath;
    char*     contentPath;
    IniFile*  rootIni;      /* root <foo>.ini file */
    IniFile*  configIni;    /* virtual machine's config.ini */

    /* for both */
    char*     skinName;     /* skin name */
    char*     skinDirPath;  /* skin directory */

    /* image files */
    char*     imagePath [ AVM_IMAGE_MAX ];
    char      imageState[ AVM_IMAGE_MAX ];
};


void
avmInfo_free( AvmInfo*  i )
{
    if (i) {
        int  nn;

        for (nn = 0; nn < AVM_IMAGE_MAX; nn++)
            qemu_free(i->imagePath[nn]);

        qemu_free(i->skinName);
        qemu_free(i->skinDirPath);

        if (i->configIni) {
            iniFile_free(i->configIni);
            i->configIni = NULL;
        }

        if (i->rootIni) {
            iniFile_free(i->rootIni);
            i->rootIni = NULL;
        }

        qemu_free(i->contentPath);
        qemu_free(i->sdkRootPath);

        if (i->inAndroidBuild) {
            qemu_free(i->androidOut);
            qemu_free(i->androidBuildRoot);
        } else {
            qemu_free(i->platformPath);
            qemu_free(i->addonTarget);
            qemu_free(i->addonPath);
        }

        qemu_free(i->machineName);
        qemu_free(i);
    }
}

/* list of default file names for each supported image file type */
static const char*  const  _imageFileNames[ AVM_IMAGE_MAX ] = {
#define  _AVM_IMG(x,y,z)  y,
    AVM_IMAGE_LIST
#undef _AVM_IMG
};

/* list of short text description for each supported image file type */
static const char*  const _imageFileText[ AVM_IMAGE_MAX ] = {
#define  _AVM_IMG(x,y,z)  z,
    AVM_IMAGE_LIST
#undef _AVM_IMG
};

/***************************************************************
 ***************************************************************
 *****
 *****    NORMAL VIRTUAL MACHINE SUPPORT
 *****
 *****/

/* compute path to the root SDK directory
 * assume we are in $SDKROOT/tools/emulator[.exe]
 */
static int
_getSdkRoot( AvmInfo*  i )
{
    char   temp[PATH_MAX], *p=temp, *end=p+sizeof(temp);

    (void) bufprint_app_dir(temp, end);

    i->sdkRootPath = path_parent(temp, 1);
    if (i->sdkRootPath == NULL) {
        derror("can't find root of SDK directory");
        return -1;
    }
    D("found SDK root at %s", i->sdkRootPath);
    return 0;
}

/* returns the full path of the platform subdirectory
 * corresponding to a given API version
 */
static char*
_findPlatformByVersion( const char*  sdkRoot, int  version )
{
    char         temp[PATH_MAX], *p=temp, *end=p+sizeof temp;
    char*        subdir = NULL;
    DirScanner*  scanner;

    DD("> %s(%s,%d)", __FUNCTION__, sdkRoot, version);
    p = bufprint(temp, end, "%s/%s", sdkRoot, PLATFORMS_SUBDIR);
    if (p >= end) {
        DD("! path too long");
        return NULL;
    }

    scanner = dirScanner_new(temp);
    if (scanner == NULL) {
        DD("! cannot scan path %s: %s", temp, strerror(errno));
        return NULL;
    }

    for (;;) {
        IniFile*  ini;
        int       apiVersion;

        subdir = (char*) dirScanner_nextFull(scanner);
        if (subdir == NULL)
            break;

        /* look for a file named "build.prop */
        p = bufprint(temp, end, "%s/build.prop", subdir);
        if (p >= end)
            continue;

        if (!path_exists(temp)) {
            DD("! no file at %s", temp);
            continue;
        }

        ini = iniFile_newFromFile(temp);
        if (ini == NULL)
            continue;

        apiVersion = iniFile_getInteger(ini, "ro.build.version.sdk", -1);
        iniFile_free(ini);

        DD("! found %s (version %d)", temp, apiVersion);

        if (apiVersion == version) {
            /* Bingo */
            subdir = qemu_strdup(subdir);
            break;
        }
    }

    if (!subdir) {
        DD("< didn't found anything");
    }

    dirScanner_free(scanner);
    return subdir;
}

/* returns the full path of the addon corresponding to a given target,
 * or NULL if not found. on success, *pversion will contain the SDK
 * version number
 */
static char*
_findAddonByTarget( const char*  sdkRoot, const char*  target, int  *pversion )
{
    char*  targetCopy    = qemu_strdup(target);
    char*  targetVendor  = NULL;
    char*  targetName    = NULL;
    int    targetVersion = -1;

    char         temp[PATH_MAX];
    char*        p;
    char*        end;
    DirScanner*  scanner;
    char*        subdir;

    DD("> %s(%s,%s)", __FUNCTION__, sdkRoot, target);

    /* extract triplet from target string */
    targetVendor = targetCopy;

    p = strchr(targetVendor, ':');
    if (p == NULL) {
        DD("< missing first column separator");
        goto FAIL;
    }
    *p         = 0;
    targetName = p + 1;
    p          = strchr(targetName, ':');
    if (p == NULL) {
        DD("< missing second column separator");
        goto FAIL;
    }
    *p++ = 0;

    targetVersion = atoi(p);

    if (targetVersion == 0) {
        DD("< invalid version number");
        goto FAIL;
    }
    /* now scan addons directory */
    p   = temp;
    end = p + sizeof temp;

    p = bufprint(p, end, "%s/%s", sdkRoot, ADDONS_SUBDIR);
    if (p >= end) {
        DD("< add-on path too long");
        goto FAIL;
    }
    scanner = dirScanner_new(temp);
    if (scanner == NULL) {
        DD("< cannot scan add-on path %s: %s", temp, strerror(errno));
        goto FAIL;
    }
    for (;;) {
        IniFile*     ini;
        const char*  vendor;
        const char*  name;
        int          version;
        int          matches;

        subdir = (char*) dirScanner_nextFull(scanner);
        if (subdir == NULL)
            break;

        /* try to open the manifest.ini file */
        p = bufprint(temp, end, "%s/manifest.ini", subdir);
        if (p >= end)
            continue;

        ini = iniFile_newFromFile(temp);
        if (ini == NULL)
            continue;

        DD("! scanning manifest.ini in %s", temp);

        /* find the vendor, name and version */
        vendor  = iniFile_getValue(ini,  "vendor");
        name    = iniFile_getValue(ini,  "name");
        version = iniFile_getInteger(ini, "api", -1);

        matches = 0;

        matches += (version == targetVersion);
        matches += (vendor && !strcmp(vendor, targetVendor));
        matches += (name   && !strcmp(name, targetName));

        DD("! matches=%d vendor=[%s] name=[%s] version=%d",
           matches,
           vendor ? vendor : "<NULL>",
           name ? name : "<NULL>",
           version);

        iniFile_free(ini);

        if (matches == 3) {
            /* bingo */
            *pversion = version;
            subdir    = qemu_strdup(subdir);
            break;
        }
    }

    dirScanner_free(scanner);

    DD("< returning %s", subdir ? subdir : "<NULL>");
    return subdir;

FAIL:
    qemu_free(targetCopy);
    return NULL;
}

static int
_checkAvmName( const char*  name )
{
    int  len  = strlen(name);
    int  len2 = strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz"
                             "0123456789_.-");
    return (len == len2);
}

/* parse the root config .ini file. it is located in
 * ~/.android/vm/<name>.ini or Windows equivalent
 */
static int
_getRootIni( AvmInfo*  i )
{
    char  temp[PATH_MAX], *p=temp, *end=p+sizeof(temp);

    p = bufprint_config_path(temp, end);
    p = bufprint(p, end, "/" ANDROID_VM_DIR "/%s.ini", i->machineName);
    if (p >= end) {
        derror("machine name too long");
        return -1;
    }

    i->rootIni = iniFile_newFromFile(temp);
    if (i->rootIni == NULL) {
        derror("unknown virtual machine name: '%s'", i->machineName);
        return -1;
    }
    D("root virtual machine file at %s", temp);
    return 0;
}

/* the .ini variable name that points to the content directory
 * in a root AVM ini file. This is required */
#   define  ROOT_PATH_KEY    "path"
#   define  ROOT_TARGET_KEY  "target"

/* retrieve the content path and target from the root .ini file */
static int
_getTarget( AvmInfo*  i )
{
    i->contentPath = iniFile_getString(i->rootIni, ROOT_PATH_KEY);
    i->addonTarget = iniFile_getString(i->rootIni, ROOT_TARGET_KEY);

    iniFile_free(i->rootIni);
    i->rootIni = NULL;

    if (i->contentPath == NULL) {
        derror("bad config: %s",
               "virtual machine file lacks a "ROOT_PATH_KEY" entry");
        return -1;
    }

    if (i->addonTarget == NULL) {
        derror("bad config: %s",
               "virtual machine file lacks a "ROOT_TARGET_KEY" entry");
        return -1;
    }

    D("virtual machine content at %s", i->contentPath);
    D("virtual machine target is %s", i->addonTarget);

    if (!strncmp(i->addonTarget, "android-", 8)) {  /* target is platform */
        char*        end;
        const char*  versionString = i->addonTarget+8;
        int          version = (int) strtol(versionString, &end, 10);
        if (*end != 0 || version <= 0) {
            derror("bad config: invalid platform version: '%s'", versionString);
            return -1;
        }
        i->platformVersion = version;
        i->platformPath    = _findPlatformByVersion(i->sdkRootPath, 
                                                    version);
        if (i->platformPath == NULL) {
            derror("bad config: unknown platform version: '%d'", version);
            return -1;
        }
    }
    else  /* target is add-on */
    {
        i->addonPath = _findAddonByTarget(i->sdkRootPath, i->addonTarget,
                                          &i->platformVersion);
        if (i->addonPath == NULL) {
            derror("bad config: %s",
                   "unknown add-on target: '%s'", i->addonTarget);
            return -1;
        }

        i->platformPath = _findPlatformByVersion(i->sdkRootPath, 
                                                 i->platformVersion);
        if (i->platformPath == NULL) {
            derror("bad config: %s",
                   "unknown add-on platform version: '%d'", i->platformVersion);
            return -1;
        }
        D("virtual machine add-on path: %s", i->addonPath);
    }
    D("virtual machine platform path: %s",   i->platformPath);
    D("virtual machine platform version %d", i->platformVersion);
    return 0;
}


/* find and parse the config.ini file from the content directory */
static int
_getConfigIni(AvmInfo*  i)
{
    char  temp[PATH_MAX], *p=temp, *end=p+sizeof(temp);

    p = bufprint(p, end, "%s/config.ini", i->contentPath);
    if (p >= end) {
        derror("can't access virtual machine content directory");
        return -1;
    }

#if 1   /* XXX: TODO: remove this in the future */
    /* for now, allow a non-existing config.ini */
    if (!path_exists(temp)) {
        D("virtual machine has no config file - no problem");
        return 0;
    }
#endif

    i->configIni = iniFile_newFromFile(temp);
    if (i->configIni == NULL) {
        derror("bad config: %s",
               "virtual machine directory lacks config.ini");
        return -1;
    }
    D("virtual machine config file: %s", temp);
    return 0;
}

/***************************************************************
 ***************************************************************
 *****
 *****    KERNEL/DISK IMAGE LOADER
 *****
 *****/

/* a structure used to handle the loading of
 * kernel/disk images.
 */
typedef struct {
    AvmInfo*        info;
    AvmInfoParams*  params;
    AvmImageType    id;
    const char*     imageFile;
    const char*     imageText;
    char**          pPath;
    char*           pState;
    char            temp[PATH_MAX];
} ImageLoader;

static void
imageLoader_init( ImageLoader*  l, AvmInfo*  info, AvmInfoParams*  params )
{
    memset(l, 0, sizeof(*l));
    l->info    = info;
    l->params  = params;
}

/* set the type of the image to load */
static void
imageLoader_set( ImageLoader*  l, AvmImageType  id )
{
    l->id        = id;
    l->imageFile = _imageFileNames[id];
    l->imageText = _imageFileText[id];
    l->pPath     = &l->info->imagePath[id];
    l->pState    = &l->info->imageState[id];

    l->pState[0] = IMAGE_STATE_READONLY;
}

/* change the image path */
static char*
imageLoader_setPath( ImageLoader*  l, const char*  path )
{
    path = path ? qemu_strdup(path) : NULL;

    qemu_free(l->pPath[0]);
    l->pPath[0] = (char*) path;

    return (char*) path;
}

static char*
imageLoader_extractPath( ImageLoader*  l )
{
    char*  result = l->pPath[0];
    l->pPath[0] = NULL;
    return result;
}

/* flags used when loading images */
enum {
    IMAGE_REQUIRED          = (1<<0),  /* image is required */
    IMAGE_SEARCH_SDK        = (1<<1),  /* search image in SDK */
    IMAGE_EMPTY_IF_MISSING  = (1<<2),  /* create empty file if missing */
    IMAGE_DONT_LOCK         = (1<<4),  /* don't try to lock image */
    IMAGE_IGNORE_IF_LOCKED  = (1<<5),  /* ignore file if it's locked */
};

#define  IMAGE_OPTIONAL  0

/* find an image from the SDK add-on and/or platform
 * directories. returns the full path or NULL if
 * the file could not be found.
 *
 * note: this stores the result in the image's path as well
 */
static char*
imageLoader_lookupSdk( ImageLoader*  l  )
{
    AvmInfo*  i     = l->info;
    char*     temp  = l->temp, *p = temp, *end = p + sizeof(l->temp);

    do {
        /* try the add-on directory, if any */
        if (i->addonPath != NULL) {
            DD("searching %s in add-on directory: %s", 
                l->imageFile, i->addonPath);

            p = bufprint(temp, end, "%s/images/%s",
                         i->addonPath, l->imageFile);

            if (p < end && path_exists(temp))
                break;
        }

        /* or try the platform directory */
        DD("searching %s in platform directory: %s", 
           l->imageFile, i->platformPath);

        p = bufprint(temp, end, "%s/images/%s",
                     i->platformPath, l->imageFile);
        if (p < end && path_exists(temp))
            break;

        DD("could not find %s in SDK", l->imageFile);
        return NULL;

    } while (0);

    l->pState[0] = IMAGE_STATE_READONLY;

    return imageLoader_setPath(l, temp);
}

/* search for a file in the content directory.
 * returns NULL if the file cannot be found.
 *
 * note that this formats l->temp with the file's path
 * allowing you to retrieve it if the function returns NULL
 */
static char*
imageLoader_lookupContent( ImageLoader*  l )
{
    AvmInfo*  i     = l->info;
    char*     temp  = l->temp, *p = temp, *end = p + sizeof(l->temp);

    DD("searching %s in content directory", l->imageFile);
    p = bufprint(temp, end, "%s/%s", i->contentPath, l->imageFile);
    if (p >= end) {
        derror("content directory path too long");
        exit(2);
    }
    if (!path_exists(temp)) {
        DD("not found %s in content directory", l->imageFile);
        return NULL;
    }

    /* assume content image files must be locked */
    l->pState[0] = IMAGE_STATE_MUSTLOCK;

    return imageLoader_setPath(l, temp);
}

/* lock a file image depending on its state and user flags
 * note that this clears l->pPath[0] if the lock could not
 * be acquired and that IMAGE_IGNORE_IF_LOCKED is used.
 */
static void
imageLoader_lock( ImageLoader*  l, unsigned  flags )
{
    const char*  path = l->pPath[0];

    if (flags & IMAGE_DONT_LOCK)
        return;

    if (l->pState[0] != IMAGE_STATE_MUSTLOCK)
        return;

    D("locking %s image at %s", l->imageText, path);

    if (filelock_create(path) != NULL) {
        /* succesful lock */
        l->pState[0] = IMAGE_STATE_LOCKED;
        return;
    }

    if (flags & IMAGE_IGNORE_IF_LOCKED) {
        dwarning("ignoring locked %s image at %s", l->imageText, path);
        imageLoader_setPath(l, NULL);
        return;
    }

    derror("the %s image is used by another emulator. aborting",
            l->imageText);
    exit(2);
}

/* make a file image empty, this may require locking */
static void
imageLoader_empty( ImageLoader*  l, unsigned  flags )
{
    const char*  path;

    imageLoader_lock(l, flags);

    path = l->pPath[0];
    if (path == NULL)  /* failed to lock, caller will handle it */
        return;

    if (make_empty_file(path) < 0) {
        derror("could not create %s image at %s: %s",
                l->imageText, path, strerror(errno));
        exit(2);
    }
    l->pState[0] = IMAGE_STATE_LOCKED_EMPTY;
}


/* copy image file from a given source 
 * assumes locking is needed.
 */
static void
imageLoader_copyFrom( ImageLoader*  l, const char*  srcPath )
{
    const char*  dstPath = NULL;

    /* find destination file */
    if (l->params) {
        dstPath = l->params->forcePaths[l->id];
    }
    if (!dstPath) {
        imageLoader_lookupContent(l);
        dstPath = l->temp;
    }

    /* lock destination */
    imageLoader_setPath(l, dstPath);
    l->pState[0] = IMAGE_STATE_MUSTLOCK;
    imageLoader_lock(l, 0);

    /* make the copy */
    if (copy_file(dstPath, srcPath) < 0) {
        derror("can't initialize %s image from SDK: %s: %s",
               l->imageText, dstPath, strerror(errno));
        exit(2);
    }
}

/* this will load and eventually lock and image file, depending
 * on the flags being used. on exit, this function udpates
 * l->pState[0] and l->pPath[0]
 *
 * returns the path to the file. Note that it returns NULL
 * only if the file was optional and could not be found.
 *
 * if the file is required and missing, the function aborts
 * the program.
 */
static char*
imageLoader_load( ImageLoader*    l,
                  unsigned        flags )
{
    const char*  path = NULL;

    DD("looking for %s image (%s)", l->imageText, l->imageFile);

    /* first, check user-provided path */
    path = l->params->forcePaths[l->id];
    if (path != NULL) {
        imageLoader_setPath(l, path);
        if (path_exists(path))
            goto EXIT;

        D("user-provided %s image does not exist: %s",
          l->imageText, path);

        /* if the file is required, abort */
        if (flags & IMAGE_REQUIRED) {
            derror("user-provided %s image at %s doesn't exist",
                    l->imageText, path);
            exit(2);
        }
    }
    else {
        const char*   contentFile;

        /* second, look in the content directory */
        path = imageLoader_lookupContent(l);
        if (path) goto EXIT;

        contentFile = qemu_strdup(l->temp);

        /* it's not there */
        if (flags & IMAGE_SEARCH_SDK) {
            /* third, look in the SDK directory */
            path = imageLoader_lookupSdk(l);
            if (path) {
                qemu_free((char*)contentFile);
                goto EXIT;
            }
        }
        DD("found no %s image (%s)", l->imageText, l->imageFile);

        /* if the file is required, abort */
        if (flags & IMAGE_REQUIRED) {
            derror("could not find required %s image (%s)",
                    l->imageText, l->imageFile);
            exit(2);
        }

        path = imageLoader_setPath(l, contentFile);
        qemu_free((char*)contentFile);
    }

    /* otherwise, do we need to create it ? */
    if (flags & IMAGE_EMPTY_IF_MISSING) {
        imageLoader_empty(l, flags);
        return l->pPath[0];
    }
    return NULL;

EXIT:
    imageLoader_lock(l, flags);
    return l->pPath[0];
}



/* find the correct path of all image files we're going to need
 * and lock the files that need it.
 */
static int
_getImagePaths(AvmInfo*  i, AvmInfoParams*  params )
{
    int   wipeData  = (params->flags & AVMINFO_WIPE_DATA) != 0;
    int   wipeCache = (params->flags & AVMINFO_WIPE_CACHE) != 0;
    int   noCache   = (params->flags & AVMINFO_NO_CACHE) != 0;
    int   noSdCard  = (params->flags & AVMINFO_NO_SDCARD) != 0;

    ImageLoader  l[1];

    imageLoader_init(l, i, params);

    /* pick up the kernel and ramdisk image files - these don't
     * need a specific handling.
     */
    imageLoader_set ( l, AVM_IMAGE_KERNEL );
    imageLoader_load( l, IMAGE_REQUIRED | IMAGE_SEARCH_SDK | IMAGE_DONT_LOCK );

    imageLoader_set ( l, AVM_IMAGE_RAMDISK );
    imageLoader_load( l, IMAGE_REQUIRED | IMAGE_SEARCH_SDK | IMAGE_DONT_LOCK );

    /* the system image
     *
     * if there is one in the content directory just lock
     * and use it.
     */
    imageLoader_set ( l, AVM_IMAGE_SYSTEM );
    imageLoader_load( l, IMAGE_REQUIRED | IMAGE_SEARCH_SDK );

    /* the data partition - this one is special because if it
     * is missing, we need to copy the initial image file into it.
     *
     * first, try to see if it is in the content directory
     * (or the user-provided path)
     */
    imageLoader_set( l, AVM_IMAGE_USERDATA );
    if ( !imageLoader_load( l, IMAGE_OPTIONAL |
                               IMAGE_EMPTY_IF_MISSING |
                               IMAGE_DONT_LOCK ) )
    {
        /* it's not, we're going to initialize it. simply
         * forcing a data wipe should be enough */
        D("initializing new data partition image: %s", l->pPath[0]);
        wipeData = 1;
    }

    if (wipeData) {
        /* find SDK source file */
        const char*  srcPath;

        if (imageLoader_lookupSdk(l)) {
            derror("can't locate initial %s image in SDK",
                l->imageText);
            exit(2);
        }
        srcPath = imageLoader_extractPath(l);

        imageLoader_copyFrom( l, srcPath );
        qemu_free((char*) srcPath);
    }
    else
    {
        /* lock the data partition image */
        l->pState[0] = IMAGE_STATE_MUSTLOCK;
        imageLoader_lock( l, 0 );
    }

    /* the cache partition: unless the user doesn't want one,
     * we're going to create it in the content directory
     */
    if (!noCache) {
        imageLoader_set (l, AVM_IMAGE_CACHE);
        imageLoader_load(l, IMAGE_OPTIONAL |
                            IMAGE_EMPTY_IF_MISSING );

        if (wipeCache) {
            if (make_empty_file(l->pPath[0]) < 0) {
                derror("cannot wipe %s image at %s: %s",
                       l->imageText, l->pPath[0],
                       strerror(errno));
                exit(2);
            }
        }
    }

    /* the SD Card image. unless the user doesn't want to, we're
     * going to mount it if available. Note that if the image is
     * already used, we must ignore it.
     */
    if (!noSdCard) {
        imageLoader_set (l, AVM_IMAGE_SDCARD);
        imageLoader_load(l, IMAGE_OPTIONAL |
                            IMAGE_IGNORE_IF_LOCKED);

        /* if the file was not found, ignore it */
        if (l->pPath[0] && !path_exists(l->pPath[0])) 
        {
            D("ignoring non-existing %s at %s: %s",
              l->imageText, l->pPath[0], strerror(errno));

            /* if the user provided the SD Card path by hand,
             * warn him. */
            if (params->forcePaths[AVM_IMAGE_SDCARD] != NULL)
                dwarning("ignoring non-existing SD Card image");

            imageLoader_setPath(l, NULL);
        }
    }

    return 0;
}

/* check that there is a skin named 'skinName' listed from 'skinDirRoot'
 * this returns 1 on success, 0 on failure
 * on success, the 'temp' buffer will get the path containing the real
 * skin directory (after alias expansion), including the skin name.
 */
static int
_checkSkinDir( char*  temp, char*  end, const char*  skinDirRoot, const char*  skinName )
{
    DirScanner*  scanner;
    char        *p, *q;
    int          result;

    p  = bufprint(temp, end, "%s/skins/%s",
                  skinDirRoot, skinName);

    DD("probing skin content in %s", temp);

    if (p >= end || !path_exists(temp)) {
        return 0;
    }

    /* first, is this a normal skin directory ? */
    q = bufprint(p, end, "/layout");
    if (q < end && path_exists(temp)) {
        /* yes */
        *p = 0;
        return 1;
    }

    /* second, is it an alias to another skin ? */
    *p      = 0;
    result  = 0;
    scanner = dirScanner_new(temp);
    if (scanner != NULL) {
        for (;;) {
            const char*  file = dirScanner_next(scanner);

            if (file == NULL)
                break;

            if (strncmp(file, "alias-", 6) || file[6] == 0)
                continue;

            p = bufprint(temp, end, "%s/skins/%s",
                            skinDirRoot, file+6);

            q = bufprint(p, end, "/layout");
            if (q < end && path_exists(temp)) {
                /* yes, it's an alias */
                *p     = 0;
                result = 1;
                break;
            }
        }
        dirScanner_free(scanner);
    }
    return result;
}

static int
_getSkin( AvmInfo*  i, AvmInfoParams*  params )
{
    char*  skinName;
    char   temp[PATH_MAX], *p=temp, *end=p+sizeof(temp);
    char   explicitSkin = 1;

    /* determine the skin name, the default is "default"
     * unless specified by the caller or in config.ini
     */
    if (params->skinName) {
        skinName = qemu_strdup(params->skinName);
    } else {
        skinName = iniFile_getString( i->configIni, "skin" );
        if (skinName == NULL) {
            skinName     = qemu_strdup("default");
            explicitSkin = 0;
        }
    }

    i->skinName = skinName;

    /* now try to find the skin directory for that name -
     * first try the content directory */
    do {
        /* if there is a single 'skin' directory in
         * the content directory, assume that's what the
         * user wants,  unless an explicit name was given
         */
        if (!explicitSkin) {
            char*  q;

            p = bufprint(temp, end, "%s/skin", i->contentPath);
            q = bufprint(p, end, "/layout");
            if (q < end && path_exists(temp)) {
                /* use this one - cheat a little */
                *p = 0;
                D("using skin content from %s", temp);
                qemu_free(i->skinName);
                i->skinName    = qemu_strdup("skin");
                i->skinDirPath = qemu_strdup(i->contentPath);
                return 0;
            }
        }

        /* look in content directory */
        if (_checkSkinDir(temp, end, i->contentPath, skinName))
            break;

        /* look in the add-on directory, if any */
        if (i->addonPath && 
            _checkSkinDir(temp, end, i->addonPath, skinName))
            break;

        /* look in the platforms directory */
        if (_checkSkinDir(temp, end, i->platformPath, skinName))
            break;

        /* didn't find it */
        if (explicitSkin)
            dwarning("could not find directory for skin '%s'", skinName);

        return -1;

    } while (0);

    /* separate skin name from parent directory. the skin name
     * returned in 'temp' might be different from the original
     * one due to alias expansion so strip it.
     */
    p = strrchr(temp, '/');
    if (p == NULL) {
        /* should not happen */
        DD("weird skin path: %s", temp);
        return -1;
    }

    *p = 0;
    DD("found skin content in %s", temp);
    i->skinDirPath = qemu_strdup(temp);
    return 0;
}


AvmInfo*
avmInfo_new( const char*  name, AvmInfoParams*  params )
{
    AvmInfo*  i;

    if (name == NULL)
        return NULL;

    if (!_checkAvmName(name)) {
        derror("virtual machine name contains invalid characters");
        exit(1);
    }

    i              = qemu_mallocz(sizeof *i);
    i->machineName = qemu_strdup(name);

    if ( _getSdkRoot(i)    < 0 ||
         _getRootIni(i)    < 0 ||
         _getTarget(i)     < 0 ||
         _getConfigIni(i)  < 0 )
        goto FAIL;

    if ( _getImagePaths(i, params) < 0 ||
         _getSkin      (i, params) < 0 )
        goto FAIL;

    return i;

FAIL:
    avmInfo_free(i);
    return NULL;
}

/***************************************************************
 ***************************************************************
 *****
 *****    ANDROID BUILD SUPPORT
 *****
 *****    The code below corresponds to the case where we're
 *****    starting the emulator inside the Android build
 *****    system. The main differences are that:
 *****
 *****    - the $ANDROID_PRODUCT_OUT directory is used as the
 *****      content file.
 *****
 *****    - built images must not be modified by the emulator,
 *****      so system.img must be copied to a temporary file
 *****      and userdata.img must be copied to userdata-qemu.img
 *****      if the latter doesn't exist.
 *****
 *****    - the kernel and default skin directory are taken from
 *****      prebuilt
 *****
 *****    - there is no root .ini file, or any config.ini in
 *****      the content directory, no SDK platform version
 *****      and no add-on to consider.
 *****/

/* used to fake a config.ini located in the content directory */
static int
_getBuildConfigIni( AvmInfo*  i )
{
    /* a blank file is ok at the moment */
    i->configIni = iniFile_newFromMemory( "", 0 );
    return 0;
}

static int
_getBuildImagePaths( AvmInfo*  i, AvmInfoParams*  params )
{
    int   wipeData  = (params->flags & AVMINFO_WIPE_DATA) != 0;
    int   noCache   = (params->flags & AVMINFO_NO_CACHE) != 0;
    int   noSdCard  = (params->flags & AVMINFO_NO_SDCARD) != 0;

    char         temp[PATH_MAX], *p=temp, *end=p+sizeof temp;
    char*        srcData;
    ImageLoader  l[1];

    imageLoader_init(l, i, params);

    /** load the kernel image
     **/

    /* if it is not in the out directory, get it from prebuilt
     */
    imageLoader_set ( l, AVM_IMAGE_KERNEL );

    if ( !imageLoader_load( l, IMAGE_OPTIONAL |
                               IMAGE_DONT_LOCK ) )
    {
#define  PREBUILT_KERNEL_PATH   "prebuilt/android-arm/kernel/kernel-qemu"
        p = bufprint(temp, end, "%s/%s", i->androidBuildRoot,
                        PREBUILT_KERNEL_PATH);
        if (p >= end || !path_exists(temp)) {
            derror("bad workspace: cannot find prebuilt kernel in: %s", temp);
            exit(1);
        }
        imageLoader_setPath(l, temp);
    }

    /** load the data partition. note that we use userdata-qemu.img
     ** since we don't want to modify userdata.img at all
     **/
    imageLoader_set ( l, AVM_IMAGE_USERDATA );
    imageLoader_load( l, IMAGE_OPTIONAL | IMAGE_DONT_LOCK );

    /* get the path of the source file, and check that it actually exists
     * if the user didn't provide an explicit data file
     */
    srcData = imageLoader_extractPath(l);
    if (srcData == NULL && params->forcePaths[AVM_IMAGE_USERDATA] == NULL) {
        derror("There is no %s image in your build directory. Please make a full build",
                l->imageText, l->imageFile);
        exit(2);
    }

    /* get the path of the target file */
    l->imageFile = "userdata-qemu.img";
    imageLoader_load( l, IMAGE_OPTIONAL |
                         IMAGE_EMPTY_IF_MISSING |
                         IMAGE_IGNORE_IF_LOCKED );

    /* force a data wipe if we just created the image */
    if (l->pState[0] == IMAGE_STATE_LOCKED_EMPTY)
        wipeData = 1;

    /* if the image was already locked, create a temp file
     * then force a data wipe.
     */
    if (l->pPath[0] == NULL) {
        TempFile*  temp = tempfile_create();
        imageLoader_setPath(l, tempfile_path(temp));
        dwarning( "Another emulator is running. user data changes will *NOT* be saved");
        wipeData = 1;
    }

    /* in the case of a data wipe, copy userdata.img into
     * the destination */
    if (wipeData) {
        if (srcData == NULL || !path_exists(srcData)) {
            derror("There is no %s image in your build directory. Please make a full build",
                   l->imageText, _imageFileNames[l->id]);
            exit(2);
        }
        if (copy_file( l->pPath[0], srcData ) < 0) {
            derror("could not initialize %s image from %s: %s",
                   l->imageText, temp, strerror(errno));
            exit(2);
        }
    }

    qemu_free(srcData);

    /** load the ramdisk image
     **/
    imageLoader_set ( l, AVM_IMAGE_RAMDISK );
    imageLoader_load( l, IMAGE_REQUIRED |
                         IMAGE_DONT_LOCK );

    /** load the system image. read-only. the caller must
     ** take care of checking the state
     **/
    imageLoader_set ( l, AVM_IMAGE_SYSTEM );
    imageLoader_load( l, IMAGE_REQUIRED | IMAGE_DONT_LOCK );

    /* force the system image to read-only status */
    l->pState[0] = IMAGE_STATE_READONLY;

    /** cache partition handling
     **/
    if (!noCache) {
        imageLoader_set (l, AVM_IMAGE_CACHE);

        /* if the user provided one cache image, lock & use it */
        if ( params->forcePaths[l->id] != NULL ) {
            imageLoader_load(l, IMAGE_REQUIRED | 
                                IMAGE_IGNORE_IF_LOCKED);
        }
    }

    /** SD Card image
     **/
    if (!noSdCard) {
        imageLoader_set (l, AVM_IMAGE_SDCARD);
        imageLoader_load(l, IMAGE_OPTIONAL | IMAGE_IGNORE_IF_LOCKED);
    }

    return 0;
}

static int
_getBuildSkin( AvmInfo*  i, AvmInfoParams*  params )
{
    /* the (current) default skin name for our build system */
    const char*  skinName = params->skinName;
    const char*  skinDir  = params->skinRootPath;
    char         temp[PATH_MAX], *p=temp, *end=p+sizeof(temp);
    char*        q;

    if (!skinName) {
        /* the (current) default skin name for the build system */
        skinName = "HVGA";
        DD("selecting default skin name '%s'", skinName);
    }

    i->skinName = qemu_strdup(skinName);

    if (!skinDir) {

#define  PREBUILT_SKINS_DIR  "development/emulator/skins"

        /* the (current) default skin directory */
        p = bufprint( temp, end, "%s/%s",
                      i->androidBuildRoot, PREBUILT_SKINS_DIR );
    } else {
        p = bufprint( temp, end, "%s", skinDir );
    }

    q  = bufprint(p, end, "/%s/layout", skinName);
    if (q >= end || !path_exists(temp)) {
        DD("skin content directory does not exist: %s", temp);
        if (skinDir)
            dwarning("could not find valid skin '%s' in %s:\n",
                     skinName, temp);
        return -1;
    }
    *p = 0;
    DD("found skin path: %s", temp);
    i->skinDirPath = qemu_strdup(temp);

    return 0;
}

AvmInfo*
avmInfo_newForAndroidBuild( const char*     androidBuildRoot,
                            const char*     androidOut,
                            AvmInfoParams*  params )
{
    AvmInfo*  i;

    i = qemu_mallocz(sizeof *i);

    i->inAndroidBuild   = 1;
    i->androidBuildRoot = qemu_strdup(androidBuildRoot);
    i->androidOut       = qemu_strdup(androidOut);
    i->contentPath      = qemu_strdup(androidOut);

    /* TODO: find a way to provide better information from the build files */
    i->machineName = qemu_strdup("<build>");

    if (_getBuildConfigIni(i)          < 0 ||
        _getBuildImagePaths(i, params) < 0 )
        goto FAIL;

    /* we don't need to fail if there is no valid skin */
    _getBuildSkin(i, params);

    return i;

FAIL:
    avmInfo_free(i);
    return NULL;
}

const char*
avmInfo_getName( AvmInfo*  i )
{
    return i ? i->machineName : NULL;
}

const char*
avmInfo_getImageFile( AvmInfo*  i, AvmImageType  imageType )
{
    if (i == NULL || (unsigned)imageType >= AVM_IMAGE_MAX)
        return NULL;

    return i->imagePath[imageType];
}

int
avmInfo_isImageReadOnly( AvmInfo*  i, AvmImageType  imageType )
{
    if (i == NULL || (unsigned)imageType >= AVM_IMAGE_MAX)
        return 1;

    return (i->imageState[imageType] == IMAGE_STATE_READONLY);
}

const char*
avmInfo_getSkinName( AvmInfo*  i )
{
    return i->skinName;
}

const char*
avmInfo_getSkinDir ( AvmInfo*  i )
{
    return i->skinDirPath;
}

int
avmInfo_getHwConfig( AvmInfo*  i, AndroidHwConfig*  hw )
{
    IniFile*   ini = i->configIni;
    int        ret;

    if (ini == NULL)
        ini = iniFile_newFromMemory("", 0);

    ret = androidHwConfig_read(hw, ini);

    if (ini != i->configIni)
        iniFile_free(ini);

    return ret;
}


char*
avmInfo_getTracePath( AvmInfo*  i, const char*  traceName )
{
    char   tmp[MAX_PATH], *p=tmp, *end=p + sizeof(tmp);

    if (i == NULL || traceName == NULL || traceName[0] == 0)
        return NULL;

    if (i->inAndroidBuild) {
        p = bufprint( p, end, "%s" PATH_SEP "traces" PATH_SEP "%s",
                      i->androidOut, traceName );
    } else {
        p = bufprint( p, end, "%s" PATH_SEP "traces" PATH_SEP "%s",
                      i->contentPath, traceName );
    }
    return qemu_strdup(tmp);
}
