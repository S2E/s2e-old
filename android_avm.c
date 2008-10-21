#include "android_avm.h"
#include "android_utils.h"
#include <string.h>

int
avm_check_name( const char*  avm_name )
{
    static const char*  goodchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-0123456789.";
    int                 len       = strlen(avm_name);
    int                 slen      = strspn(avm_name, goodchars);

    return (len == slen);
}


char*
avm_bufprint_default_root( char*  p, char* end )
{
    /* at the moment, store in $CONFIG_PATH/VMs */
    return bufprint_config_path(p, end, "VMs" );
}

char*
avm_bufprint_avm_dir( char*  p, char* end, const char*  avm_name, const char*  root_dir )
{
    if (root_dir) {
        int  len = strlen(root_dir);
        /* get rid of trailing path separators */
        while (len > 0 && root_dir[len-1] == PATH_SEP[0])
            len -= 1;

        p = bufprint( p, end, "%.*s", len, root_dir );
    } else {
        p = avm_bufprint_default_root( p, end );

    p = bufprint( p, end, PATH_SEP "%s", avm_name );

    return p;
}



