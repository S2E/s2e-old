/* Copyright (C) 2006-2008 The Android Open Source Project
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

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#ifdef _WIN32
#include <process.h>
#endif

#include "sockets.h"

#include "android/android.h"
#include "qemu-common.h"
#include "sysemu.h"
#include "console.h"
#include "user-events.h"

#include <SDL.h>
#include <SDL_syswm.h>

#include "math.h"

#include "android/charmap.h"
#include "android/utils/debug.h"
#include "android/config.h"
#include "android/config/config.h"

#include "android/user-config.h"
#include "android/utils/bufprint.h"
#include "android/utils/dirscanner.h"
#include "android/utils/path.h"
#include "android/utils/tempfile.h"

#include "android/main-common.h"
#include "android/help.h"
#include "hw/goldfish_nand.h"

#include "android/globals.h"

#include "android/qemulator.h"
#include "android/display.h"

#include "android/snapshot.h"
#include "android/protocol/core-connection.h"
#include "android/protocol/fb-updates-impl.h"
#include "android/protocol/user-events-proxy.h"
#include "android/protocol/core-commands-proxy.h"
#include "android/protocol/ui-commands-impl.h"
#include "android/protocol/attach-ui-impl.h"

#include "android/framebuffer.h"
#include "iolooper.h"

AndroidRotation  android_framebuffer_rotation;

#define  STRINGIFY(x)   _STRINGIFY(x)
#define  _STRINGIFY(x)  #x

#ifdef ANDROID_SDK_TOOLS_REVISION
#  define  VERSION_STRING  STRINGIFY(ANDROID_SDK_TOOLS_REVISION)".0"
#else
#  define  VERSION_STRING  "standalone"
#endif

#define  D(...)  do {  if (VERBOSE_CHECK(init)) dprint(__VA_ARGS__); } while (0)

extern int  control_console_start( int  port );  /* in control.c */

extern int qemu_milli_needed;

/* the default device DPI if none is specified by the skin
 */
#define  DEFAULT_DEVICE_DPI  165

#if 0
static int  opts->flashkeys;      /* forward */
#endif

#ifdef CONFIG_TRACE
extern void  start_tracing(void);
extern void  stop_tracing(void);
#endif

unsigned long   android_verbose;

/* -ui-settings parameters received from the core on UI attachment. */
char* core_ui_settings = "";

/* Emulator's core port. */
int android_base_port = 0;

/* this is used by hw/events_device.c to send the charmap name to the system */
extern const char*    android_skin_keycharmap;


int qemu_main(int argc, char **argv);

/* this function dumps the QEMU help */
extern void  help( void );
extern void  emulator_help( void );

#define  VERBOSE_OPT(str,var)   { str, &var }

#define  _VERBOSE_TAG(x,y)   { #x, VERBOSE_##x, y },
static const struct { const char*  name; int  flag; const char*  text; }
verbose_options[] = {
    VERBOSE_TAG_LIST
    { 0, 0, 0 }
};

void emulator_help( void )
{
    STRALLOC_DEFINE(out);
    android_help_main(out);
    printf( "%.*s", out->n, out->s );
    stralloc_reset(out);
    exit(1);
}

/* this function is used to perform auto-detection of the
 * system directory in the case of a SDK installation.
 *
 * we want to deal with several historical usages, hence
 * the slightly complicated logic.
 *
 * NOTE: the function returns the path to the directory
 *       containing 'fileName'. this is *not* the full
 *       path to 'fileName'.
 */
static char*
_getSdkImagePath( const char*  fileName )
{
    char   temp[MAX_PATH];
    char*  p   = temp;
    char*  end = p + sizeof(temp);
    char*  q;
    char*  app;

    static const char* const  searchPaths[] = {
        "",                                  /* program's directory */
        "/lib/images",                       /* this is for SDK 1.0 */
        "/../platforms/android-1.1/images",  /* this is for SDK 1.1 */
        NULL
    };

    app = bufprint_app_dir(temp, end);
    if (app >= end)
        return NULL;

    do {
        int  nn;

        /* first search a few well-known paths */
        for (nn = 0; searchPaths[nn] != NULL; nn++) {
            p = bufprint(app, end, "%s", searchPaths[nn]);
            q = bufprint(p, end, "/%s", fileName);
            if (q < end && path_exists(temp)) {
                *p = 0;
                goto FOUND_IT;
            }
        }

        /* hmmm. let's assume that we are in a post-1.1 SDK
         * scan ../platforms if it exists
         */
        p = bufprint(app, end, "/../platforms");
        if (p < end) {
            DirScanner*  scanner = dirScanner_new(temp);
            if (scanner != NULL) {
                int          found = 0;
                const char*  subdir;

                for (;;) {
                    subdir = dirScanner_next(scanner);
                    if (!subdir) break;

                    q = bufprint(p, end, "/%s/images/%s", subdir, fileName);
                    if (q >= end || !path_exists(temp))
                        continue;

                    found = 1;
                    p = bufprint(p, end, "/%s/images", subdir);
                    break;
                }
                dirScanner_free(scanner);
                if (found)
                    break;
            }
        }

        /* I'm out of ideas */
        return NULL;

    } while (0);

FOUND_IT:
    //D("image auto-detection: %s/%s", temp, fileName);
    return android_strdup(temp);
}

static char*
_getSdkImage( const char*  path, const char*  file )
{
    char  temp[MAX_PATH];
    char  *p = temp, *end = p + sizeof(temp);

    p = bufprint(temp, end, "%s/%s", path, file);
    if (p >= end || !path_exists(temp))
        return NULL;

    return android_strdup(temp);
}

static char*
_getSdkSystemImage( const char*  path, const char*  optionName, const char*  file )
{
    char*  image = _getSdkImage(path, file);

    if (image == NULL) {
        derror("Your system directory is missing the '%s' image file.\n"
               "Please specify one with the '%s <filepath>' option",
               file, optionName);
        exit(2);
    }
    return image;
}

