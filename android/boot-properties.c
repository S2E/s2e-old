/* Copyright (C) 2009 The Android Open Source Project
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

#include "android/boot-properties.h"
#include "android/utils/debug.h"
#include "android/utils/system.h"
#include "android/hw-qemud.h"
#include "android/globals.h"

#define  D(...)  VERBOSE_PRINT(init,__VA_ARGS__)

/* define T_ACTIVE to 1 to debug transport communications */
#define  T_ACTIVE  0

#if T_ACTIVE
#define  T(...)  VERBOSE_PRINT(init,__VA_ARGS__)
#else
#define  T(...)   ((void)0)
#endif

/* this code supports the list of system properties that will
 * be set on boot in the emulated system.
 */

typedef struct BootProperty {
    struct BootProperty*  next;
    char*                 property;
    int                   length;
} BootProperty;

static BootProperty*
boot_property_alloc( const char*  name,  int  namelen,
                     const char*  value, int  valuelen )
{
    int            length = namelen + 1 + valuelen;
    BootProperty*  prop = android_alloc( sizeof(*prop) + length + 1 );
    char*          p;

    prop->next     = NULL;
    prop->property = p = (char*)(prop + 1);
    prop->length   = length;

    memcpy( p, name, namelen );
    p += namelen;
    *p++ = '=';
    memcpy( p, value, valuelen );
    p += valuelen;
    *p = '\0';

    return prop;
}

static BootProperty*   _boot_properties;
static BootProperty**  _boot_properties_tail = &_boot_properties;
static int             _inited;

int
boot_property_add2( const char*  name, int  namelen,
                    const char*  value, int  valuelen )
{
    BootProperty*  prop;

    /* check the lengths
     */
    if (namelen > PROPERTY_MAX_NAME)
        return -1;

    if (valuelen > PROPERTY_MAX_VALUE)
        return -2;

    /* check that there are not invalid characters in the
     * property name
     */
    const char*  reject = " =$*?'\"";
    int          nn;

    for (nn = 0; nn < namelen; nn++) {
        if (strchr(reject, name[nn]) != NULL)
            return -3;
    }

    /* init service if needed */
    if (!_inited) {
        boot_property_init_service();
        _inited = 1;
    }

    D("Adding boot property: '%.*s' = '%.*s'",
      namelen, name, valuelen, value);

    /* add to the internal list */
    prop = boot_property_alloc(name, namelen, value, valuelen);

    *_boot_properties_tail = prop;
    _boot_properties_tail  = &prop->next;

    return 0;
}


int
boot_property_add( const char*  name, const char*  value )
{
    int  namelen = strlen(name);
    int  valuelen = strlen(value);

    return boot_property_add2(name, namelen, value, valuelen);
}



#define SERVICE_NAME  "boot-properties"

static void
boot_property_client_recv( void*         opaque,
                           uint8_t*      msg,
                           int           msglen,
                           QemudClient*  client )
{
    /* the 'list' command shall send all boot properties
     * to the client, then close the connection.
     */
    if (msglen == 4 && !memcmp(msg, "list", 4)) {
        BootProperty*  prop;
        for (prop = _boot_properties; prop != NULL; prop = prop->next) {
            qemud_client_send(client, (uint8_t*)prop->property, prop->length);
        }
        qemud_client_close(client);
        return;
    }

    /* unknown command ? */
    D("%s: ignoring unknown command: %.*s", __FUNCTION__, msglen, msg);
}

static QemudClient*
boot_property_service_connect( void*          opaque,
                               QemudService*  serv,
                               int            channel )
{
    QemudClient*  client;

    client = qemud_client_new( serv, channel, NULL,
                               boot_property_client_recv,
                               NULL );

    qemud_client_set_framing(client, 1);
    return client;
}


void
boot_property_init_service( void )
{
    if (!_inited) {
        QemudService*  serv = qemud_service_register( SERVICE_NAME,
                                                    1, NULL,
                                                    boot_property_service_connect );
        if (serv == NULL) {
            derror("could not register '%s' service", SERVICE_NAME);
            return;
        }
        D("registered '%s' qemud service", SERVICE_NAME);
    }
}



void
boot_property_parse_option( const char*  param )
{
    char* q = strchr(param,'=');
    const char* name;
    const char* value;
    int   namelen, valuelen, ret;

    if (q == NULL) {
        dwarning("boot property missing (=) separator: %s", param);
        return;
    }

    name    = param;
    namelen = q - param;

    value    = q+1;
    valuelen = strlen(name) - (namelen+1);

    ret = boot_property_add2(name, namelen, value, valuelen);
    if (ret < 0) {
        switch (ret) {
        case -1: 
            dwarning("boot property name too long: '%.*s'",
                        namelen, name);
            break;
        case -2:
            dwarning("boot property value too long: '%.*s'",
                        valuelen, value);
            break;
        case -3:
            dwarning("boot property name contains invalid chars: %.*s",
                        namelen, name);
            break;
        }
    }
}
