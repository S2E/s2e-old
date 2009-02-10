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
#ifndef _ANDROID_UTILS_REFLIST_H
#define _ANDROID_UTILS_REFLIST_H

#include "android/utils/system.h"

/* definitions for a smart list of references to generic objects.
 * supports safe deletion and addition while they are being iterated
 * with AREFLIST_FOREACH() macro
 */

typedef struct ARefList {
    uint16_t   count, max;
    uint16_t   iteration;
    union {
        struct ARefList*  next;
        void*             item0;
        void**            items;
    } u;
} ARefList;

AINLINED void  
areflist_init(ARefList*  l)
{
    l->count     = 0;
    l->max       = 1;
    l->iteration = 0;
}

void  areflist_setEmpty(ARefList*  l);

AINLINED void
areflist_done(ARefList*  l)
{
    areflist_setEmpty(l);
}

AINLINED ABool
areflist_isEmpty(ARefList*  l)
{
    return (l->count == 0);
}

int    areflist_indexOf(ARefList*  l, void*  item);

AINLINED ABool 
areflist_has(ARefList*  l, void*  item)
{
    return areflist_indexOf(l, item) >= 0;
}

/* if 'item' is not NULL, append it to the list. An item
 * can be added several times to a list */
void    areflist_add(ARefList*  l, void*  item);

/* if 'item' is not NULL, try to remove it from the list */
/* returns TRUE iff the item was found in the list */
ABool   areflist_del(ARefList*  l, void*  item);

AINLINED void
areflist_push(ARefList*  l, void*  item)
{
    areflist_add(l, item);
}

void*  areflist_pop(ARefList*  l);

AINLINED void**
areflist_items(ARefList*  l)
{
    return (l->max == 1) ? &l->u.item0 : l->u.items;
}

AINLINED int
areflist_count(ARefList*  l)
{
    return l->count;
}

/* return a pointer to the n-th list array entry, 
   or NULL in case of invalid index */
void**  areflist_at(ARefList*  l, int  n);

/* return the n-th array entry, or NULL in case of invalid index */
void*   areflist_get(ARefList*  l, int  n);

/* used internally */
void    _areflist_remove_deferred(ARefList*  l);

#define  AREFLIST_FOREACH(list_,item_,statement_) \
    ({ ARefList*  _reflist   = (list_); \
       int        _reflist_i = 0; \
       int        _reflist_n = _reflist->count; \
       _reflist->iteration += 2; \
       for ( ; _reflist_i < _reflist_n; _reflist_i++ ) { \
           void**  __reflist_at   = areflist_at(_reflist, _reflist_i); \
           void*  item_ = *__reflist_at; \
           if (item_ != NULL) { \
               statement_; \
           } \
       } \
       _reflist->iteration -= 2; \
       if (_reflist->iteration == 1) \
           _areflist_remove_deferred(_reflist); \
    })

/* use this to delete the currently iterated element */
#define  AREFLIST_DEL_ITERATED()  \
    ({ *_reflist_at = NULL; \
       _reflist->iteration |= 1; })

/* use this to replace the currently iterated element */
#define  AREFLIST_SET_ITERATED(item) \
    ({ *_reflist_at = (item); \
       if (item == NULL) _reflist->iteration |= 1; })

void  areflist_copy(ARefList*  dst, ARefList*  src);

#endif /* _ANDROID_UTILS_REFLIST_H */