static void
_forceAvdImagePath( AvdImageType  imageType,
                   const char*   path,
                   const char*   description,
                   int           required )
{
    if (path == NULL)
        return;

    if (required && !path_exists(path)) {
        derror("Cannot find %s image file: %s", description, path);
        exit(1);
    }
    android_avdParams->forcePaths[imageType] = path;
}

static uint64_t
_adjustPartitionSize( const char*  description,
                      uint64_t     imageBytes,
                      uint64_t     defaultBytes,
                      int          inAndroidBuild )
{
    char      temp[64];
    unsigned  imageMB;
    unsigned  defaultMB;

    if (imageBytes <= defaultBytes)
        return defaultBytes;

    imageMB   = convertBytesToMB(imageBytes);
    defaultMB = convertBytesToMB(defaultBytes);

    if (imageMB > defaultMB) {
        snprintf(temp, sizeof temp, "(%d MB > %d MB)", imageMB, defaultMB);
    } else {
        snprintf(temp, sizeof temp, "(%lld bytes > %lld bytes)", imageBytes, defaultBytes);
    }

    if (inAndroidBuild) {
        dwarning("%s partition size adjusted to match image file %s\n", description, temp);
    }

    return convertMBToBytes(imageMB);
}

// Base console port
#define CORE_BASE_PORT          5554

// Maximum number of core porocesses running simultaneously on a machine.
#define MAX_CORE_PROCS          16

// Socket timeout in millisec (set to 5 seconds)
#define CORE_PORT_TIMEOUT_MS    5000

#include "android/async-console.h"

typedef struct {
    LoopIo                 io[1];
    int                    port;
    int                    ok;
    AsyncConsoleConnector  connector[1];
} CoreConsole;

static void
coreconsole_io_func(void* opaque, int fd, unsigned events)
{
    CoreConsole* cc = opaque;
    AsyncStatus  status;
    status = asyncConsoleConnector_run(cc->connector, cc->io);
    if (status == ASYNC_COMPLETE) {
        cc->ok = 1;
    }
}

static void
coreconsole_init(CoreConsole* cc, const SockAddress* address, Looper* looper)
{
    int fd = socket_create_inet(SOCKET_STREAM);
    AsyncStatus status;
    cc->port = sock_address_get_port(address);
    cc->ok   = 0;
    loopIo_init(cc->io, looper, fd, coreconsole_io_func, cc);
    if (fd >= 0) {
        status = asyncConsoleConnector_connect(cc->connector, address, cc->io);
        if (status == ASYNC_ERROR) {
            cc->ok = 0;
        }
    }
}

static void
coreconsole_done(CoreConsole* cc)
{
    socket_close(cc->io->fd);
    loopIo_done(cc->io);
}

/* List emulator core processes running on the given machine.
 * This routine is called from main() if -list-cores parameter is set in the
 * command line.
 * Param:
 *  host Value passed with -list-core parameter. Must be either "localhost", or
 *  an IP address of a machine where core processes must be enumerated.
 */
static void
list_running_cores(const char* host)
{
    Looper*         looper;
    CoreConsole     cores[MAX_CORE_PROCS];
    SockAddress     address;
    int             nn, found;

    if (sock_address_init_resolve(&address, host, CORE_BASE_PORT, 0) < 0) {
        derror("Unable to resolve hostname %s: %s", host, errno_str);
        return;
    }

    looper = looper_newGeneric();

    for (nn = 0; nn < MAX_CORE_PROCS; nn++) {
        int port = CORE_BASE_PORT + nn*2;
        sock_address_set_port(&address, port);
        coreconsole_init(&cores[nn], &address, looper);
    }

    looper_runWithTimeout(looper, CORE_PORT_TIMEOUT_MS*2);

    found = 0;
    for (nn = 0; nn < MAX_CORE_PROCS; nn++) {
        int port = CORE_BASE_PORT + nn*2;
        if (cores[nn].ok) {
            if (found == 0) {
                fprintf(stdout, "Running emulator core processes:\n");
            }
            fprintf(stdout, "Emulator console port %d\n", port);
            found++;
        }
        coreconsole_done(&cores[nn]);
    }
    looper_free(looper);

    if (found == 0) {
       fprintf(stdout, "There were no running emulator core processes found on %s.\n",
               host);
    }
}

/* Attaches starting UI to a running core process.
 * This routine is called from main() when -attach-core parameter is set,
 * indicating that this UI instance should attach to a running core, rather than
 * start a new core process.
 * Param:
 *  opts Android options containing non-NULL attach_core.
 * Return:
 *  0 on success, or -1 on failure.
 */
static int
attach_to_core(AndroidOptions* opts) {
    int iter;
    SockAddress console_socket;
    SockAddress** sockaddr_list;
    QEmulator* emulator;

    // Parse attach_core param extracting the host name, and the port name.
    char* console_address = strdup(opts->attach_core);
    char* host_name = console_address;
    char* port_num = strchr(console_address, ':');
    if (port_num == NULL) {
        // The host name is ommited, indicating the localhost
        host_name = "localhost";
        port_num = console_address;
    } else if (port_num == console_address) {
        // Invalid.
        derror("Invalid value %s for -attach-core parameter\n",
               opts->attach_core);
        return -1;
    } else {
        *port_num = '\0';
        port_num++;
        if (*port_num == '\0') {
            // Invalid.
            derror("Invalid value %s for -attach-core parameter\n",
                   opts->attach_core);
            return -1;
        }
    }

    /* Create socket address list for the given address, and pull appropriate
     * address to use for connection. Note that we're fine copying that address
     * out of the list, since INET and IN6 will entirely fit into SockAddress
     * structure. */
    sockaddr_list =
        sock_address_list_create(host_name, port_num, SOCKET_LIST_FORCE_INET);
    free(console_address);
    if (sockaddr_list == NULL) {
        derror("Unable to resolve address %s: %s\n",
               opts->attach_core, errno_str);
        return -1;
    }
    for (iter = 0; sockaddr_list[iter] != NULL; iter++) {
        if (sock_address_get_family(sockaddr_list[iter]) == SOCKET_INET ||
            sock_address_get_family(sockaddr_list[iter]) == SOCKET_IN6) {
            memcpy(&console_socket, sockaddr_list[iter], sizeof(SockAddress));
            break;
        }
    }
    if (sockaddr_list[iter] == NULL) {
        derror("Unable to resolve address %s. Note that 'port' parameter passed to -attach-core\n"
               "must be resolvable into an IP address.\n", opts->attach_core);
        sock_address_list_free(sockaddr_list);
        return -1;
    }
    sock_address_list_free(sockaddr_list);

    if (attachUiImpl_create(&console_socket)) {
        return -1;
    }

    // Save core's port, and set the title.
    android_base_port = sock_address_get_port(&console_socket);
    emulator = qemulator_get();
    qemulator_set_title(emulator);

    return 0;
}

