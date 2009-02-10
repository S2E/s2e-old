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
#include "android/utils/reflist.h"
#include <stdlib.h>
#include <string.h>

void
areflist_setEmpty(ARefList*  l)
{
    if (l->iteration > 0) {
        /* deferred empty, set all items to NULL
         * to stop iterations */
        void**  items = areflist_items(l);
        memset(items, 0, l->count*sizeof(items[0]));
        l->iteration |= 1;
    } else {
        /* direct empty */
        if (l->max > 1) {
            free(l->u.items);
            l->max = 1;
        }
    }
    l->count = 0;
}

int
areflist_indexOf(ARefList*  l, void*  item)
{
    if (item) {
        void**  items = (l->max == 1) ? &l->u.item0 : l->u.items;
        void**  end   = items + l->count;
        void**  ii    = items;

        for ( ; ii < end; ii += 1 )
            if (*ii == item)
                return (ii - items);
    }
    return -1;
}

static void
areflist_grow(ARefList*  l, int  count)
{
    int   newcount = l->count + count;
    if (newcount > l->max) {
        int  oldmax = l->max == 1 ? 0 : l->max;
        int  newmax = oldmax;
        void**  olditems = l->max == 1 ? NULL : l->u.items;
        void**  newitems;

        while (oldmax < newcount)
            oldmax += (oldmax >> 1) + 4;

        newitems = realloc(olditems, newmax*sizeof(newitems[0]));

        l->u.items = newitems;
        l->max     = (uint16_t) newmax;
    }
}


void
areflist_add(ARefList*  l, void*  item)
{
    if (item) {
        void**  items;

        if (l->count >= l->max) {
            areflist_grow(l, 1);
        }
        items = areflist_items(l);
        items[l->count] = item;
        l->count += 1;
    }
}

void*
areflist_pop(ARefList*  l)
{
    void*   item = NULL;
    void**  items = areflist_items(l);

    if (l->count > 0) {
        if (l->iteration > 0) {
            /* deferred deletion */
            int  nn;
            for (nn = l->count-1; nn > 0; nn--) {
                item = items[nn];
                if (item != NULL) {
                    l->count -= 1;
                    return item;
                }
            }
        } else {
            /* normal pop */
            item = items[--l->count];
            if (l->count <= 0 && l->max > 1) {
                free(l->u.items);
                l->max = 1;
            }
        }
    }
    return item;
}

ABool
areflist_del(ARefList*  l, void*  item)
{
    if (item) {
        int  index = areflist_indexOf(l, item);
        if (index >= 0) {
            void** items = areflist_items(l);

            if (l->iteration > 0) {
                /* deferred deletion */
                items[index]  = NULL;
                l->iteration |= 1;
            } else {
                /* direct deletion */
                if (l->max > 1) {
                    memmove(items + index, items + index + 1, l->count - index - 1);
                    if (--l->count == 0) {
                        free(l->u.items);
                        l->max = 1;
                    }
                } else {
                    l->u.item0 = NULL;
                    l->count   = 0;
                }
            }
            return 1;
        }
    }
    return 0;
}

void
_areflist_remove_deferred(ARefList*  l)
{
    if (l->iteration & 1) {
        /* remove all NULL elements from the array */
        void**  items = areflist_items(l);
        int     rr = 0;
        int     ww = 0;
        for ( ; rr < l->count; rr++ ) {
            if (items[rr] != NULL)
                items[ww++] = items[rr];
        }
        l->count = (int16_t) ww;

        /* if the list is empty, release its array */
        if (l->count == 0 && l->max > 1) {
            free(l->u.items);
            l->max = 1;
        }
    }
    l->iteration = 0;
}

void  
areflist_copy(ARefList*  dst, ARefList*  src)
{
    dst[0] = src[0];

    if (src->max > 1) {
        dst->u.items = malloc( dst->count*sizeof(void*) );
        dst->max     = dst->count;
    }
}

void*
areflist_get(ARefList*  l, int  n)
{
    if ((unsigned)n >= (unsigned)l->count)
        return NULL;

    if (l->max == 1)
        return l->u.item0;

    return l->u.items[n];
}

void**
areflist_at(ARefList*  l, int  n)
{
    void**  items;

    if ((unsigned)n >= (unsigned)l->count)
        return NULL;

    items = (l->max == 1) ? &l->u.item0 : l->u.items;

    return items + n;
}
