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
#include "android/hw-events.h"
#include "android/utils/bufprint.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char*  name;
    int          value;
} PairRec;

#define  EV_TYPE(n,v)   { STRINGIFY(n), (v) },
#define  BTN_CODE(n,v)  { STRINGIFY(n), (v) },
#define  REL_CODE(n,v)  { STRINGIFY(n), (v) },
#define  ABS_CODE(n,v)  { STRINGIFY(n), (v) },

static const PairRec  _ev_types_tab[] =
{
    EVENT_TYPE_LIST
    { NULL, 0 }
};

static const PairRec  _btn_codes_list[] =
{
    EVENT_BTN_LIST
    { NULL, 0 }
};

static const PairRec  _rel_codes_list[] =
{
    EVENT_REL_LIST
    { NULL, 0 }
};

static const PairRec  _abs_codes_list[] =
{
    EVENT_ABS_LIST
    { NULL, 0 }
};

#undef EV_TYPE
#undef BTN_CODE
#undef REL_CODE
#undef ABS_CODE

static int
count_list( const PairRec*  list )
{
    int  nn = 0;
    while (list[nn].name != NULL)
        nn += 1;

    return nn;
}

static int
scan_list( const PairRec*  list,
           const char*     prefix,
           const char*     name,
           int             namelen )
{
    int   len;

    if (namelen <= 0)
        return -1;

    len = strlen(prefix);
    if (namelen <= len)
        return -1;
    if ( memcmp( name, prefix, len ) != 0 )
        return -1;

    name    += len;
    namelen -= len;

    for ( ; list->name != NULL; list += 1 )
    {
        if ( memcmp( list->name, name, namelen ) == 0 && list->name[namelen] == 0 )
            return list->value;
    }
    return -1;
}


typedef struct {
    int             type;
    const char*     prefix;
    const PairRec*  pairs;
} TypeListRec;

typedef const TypeListRec*  TypeList;

static const TypeListRec  _types_list[] =
{
    { EV_KEY, "BTN_", _btn_codes_list },
    { EV_REL, "REL_", _rel_codes_list },
    { EV_ABS, "ABS_", _abs_codes_list },
    { -1, NULL, NULL }
};


static TypeList
find_type_list( int  type )
{
    TypeList  list = _types_list;

    for ( ; list->type >= 0; list += 1 )
        if (list->type == type)
            return list;

    return NULL;
}


int
android_event_from_str( const char*  name,
                        int         *ptype,
                        int         *pcode,
                        int         *pvalue )
{
    const char*  p;
    const char*  pend;
    const char*  q;
    TypeList     list;
    char*        end;

    *ptype  = 0;
    *pcode  = 0;
    *pvalue = 0;

    p    = name;
    pend = p + strcspn(p, " \t");
    q    = strchr(p, ':');
    if (q == NULL || q > pend)
        q = pend;

    *ptype = scan_list( _ev_types_tab, "EV_", p, q-p );
    if (*ptype < 0) {
        *ptype = (int) strtol( p, &end, 0 );
        if (end != q)
            return -1;
    }

    if (*q != ':')
        return 0;

    p = q + 1;
    q = strchr(p, ':');
    if (q == NULL || q > pend)
        q = pend;

    list = find_type_list( *ptype );

    *pcode = -1;
    if (list != NULL) {
        *pcode = scan_list( list->pairs, list->prefix, p, q-p );
    }
    if (*pcode < 0) {
        *pcode = (int) strtol( p, &end, 0 );
        if (end != q)
            return -2;
    }

    if (*q != ':')
        return 0;

    p = q + 1;
    q = strchr(p, ':');
    if (q == NULL || q > pend)
        q = pend;

    *pvalue = (int)strtol( p, &end, 0 );
    if (end != q)
        return -3;

    return 0;
}

int
android_event_get_type_count( void )
{
    return count_list( _ev_types_tab );
}

char*
android_event_bufprint_type_str( char*  buff, char*  end, int  type_index )
{
    return bufprint( buff, end, "EV_%s", _ev_types_tab[type_index].name );
}

/* returns the list of valid event code string aliases for a given event type */
int
android_event_get_code_count( int  type )
{
    TypeList  list = find_type_list(type);

    if (list == NULL)
        return 0;

    return count_list( list->pairs );
}

char*
android_event_bufprint_code_str( char*  buff, char*  end, int  type, int  code_index )
{
    TypeList  list = find_type_list(type);

    if (list == NULL)
        return buff;

    return bufprint( buff, end, "%s%s", list->prefix, list->pairs[code_index].name );
}