int main(int argc, char **argv)
{
    char   tmp[MAX_PATH];
    char*  tmpend = tmp + sizeof(tmp);
    char*  args[128];
    int    n;
    char*  opt;
    int    use_sdcard_img = 0;
    int    serial = 0;
    int    gps_serial = 0;
    int    radio_serial = 0;
    int    qemud_serial = 0;
    int    shell_serial = 0;
    unsigned  cachePartitionSize = 0;
    unsigned  systemPartitionSize = 0;
    unsigned  dataPartitionSize = 0;
    unsigned  defaultPartitionSize = convertMBToBytes(66);

    AndroidHwConfig*  hw;
    AvdInfo*          avd;
    AConfig*          skinConfig;
    char*             skinPath;

    //const char *appdir = get_app_dir();
    char*       android_build_root = NULL;
    char*       android_build_out  = NULL;

    AndroidOptions  opts[1];
    /* LCD density value to pass to the core. */
    char lcd_density[16];
    /* net.shared_net_ip boot property value. */
    char boot_prop_ip[64];
    boot_prop_ip[0] = '\0';

    args[0] = argv[0];

    if ( android_parse_options( &argc, &argv, opts ) < 0 ) {
        exit(1);
    }

#ifdef _WIN32
    socket_init();
#endif

    // Lets see if user just wants to list core process.
    if (opts->list_cores) {
        fprintf(stdout, "Enumerating running core processes.\n");
        list_running_cores(opts->list_cores);
        exit(0);
    }

    while (argc-- > 1) {
        opt = (++argv)[0];

        if(!strcmp(opt, "-qemu")) {
            argc--;
            argv++;
            break;
        }

        if (!strcmp(opt, "-help")) {
            emulator_help();
        }

        if (!strncmp(opt, "-help-",6)) {
            STRALLOC_DEFINE(out);
            opt += 6;

            if (!strcmp(opt, "all")) {
                android_help_all(out);
            }
            else if (android_help_for_option(opt, out) == 0) {
                /* ok */
            }
            else if (android_help_for_topic(opt, out) == 0) {
                /* ok */
            }
            if (out->n > 0) {
                printf("\n%.*s", out->n, out->s);
                exit(0);
            }

            fprintf(stderr, "unknown option: -help-%s\n", opt);
            fprintf(stderr, "please use -help for a list of valid topics\n");
            exit(1);
        }

        if (opt[0] == '-') {
            fprintf(stderr, "unknown option: %s\n", opt);
            fprintf(stderr, "please use -help for a list of valid options\n");
            exit(1);
        }

        fprintf(stderr, "invalid command-line parameter: %s.\n", opt);
        fprintf(stderr, "Hint: use '@foo' to launch a virtual device named 'foo'.\n");
        fprintf(stderr, "please use -help for more information\n");
        exit(1);
    }

    if (opts->version) {
        printf("Android emulator version %s\n"
               "Copyright (C) 2006-2008 The Android Open Source Project and many others.\n"
               "This program is a derivative of the QEMU CPU emulator (www.qemu.org).\n\n",
#if defined ANDROID_BUILD_ID
               VERSION_STRING " (build_id " STRINGIFY(ANDROID_BUILD_ID) ")" );
#else
               VERSION_STRING);
#endif
        printf("  This software is licensed under the terms of the GNU General Public\n"
               "  License version 2, as published by the Free Software Foundation, and\n"
               "  may be copied, distributed, and modified under those terms.\n\n"
               "  This program is distributed in the hope that it will be useful,\n"
               "  but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
               "  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
               "  GNU General Public License for more details.\n\n");

        exit(0);
    }

    /* Initialization of UI started with -attach-core should work differently
     * than initialization of UI that starts the core. In particular....
     */

    /* -charmap is incompatible with -attach-core, because particular
     * charmap gets set up in the running core. */
    if (android_charmap_setup(opts->charmap)) {
        exit(1);
    }

    /* legacy support: we used to use -system <dir> and -image <file>
     * instead of -sysdir <dir> and -system <file>, so handle this by checking
     * whether the options point to directories or files.
     */
    if (opts->image != NULL) {
        if (opts->system != NULL) {
            if (opts->sysdir != NULL) {
                derror( "You can't use -sysdir, -system and -image at the same time.\n"
                        "You should probably use '-sysdir <path> -system <file>'.\n" );
                exit(2);
            }
        }
        dwarning( "Please note that -image is obsolete and that -system is now used to point\n"
                  "to the system image. Next time, try using '-sysdir <path> -system <file>' instead.\n" );
        opts->sysdir = opts->system;
        opts->system = opts->image;
        opts->image  = NULL;
    }
    else if (opts->system != NULL && path_is_dir(opts->system)) {
        if (opts->sysdir != NULL) {
            derror( "Option -system should now be followed by a file path, not a directory one.\n"
                    "Please use '-sysdir <path>' to point to the system directory.\n" );
            exit(1);
        }
        dwarning( "Please note that the -system option should now be used to point to the initial\n"
                  "system image (like the obsolete -image option). To point to the system directory\n"
                  "please now use '-sysdir <path>' instead.\n" );

        opts->sysdir = opts->system;
        opts->system = NULL;
    }

    if (opts->nojni)
        opts->no_jni = opts->nojni;

    if (opts->nocache)
        opts->no_cache = opts->nocache;

    if (opts->noaudio)
        opts->no_audio = opts->noaudio;

    if (opts->noskin)
        opts->no_skin = opts->noskin;

    /* If no AVD name was given, try to find the top of the
     * Android build tree
     */
    if (opts->avd == NULL) {
        do {
            char*  out = getenv("ANDROID_PRODUCT_OUT");

            if (out == NULL || out[0] == 0)
                break;

            if (!path_exists(out)) {
                derror("Can't access ANDROID_PRODUCT_OUT as '%s'\n"
                    "You need to build the Android system before launching the emulator",
                    out);
                exit(2);
            }

            android_build_root = path_parent( out, 4 );
            if (android_build_root == NULL || !path_exists(android_build_root)) {
                derror("Can't find the Android build root from '%s'\n"
                    "Please check the definition of the ANDROID_PRODUCT_OUT variable.\n"
                    "It should point to your product-specific build output directory.\n",
                    out );
                exit(2);
            }
            android_build_out = out;
            D( "found Android build root: %s", android_build_root );
            D( "found Android build out:  %s", android_build_out );
        } while (0);
    }
    /* if no virtual device name is given, and we're not in the
     * Android build system, we'll need to perform some auto-detection
     * magic :-)
     */
    if (opts->avd == NULL && !android_build_out)
    {
        char   dataDirIsSystem = 0;

        if (!opts->sysdir) {
            opts->sysdir = _getSdkImagePath("system.img");
            if (!opts->sysdir) {
                derror(
                "You did not specify a virtual device name, and the system\n"
                "directory could not be found.\n\n"
                "If you are an Android SDK user, please use '@<name>' or '-avd <name>'\n"
                "to start a given virtual device (see -help-avd for details).\n\n"

                "Otherwise, follow the instructions in -help-disk-images to start the emulator\n"
                );
                exit(2);
            }
            D("autoconfig: -sysdir %s", opts->sysdir);
        }

        if (!opts->system) {
            opts->system = _getSdkSystemImage(opts->sysdir, "-image", "system.img");
            D("autoconfig: -image %s", opts->image);
        }

        if (!opts->kernel) {
            opts->kernel = _getSdkSystemImage(opts->sysdir, "-kernel", "kernel-qemu");
            D("autoconfig: -kernel %s", opts->kernel);
        }

        if (!opts->ramdisk) {
            opts->ramdisk = _getSdkSystemImage(opts->sysdir, "-ramdisk", "ramdisk.img");
            D("autoconfig: -ramdisk %s", opts->ramdisk);
        }

        /* if no data directory is specified, use the system directory */
        if (!opts->datadir) {
            opts->datadir   = android_strdup(opts->sysdir);
            dataDirIsSystem = 1;
            D("autoconfig: -datadir %s", opts->sysdir);
        }

        if (!opts->data) {
            /* check for userdata-qemu.img in the data directory */
            bufprint(tmp, tmpend, "%s/userdata-qemu.img", opts->datadir);
            if (!path_exists(tmp)) {
                derror(
                "You did not provide the name of an Android Virtual Device\n"
                "with the '-avd <name>' option. Read -help-avd for more information.\n\n"

                "If you *really* want to *NOT* run an AVD, consider using '-data <file>'\n"
                "to specify a data partition image file (I hope you know what you're doing).\n"
                );
                exit(2);
            }

            opts->data = android_strdup(tmp);
            D("autoconfig: -data %s", opts->data);
        }

        if (!opts->sdcard && opts->datadir) {
            bufprint(tmp, tmpend, "%s/sdcard.img", opts->datadir);
            if (path_exists(tmp)) {
                opts->sdcard = android_strdup(tmp);
                D("autoconfig: -sdcard %s", opts->sdcard);
            }
        }

#if CONFIG_ANDROID_SNAPSHOTS
        if (!opts->snapstorage && opts->datadir) {
            bufprint(tmp, tmpend, "%s/snapshots.img", opts->datadir);
            if (path_exists(tmp)) {
                opts->snapstorage = android_strdup(tmp);
                D("autoconfig: -snapstorage %s", opts->snapstorage);
            }
        }
#endif // CONFIG_ANDROID_SNAPSHOTS
    }

    /* setup the virtual device parameters from our options
     */
    if (opts->no_cache) {
        android_avdParams->flags |= AVDINFO_NO_CACHE;
    }
    if (opts->wipe_data) {
        android_avdParams->flags |= AVDINFO_WIPE_DATA | AVDINFO_WIPE_CACHE;
    }
