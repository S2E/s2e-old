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
#include "android/utils/path.h"
#include "android/utils/tempfile.h"

#include "android/main-common.h"
#include "android/help.h"
#include "hw/goldfish_nand.h"

#include "android/globals.h"

#include "android/qemulator.h"
#include "android/display.h"

#include "android/snapshot.h"

#include "android/framebuffer.h"
#include "iolooper.h"

#ifdef TARGET_I386
nand_threshold android_nand_read_threshold;
nand_threshold android_nand_write_threshold;
#endif

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

    AndroidHwConfig*  hw;
    AvdInfo*          avd;
    AConfig*          skinConfig;
    char*             skinPath;
    int               inAndroidBuild;

    AndroidOptions  opts[1];
    /* net.shared_net_ip boot property value. */
    char boot_prop_ip[64];
    boot_prop_ip[0] = '\0';

    args[0] = argv[0];

    if ( android_parse_options( &argc, &argv, opts ) < 0 ) {
        exit(1);
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

    if (android_charmap_setup(opts->charmap)) {
        exit(1);
    }

    if (opts->nojni)
        opts->no_jni = opts->nojni;

    if (opts->nocache)
        opts->no_cache = opts->nocache;

    if (opts->noaudio)
        opts->no_audio = opts->noaudio;

    if (opts->noskin)
        opts->no_skin = opts->noskin;

    /* Parses options and builds an appropriate AVD. */
    avd = android_avdInfo = createAVD(opts, &inAndroidBuild) ;

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

    /* Update HW config with the AVD information. */
    updateHwConfigFromAVD(hw, avd, opts, inAndroidBuild);

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
    parse_skin_files(opts->skindir, opts->skin, opts, hw,
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

    {
        const char*  filetype = "file";

        // TODO: This should go to core!
        if (avdInfo_isImageReadOnly(avd, AVD_IMAGE_INITSYSTEM))
            filetype = "initfile";

        bufprint(tmp, tmpend,
             "system,size=0x%x,%s=%s", (uint32_t)hw->disk_systemPartition_size,
             filetype, avdInfo_getImageFile(avd, AVD_IMAGE_INITSYSTEM));

        args[n++] = "-nand";
        args[n++] = strdup(tmp);
    }

    bufprint(tmp, tmpend,
             "userdata,size=0x%x,file=%s",
             (uint32_t)hw->disk_dataPartition_size,
             avdInfo_getImageFile(avd, AVD_IMAGE_USERDATA));

    args[n++] = "-nand";
    args[n++] = strdup(tmp);

    if (hw->disk_cachePartition) {
        // TODO: This should go to core
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

     // TODO: This should go to core
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
        // TODO: This should go to core
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
        if (opts->no_snapshot_update_time) {
            android_snapshot_update_time = 0;
        }
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
        hw->hw_ramSize = ramSize;
    }
    if (!opts->memory) {
        int ramSize = hw->hw_ramSize;
        if (ramSize <= 0) {
            /* Compute the default RAM size based on the size of screen.
             * This is only used when the skin doesn't provide the ram
             * size through its hardware.ini (i.e. legacy ones) or when
             * in the full Android build system.
             */
            int64_t pixels  = hw->hw_lcd_width * hw->hw_lcd_height;
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
        hw->hw_ramSize = ramSize;
    }

    D("Physical RAM size: %dMB\n", hw->hw_ramSize);

    if (hw->vm_heapSize == 0) {
        /* Compute the default heap size based on the RAM size.
         * Essentially, we want to ensure the following liberal mappings:
         *
         *   96MB RAM -> 16MB heap
         *  128MB RAM -> 24MB heap
         *  256MB RAM -> 48MB heap
         */
        int  ramSize = hw->hw_ramSize;
        int  heapSize;

        if (ramSize < 100)
            heapSize = 16;
        else if (ramSize < 192)
            heapSize = 24;
        else
            heapSize = 48;

        hw->vm_heapSize = heapSize;
    }

    if (opts->trace) {
        args[n++] = "-trace";
        args[n++] = opts->trace;
        args[n++] = "-tracing";
        args[n++] = "off";
    }

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
#ifdef TARGET_I386
        p = bufprint(p, end, " androidboot.hardware=goldfish");
        p = bufprint(p, end, " clocksource=pit");
#endif

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

    /* physical memory is now in hw->hw_ramSize */

    /* on Linux, the 'dynticks' clock sometimes doesn't work
     * properly. this results in the UI freezing while emulation
     * continues, for several seconds...
     */
#ifdef __linux__
    args[n++] = "-clock";
    args[n++] = "unix";
#endif

    args[n++] = "-android-avdname";
    args[n++] = avdInfo_getName(avd);

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

    return qemu_main(n, args);
}
