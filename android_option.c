#include "android_option.h"
#include "android_debug.h"
#include "android_utils.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define  _VERBOSE_TAG(x,y)   { #x, VERBOSE_##x, y },
static const struct { const char*  name; int  flag; const char*  text; }
debug_tags[] = {
    VERBOSE_TAG_LIST
    { 0, 0, 0 }
};

static void  parse_debug_tags( const char*  tags );
static void  parse_env_debug_tags( void );


typedef struct {
    const char*  name;
    int          var_offset;
    int          var_is_param;
    int          var_is_config;
} OptionInfo;

#define  OPTION(_name,_type,_config)  \
    { #_name, offsetof(AndroidOptions,_name), _type, _config },


static const OptionInfo  option_keys[] = {
#define  OPT_PARAM(_name,_template,_descr)  OPTION(_name,1,0)
#define  OPT_FLAG(_name,_descr)             OPTION(_name,0,0)
#define  CFG_PARAM(_name,_template,_descr)  OPTION(_name,1,1)
#define  CFG_FLAG(_name,_descr)             OPTION(_name,0,1)
#include "android_options.h"
    { NULL, 0, 0, 0 }
};

int
android_parse_options( int  *pargc, char**  *pargv, AndroidOptions*  opt )
{
    int     nargs = *pargc-1;
    char**  aread = *pargv+1;
    char**  awrite = aread;

    memset( opt, 0, sizeof *opt );

    while (nargs > 0 && aread[0][0] == '-') {
        char*  arg = aread[0]+1;
        int    nn;

        /* an option cannot contain an underscore */
        if (strchr(arg, '_') != NULL) {
            break;
        }

        nargs--;
        aread++;

        /* for backwards compatibility with previous versions */
        if (!strcmp(arg, "verbose")) {
            arg = "debug-init";
        }

        /* special handing for -debug <tags> */
        if (!strcmp(arg, "debug")) {
            if (nargs == 0) {
                derror( "-debug must be followed by tags (see -help-verbose)\n");
                exit(1);
            }
            nargs--;
            parse_debug_tags(*aread++);
            continue;
        }

        /* special handling for -debug-<tag> and -debug-no-<tag> */
        if (!strncmp(arg, "debug-", 6)) {
            int            remove = 0;
            unsigned long  mask   = 0;
            arg += 6;
            if (!strncmp(arg, "no-", 3)) {
                arg   += 3;
                remove = 1;
            }
            if (!strcmp(arg, "all")) {
                mask = ~0;
            }
            for (nn = 0; debug_tags[nn].name; nn++) {
                if (!strcmp(arg, debug_tags[nn].name)) {
                    mask = (1 << debug_tags[nn].flag);
                    break;
                }
            }
            if (remove)
                android_verbose &= ~mask;
            else
                android_verbose |= mask;
            continue;
        }

        /* look into our table of options
         *
         * NOTE: the 'option_keys' table maps option names
         * to field offsets into the AndroidOptions structure.
         *
         * however, the names stored in the table used underscores
         * instead of dashes. this means that the command-line option
         * '-foo-bar' will be associated to the name 'foo_bar' in
         * this table, and will point to the field 'foo_bar' or
         * AndroidOptions.
         *
         * as such, before comparing the current option to the
         * content of the table, we're going to translate dashes
         * into underscores.
         */
        {
            const OptionInfo*  oo = option_keys;
            char               arg2[64];
            int                len = strlen(arg);

            /* copy into 'arg2' buffer, translating dashes
             * to underscores. note that we truncate to 63
             * characters, which should be enough in practice
             */
            if (len > sizeof(arg2)-1)
                len = sizeof(arg2)-1;

            memcpy(arg2, arg, len+1);
            buffer_translate_char(arg2, len, '-', '_'); 

            for ( ; oo->name; oo++ ) {
                if ( !strcmp( oo->name, arg2 ) ) {
                    void*  field = (char*)opt + oo->var_offset;

                    if (oo->var_is_param) {
                        /* parameter option */
                        if (nargs == 0) {
                            derror( "-%s must be followed by parameter (see -help-%s)",
                                    arg, arg );
                            exit(1);
                        }
                        nargs--;
                        ((char**)field)[0] = *aread++;
                    } else {
                        /* flag option */
                        ((int*)field)[0] = 1;
                    }
                    break;
                }
            }

            if (oo->name == NULL) {  /* unknown option ? */
                nargs++;
                aread--;
                break;
            }
        }
    }

    /* copy remaining parameters, if any, to command line */
    *pargc = nargs + 1;

    while (nargs > 0) {
        awrite[0] = aread[0];
        awrite ++;
        aread  ++;
        nargs  --;
    }

    awrite[0] = NULL;

    return 0;
}



/* special handling of -debug option and tags */
#define  ENV_DEBUG   "ANDROID_DEBUG"

static void
parse_debug_tags( const char*  tags )
{
    char*        x;
    char*        y;
    char*        x0;

    if (tags == NULL)
        return;

    x = x0 = strdup(tags);
    while (*x) {
        y = strchr(x, ',');
        if (y == NULL)
            y = x + strlen(x);
        else
            *y++ = 0;

        if (y > x+1) {
            int  nn, remove = 0;
            unsigned mask = 0;

            if (x[0] == '-') {
                remove = 1;
                x += 1;
            }

            if (!strcmp( "all", x ))
                mask = ~0;
            else {
                for (nn = 0; debug_tags[nn].name != NULL; nn++) {
                    if ( !strcmp( debug_tags[nn].name, x ) ) {
                        mask |= (1 << debug_tags[nn].flag);
                        break;
                    }
                }
            }

            if (mask == 0)
                dprint( "ignoring unknown " ENV_DEBUG " item '%s'", x );
            else {
                if (remove)
                    android_verbose &= ~mask;
                else
                    android_verbose |= mask;
            }
        }
        x = y;
    }

    free(x0);
}


static void
parse_env_debug_tags( void )
{
    const char*  env = getenv( ENV_DEBUG );
    parse_debug_tags( env );
}