#if CONFIG_ANDROID_SNAPSHOTS
    if (opts->no_snapstorage) {
        android_avdParams->flags |= AVDINFO_NO_SNAPSHOTS;
    }
#endif

    /* if certain options are set, we can force the path of
        * certain kernel/disk image files
        */
    _forceAvdImagePath(AVD_IMAGE_KERNEL,     opts->kernel,      "kernel", 1);
    _forceAvdImagePath(AVD_IMAGE_INITSYSTEM, opts->system,      "system", 1);
    _forceAvdImagePath(AVD_IMAGE_RAMDISK,    opts->ramdisk,     "ramdisk", 1);
    _forceAvdImagePath(AVD_IMAGE_USERDATA,   opts->data,        "user data", 0);
    _forceAvdImagePath(AVD_IMAGE_CACHE,      opts->cache,       "cache", 0);
    _forceAvdImagePath(AVD_IMAGE_SDCARD,     opts->sdcard,      "SD Card", 0);
#if CONFIG_ANDROID_SNAPSHOTS
    _forceAvdImagePath(AVD_IMAGE_SNAPSHOTS,  opts->snapstorage, "snapshots", 0);
#endif

    /* we don't accept -skindir without -skin now
     * to simplify the autoconfig stuff with virtual devices
     */
    if (opts->no_skin) {
        opts->skin    = "320x480";
        opts->skindir = NULL;
    }

    if (opts->skindir) {
        if (!opts->skin) {
            derror( "the -skindir <path> option requires a -skin <name> option");
            exit(1);
        }
    }
    android_avdParams->skinName     = opts->skin;
    android_avdParams->skinRootPath = opts->skindir;

    /* setup the virtual device differently depending on whether
     * we are in the Android build system or not
     */
    if (opts->avd != NULL)
    {
        android_avdInfo = avdInfo_new( opts->avd, android_avdParams );
        if (android_avdInfo == NULL) {
            /* an error message has already been printed */
            dprint("could not find virtual device named '%s'", opts->avd);
            exit(1);
        }
    }
    else
    {
        if (!android_build_out) {
            android_build_out = android_build_root = opts->sysdir;
        }
        android_avdInfo = avdInfo_newForAndroidBuild(
                            android_build_root,
                            android_build_out,
                            android_avdParams );

        if(android_avdInfo == NULL) {
            D("could not start virtual device\n");
            exit(1);
        }
    }

    avd = android_avdInfo;

    /* get the skin from the virtual device configuration */
    opts->skin    = (char*) avdInfo_getSkinName( avd );
    opts->skindir = (char*) avdInfo_getSkinDir( avd );

    if (opts->skin) {
        D("autoconfig: -skin %s", opts->skin);
    }
    if (opts->skindir) {
        D("autoconfig: -skindir %s", opts->skindir);
    }

    /* Read hardware configuration */
    hw = android_hw;
    if (avdInfo_getHwConfig(avd, hw) < 0) {
        derror("could not read hardware configuration ?");
        exit(1);
    }

    if (opts->keyset) {
        parse_keyset(opts->keyset, opts);
        if (!android_keyset) {
            fprintf(stderr,
                    "emulator: WARNING: could not find keyset file named '%s',"
                    " using defaults instead\n",
                    opts->keyset);
        }
    }
    if (!android_keyset) {
        parse_keyset("default", opts);
        if (!android_keyset) {
            android_keyset = skin_keyset_new_from_text( skin_keyset_get_default() );
            if (!android_keyset) {
                fprintf(stderr, "PANIC: default keyset file is corrupted !!\n" );
                fprintf(stderr, "PANIC: please update the code in android/skin/keyset.c\n" );
                exit(1);
            }
            if (!opts->keyset)
                write_default_keyset();
        }
    }

    if (opts->shared_net_id) {
        char*  end;
        long   shared_net_id = strtol(opts->shared_net_id, &end, 0);
        if (end == NULL || *end || shared_net_id < 1 || shared_net_id > 255) {
            fprintf(stderr, "option -shared-net-id must be an integer between 1 and 255\n");
            exit(1);
        }
        snprintf(boot_prop_ip, sizeof(boot_prop_ip),
                 "net.shared_net_ip=10.1.2.%ld", shared_net_id);
    }


    user_config_init();
    parse_skin_files(opts->skindir, opts->skin, opts,
                     &skinConfig, &skinPath);

    if (!opts->netspeed) {
        if (skin_network_speed)
            D("skin network speed: '%s'", skin_network_speed);
        opts->netspeed = (char*)skin_network_speed;
    }
    if (!opts->netdelay) {
        if (skin_network_delay)
            D("skin network delay: '%s'", skin_network_delay);
        opts->netdelay = (char*)skin_network_delay;
    }

    if (opts->trace) {
        char*   tracePath = avdInfo_getTracePath(avd, opts->trace);
        int     ret;

        if (tracePath == NULL) {
            derror( "bad -trace parameter" );
            exit(1);
        }
        ret = path_mkdir_if_needed( tracePath, 0755 );
        if (ret < 0) {
            fprintf(stderr, "could not create directory '%s'\n", tmp);
            exit(2);
        }
        opts->trace = tracePath;
    }

    if (opts->no_cache)
        opts->cache = 0;

    n = 1;
    /* generate arguments for the underlying qemu main() */
    {
        const char*  kernelFile    = avdInfo_getImageFile(avd, AVD_IMAGE_KERNEL);
        int          kernelFileLen = strlen(kernelFile);

        args[n++] = "-kernel";
        args[n++] = (char*)kernelFile;

        /* If the kernel image name ends in "-armv7", then change the cpu
         * type automatically. This is a poor man's approach to configuration
         * management, but should allow us to get past building ARMv7
         * system images with dex preopt pass without introducing too many
         * changes to the emulator sources.
         *
         * XXX:
         * A 'proper' change would require adding some sort of hardware-property
         * to each AVD config file, then automatically determine its value for
         * full Android builds (depending on some environment variable), plus
         * some build system changes. I prefer not to do that for now for reasons
         * of simplicity.
         */
         if (kernelFileLen > 6 && !memcmp(kernelFile + kernelFileLen - 6, "-armv7", 6)) {
            args[n++] = "-cpu";
            args[n++] = "cortex-a8";
         }
    }

    if (boot_prop_ip[0]) {
        args[n++] = "-boot-property";
        args[n++] = boot_prop_ip;
    }

    if (opts->tcpdump) {
        args[n++] = "-tcpdump";
        args[n++] = opts->tcpdump;
    }

#ifdef CONFIG_NAND_LIMITS
    if (opts->nand_limits) {
        args[n++] = "-nand-limits";
        args[n++] = opts->nand_limits;
    }
#endif

    if (opts->timezone) {
        args[n++] = "-timezone";
        args[n++] = opts->timezone;
    }

    if (opts->netspeed) {
        args[n++] = "-netspeed";
        args[n++] = opts->netspeed;
    }
    if (opts->netdelay) {
        args[n++] = "-netdelay";
        args[n++] = opts->netdelay;
    }
    if (opts->netfast) {
        args[n++] = "-netfast";
    }

    /* the purpose of -no-audio is to disable sound output from the emulator,
     * not to disable Audio emulation. So simply force the 'none' backends */
    if (opts->no_audio)
        opts->audio = "none";

    if (opts->audio) {
        args[n++] = "-audio";
        args[n++] = opts->audio;
    }

    if (opts->cpu_delay) {
        args[n++] = "-cpu-delay";
        args[n++] = opts->cpu_delay;
    }

    if (opts->dns_server) {
        args[n++] = "-dns-server";
        args[n++] = opts->dns_server;
    }

    args[n++] = "-initrd";
    args[n++] = (char*) avdInfo_getImageFile(avd, AVD_IMAGE_RAMDISK);

    if (opts->partition_size) {
        char*  end;
        long   sizeMB = strtol(opts->partition_size, &end, 0);
        long   minSizeMB = 10;
        long   maxSizeMB = LONG_MAX / ONE_MB;

        if (sizeMB < 0 || *end != 0) {
            derror( "-partition-size must be followed by a positive integer" );
            exit(1);
        }
        if (sizeMB < minSizeMB || sizeMB > maxSizeMB) {
            derror( "partition-size (%d) must be between %dMB and %dMB",
                    sizeMB, minSizeMB, maxSizeMB );
            exit(1);
        }
        defaultPartitionSize = sizeMB * ONE_MB;
    }

    /* Check the size of the system partition image.
     * If we have an AVD, it must be smaller than
     * the disk.systemPartition.size hardware property.
     *
     * Otherwise, we need to adjust the systemPartitionSize
     * automatically, and print a warning.
     *
     */
    {
        uint64_t   systemBytes  = avdInfo_getImageFileSize(avd, AVD_IMAGE_INITSYSTEM);
        uint64_t   defaultBytes = defaultPartitionSize;

        if (defaultBytes == 0 || opts->partition_size)
            defaultBytes = defaultPartitionSize;

        systemPartitionSize = _adjustPartitionSize("system", systemBytes, defaultBytes,
                                                   android_build_out != NULL);
    }

    /* Check the size of the /data partition. The only interesting cases here are:
     * - when the USERDATA image already exists and is larger than the default
     * - when we're wiping data and the INITDATA is larger than the default.
     */

    {
        const char*  dataPath     = avdInfo_getImageFile(avd, AVD_IMAGE_USERDATA);
        uint64_t     defaultBytes = defaultPartitionSize;

        if (defaultBytes == 0 || opts->partition_size)
            defaultBytes = defaultPartitionSize;

        if (dataPath == NULL || !path_exists(dataPath) || opts->wipe_data) {
            dataPath = avdInfo_getImageFile(avd, AVD_IMAGE_INITDATA);
        }
        if (dataPath == NULL || !path_exists(dataPath)) {
            dataPartitionSize = defaultBytes;
        }
        else {
            uint64_t  dataBytes;
            path_get_size(dataPath, &dataBytes);

            dataPartitionSize = _adjustPartitionSize("data", dataBytes, defaultBytes,
                                                     android_build_out != NULL);
        }
    }

    {
        const char*  filetype = "file";

        if (avdInfo_isImageReadOnly(avd, AVD_IMAGE_INITSYSTEM))
            filetype = "initfile";

        bufprint(tmp, tmpend,
             "system,size=0x%x,%s=%s", systemPartitionSize, filetype,
             avdInfo_getImageFile(avd, AVD_IMAGE_INITSYSTEM));

        args[n++] = "-nand";
        args[n++] = strdup(tmp);
    }

    bufprint(tmp, tmpend,
             "userdata,size=0x%x,file=%s",
             dataPartitionSize,
             avdInfo_getImageFile(avd, AVD_IMAGE_USERDATA));

    args[n++] = "-nand";
    args[n++] = strdup(tmp);

    if (hw->disk_cachePartition) {
        opts->cache = (char*) avdInfo_getImageFile(avd, AVD_IMAGE_CACHE);
        cachePartitionSize = hw->disk_cachePartition_size;
    }
    else if (opts->cache) {
        dwarning( "Emulated hardware doesn't support a cache partition" );
        opts->cache    = NULL;
        opts->no_cache = 1;
    }

    if (opts->cache) {
        /* use a specific cache file */
        sprintf(tmp, "cache,size=0x%0x,file=%s", cachePartitionSize, opts->cache);
        args[n++] = "-nand";
        args[n++] = strdup(tmp);
    }
    else if (!opts->no_cache) {
        /* create a temporary cache partition file */
        sprintf(tmp, "cache,size=0x%0x", cachePartitionSize);
        args[n++] = "-nand";
        args[n++] = strdup(tmp);
    }

    if (hw->hw_sdCard != 0)
        opts->sdcard = (char*) avdInfo_getImageFile(avd, AVD_IMAGE_SDCARD);
    else if (opts->sdcard) {
        dwarning( "Emulated hardware doesn't support SD Cards" );
        opts->sdcard = NULL;
    }

    if(opts->sdcard) {
        uint64_t  size;
        if (path_get_size(opts->sdcard, &size) == 0) {
            /* see if we have an sdcard image.  get its size if it exists */
            /* due to what looks like limitations of the MMC protocol, one has
             * to use an SD Card image that is equal or larger than 9 MB
             */
            if (size < 9*1024*1024ULL) {
                fprintf(stderr, "### WARNING: SD Card files must be at least 9MB, ignoring '%s'\n", opts->sdcard);
            } else {
                args[n++] = "-hda";
                args[n++] = opts->sdcard;
                use_sdcard_img = 1;
            }
        } else {
            D("no SD Card image at '%s'", opts->sdcard);
        }
    }

#if CONFIG_ANDROID_SNAPSHOTS
    if (!opts->no_snapstorage) {
        opts->snapstorage = (char*) avdInfo_getImageFile(avd, AVD_IMAGE_SNAPSHOTS);
        if(opts->snapstorage) {
            if (path_exists(opts->snapstorage)) {
                args[n++] = "-hdb";
                args[n++] = opts->snapstorage;
            } else {
                D("no image at '%s', state snapshots disabled", opts->snapstorage);
            }
        }

        if (!opts->no_snapshot) {
            char* snapshot_name =
                opts->snapshot ? opts->snapshot : "default-boot";
            if (!opts->no_snapshot_load) {
              args[n++] = "-loadvm";
              args[n++] = snapshot_name;
            }
            if (!opts->no_snapshot_save) {
              args[n++] = "-savevm-on-exit";
              args[n++] = snapshot_name;
            }
        } else if (opts->snapshot) {
            dwarning("option '-no-snapshot' overrides '-snapshot', continuing with boot sequence");
        } else if (opts->no_snapshot_load || opts->no_snapshot_save) {
            D("ignoring redundant option(s) '-no-snapshot-load' and/or '-no-snapshot-save' implied by '-no-snapshot'");
        }
        // TODO: Convey -no-snapshot-time-update to core subprocess (?)
    } else if (opts->snapshot || opts->snapstorage) {
        dwarning("option '-no-snapstorage' overrides '-snapshot' and '-snapstorage', "
                 "continuing with full boot, state snapshots are disabled");
    } else if (opts->no_snapshot) {
        D("ignoring redundant option '-no-snapshot' implied by '-no-snapstorage'");
    }

    if (opts->snapshot_list) {
        snapshot_print_and_exit(opts->snapstorage);
    }
#endif // CONFIG_ANDROID_SNAPSHOTS

    if (!opts->logcat || opts->logcat[0] == 0) {
        opts->logcat = getenv("ANDROID_LOG_TAGS");
        if (opts->logcat && opts->logcat[0] == 0)
            opts->logcat = NULL;
    }

#if 0
    if (opts->console) {
        derror( "option -console is obsolete. please use -shell instead" );
        exit(1);
    }
#endif

    /* we always send the kernel messages from ttyS0 to android_kmsg */
    {
        if (opts->show_kernel) {
            args[n++] = "-show-kernel";
        }

        args[n++] = "-serial";
        args[n++] = "android-kmsg";
        serial++;
    }

    /* XXXX: TODO: implement -shell and -logcat through qemud instead */
    if (!opts->shell_serial) {
#ifdef _WIN32
        opts->shell_serial = "con:";
#else
        opts->shell_serial = "stdio";
#endif
    }
    else
        opts->shell = 1;

    if (opts->shell || opts->logcat) {
        args[n++] = "-serial";
        args[n++] = opts->shell_serial;
        shell_serial = serial++;
    }

    if (opts->old_system)
    {
        if (opts->radio) {
            args[n++] = "-serial";
            args[n++] = opts->radio;
            radio_serial = serial++;
        }
        else {
            args[n++] = "-serial";
            args[n++] = "android-modem";
            radio_serial = serial++;
        }
        if (opts->gps) {
            args[n++] = "-serial";
            args[n++] = opts->gps;
            gps_serial = serial++;
        }
    }
    else /* !opts->old_system */
    {
        args[n++] = "-serial";
        args[n++] = "android-qemud";
        qemud_serial = serial++;

        if (opts->radio) {
            args[n++] = "-radio";
            args[n++] = opts->radio;
        }

        if (opts->gps) {
            args[n++] = "-gps";
            args[n++] = opts->gps;
        }
    }

    if (opts->memory) {
        char*  end;
        long   ramSize = strtol(opts->memory, &end, 0);
        if (ramSize < 0 || *end != 0) {
            derror( "-memory must be followed by a positive integer" );
            exit(1);
        }
        if (ramSize < 32 || ramSize > 4096) {
            derror( "physical memory size must be between 32 and 4096 MB" );
            exit(1);
        }
    }
    if (!opts->memory) {
        int ramSize = hw->hw_ramSize;
        if (ramSize <= 0) {
            /* Compute the default RAM size based on the size of screen.
             * This is only used when the skin doesn't provide the ram
             * size through its hardware.ini (i.e. legacy ones) or when
             * in the full Android build system.
             */
            int64_t pixels  = get_screen_pixels(skinConfig);

            /* The following thresholds are a bit liberal, but we
             * essentially want to ensure the following mappings:
             *
             *   320x480 -> 96
             *   800x600 -> 128
             *  1024x768 -> 256
             *
             * These are just simple heuristics, they could change in
             * the future.
             */
            if (pixels <= 250000)
                ramSize = 96;
            else if (pixels <= 500000)
                ramSize = 128;
            else
                ramSize = 256;
        }
        bufprint(tmp, tmpend, "%d", ramSize);
        opts->memory = android_strdup(tmp);
    }

    if (opts->trace) {
        args[n++] = "-trace";
        args[n++] = opts->trace;
        args[n++] = "-tracing";
        args[n++] = "off";
    }

    /* Pass LCD density value to the core. */
    snprintf(lcd_density, sizeof(lcd_density), "%d", hw->hw_lcd_density);
    args[n++] = "-lcd-density";
    args[n++] = lcd_density;

    /* Pass boot properties to the core. */
    if (opts->prop != NULL) {
        ParamList*  pl = opts->prop;
        for ( ; pl != NULL; pl = pl->next ) {
            args[n++] = "-boot-property";
            args[n++] = pl->param;
        }
    }

    args[n++] = "-append";

    if (opts->bootchart) {
        char*  end;
        int    timeout = strtol(opts->bootchart, &end, 10);
        if (timeout == 0)
            opts->bootchart = NULL;
        else if (timeout < 0 || timeout > 15*60) {
            derror( "timeout specified for -bootchart option is invalid.\n"
                    "please use integers between 1 and 900\n");
            exit(1);
        }
    }

    /* Setup the kernel init options
     */
    {
        static char  params[1024];
        char        *p = params, *end = p + sizeof(params);

        p = bufprint(p, end, "qemu=1 console=ttyS0" );

        if (opts->shell || opts->logcat) {
            p = bufprint(p, end, " androidboot.console=ttyS%d", shell_serial );
        }

        if (opts->trace) {
            p = bufprint(p, end, " android.tracing=1");
        }

        if (!opts->no_jni) {
            p = bufprint(p, end, " android.checkjni=1");
        }

        if (opts->no_boot_anim) {
            p = bufprint( p, end, " android.bootanim=0" );
        }

        if (opts->logcat) {
            char*  q = bufprint(p, end, " androidboot.logcat=%s", opts->logcat);

            if (q < end) {
                /* replace any space by a comma ! */
                {
                    int  nn;
                    for (nn = 1; p[nn] != 0; nn++)
                        if (p[nn] == ' ' || p[nn] == '\t')
                            p[nn] = ',';
                    p += nn;
                }
            }
            p = q;
        }

        if (opts->old_system)
        {
            p = bufprint(p, end, " android.ril=ttyS%d", radio_serial);

            if (opts->gps) {
                p = bufprint(p, end, " android.gps=ttyS%d", gps_serial);
            }
        }
        else
        {
            p = bufprint(p, end, " android.qemud=ttyS%d", qemud_serial);
        }

        if (opts->bootchart) {
            p = bufprint(p, end, " androidboot.bootchart=%s", opts->bootchart);
        }

        if (p >= end) {
            fprintf(stderr, "### ERROR: kernel parameters too long\n");
            exit(1);
        }

        args[n++] = strdup(params);
    }

    if (opts->ports) {
        args[n++] = "-android-ports";
        args[n++] = opts->ports;
    }

    if (opts->port) {
        args[n++] = "-android-port";
        args[n++] = opts->port;
    }

    if (opts->report_console) {
        args[n++] = "-android-report-console";
        args[n++] = opts->report_console;
    }

    if (opts->http_proxy) {
        args[n++] = "-http-proxy";
        args[n++] = opts->http_proxy;
    }

    if (opts->charmap) {
        args[n++] = "-charmap";
        args[n++] = opts->charmap;
    }

    if (opts->memcheck) {
        args[n++] = "-android-memcheck";
        args[n++] = opts->memcheck;
    }

    /* physical memory */
    args[n++] = "-m";
    args[n++] = opts->memory;

    /* on Linux, the 'dynticks' clock sometimes doesn't work
     * properly. this results in the UI freezing while emulation
     * continues, for several seconds...
     */
#ifdef __linux__
    args[n++] = "-clock";
    args[n++] = "unix";
#endif

    /* Set up the interfaces for inter-emulator networking */
    if (opts->shared_net_id) {
        unsigned int shared_net_id = atoi(opts->shared_net_id);
        char nic[37];

        args[n++] = "-net";
        args[n++] = "nic,vlan=0";
        args[n++] = "-net";
        args[n++] = "user,vlan=0";

        args[n++] = "-net";
        snprintf(nic, sizeof nic, "nic,vlan=1,macaddr=52:54:00:12:34:%02x", shared_net_id);
        args[n++] = strdup(nic);
        args[n++] = "-net";
        args[n++] = "socket,vlan=1,mcast=230.0.0.10:1234";
    }

    while(argc-- > 0) {
        args[n++] = *argv++;
    }
    args[n] = 0;

    /* Generate a temporary hardware.ini for this AVD. The real hardware
     * configuration is ususally stored in several files, e.g. the AVD's
     * config.ini plus the skin-specific hardware.ini.
     *
     * The new temp file will group all definitions and will be used to
     * launch the core with the -android-hw <file> option.
     */
    {
        const char* coreHwIniPath = avdInfo_getCoreHwIniPath(avd);
        IniFile*    hwIni         = iniFile_newFromMemory("", NULL);
        androidHwConfig_write(hw, hwIni);
        if (iniFile_saveToFile(hwIni, coreHwIniPath) < 0) {
            derror("Could not write hardware.ini to %s: %s", coreHwIniPath, strerror(errno));
            exit(2);
        }
        args[n++] = "-android-hw";
        args[n++] = strdup(coreHwIniPath);
    }

    if(VERBOSE_CHECK(init)) {
        int i;
        printf("QEMU options list:\n");
        for(i = 0; i < n; i++) {
            printf("emulator: argv[%02d] = \"%s\"\n", i, args[i]);
        }
        /* Dump final command-line option to make debugging the core easier */
        printf("Concatenated QEMU options:\n");
        for (i = 0; i < n; i++) {
            printf(" %s", args[i]);
        }
        printf("\n");
    }

    /* Setup SDL UI just before calling the code */
    init_sdl_ui(skinConfig, skinPath, opts);

    // Lets see if we're attaching to a running core process here.
    if (opts->attach_core) {
        if (attach_to_core(opts)) {
            return -1;
        }
        // Connect to the core's UI control services.
        if (coreCmdProxy_create(attachUiImpl_get_console_socket())) {
            return -1;
        }
        // Connect to the core's user events service.
        if (userEventsProxy_create(attachUiImpl_get_console_socket())) {
            return -1;
        }
    }

    return qemu_main(n, args);
}
