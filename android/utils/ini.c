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
#include "android/utils/ini.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "android_debug.h"
#include "osdep.h"

/* W() is used to print warnings, D() to print debugging info */
#define  W(...)   dwarning(__VA_ARGS__)
#define  D(...)   VERBOSE_PRINT(vm_config,__VA_ARGS__)

/* a simple .ini file parser and container for Android
 * no sections support. see android/utils/ini.h for
 * more details on the supported file format.
 */
typedef struct {
    char*  key;
    char*  value;
} IniPair;

struct IniFile {
    int       numPairs;
    int       maxPairs;
    IniPair*  pairs;
};

void
iniFile_free( IniFile*  i )
{
    int  nn;
    for (nn = 0; nn < i->numPairs; nn++) {
        free(i->pairs[nn].key);
        i->pairs[nn].key   = NULL;
        i->pairs[nn].value = NULL;
    }
    free(i->pairs);
    free(i);
}

static IniFile*
iniFile_alloc( void )
{
    IniFile*  i = calloc(1, sizeof(*i));
    return i;
}

static void
iniFile_addPair( IniFile*  i, const char*  key,   int  keyLen,
                                    const char*  value, int  valueLen )
{
    IniPair*  pair;

    if (i->numPairs >= i->maxPairs) {
        int       oldMax   = i->maxPairs;
        int       newMax   = oldMax + (oldMax >> 1) + 4;
        IniPair*  newPairs = realloc(i->pairs, newMax*sizeof(newPairs[0]));

        i->pairs    = newPairs;
        i->maxPairs = newMax;
    }

    pair = i->pairs + i->numPairs;

    pair->key   = malloc(keyLen + valueLen + 2);
    memcpy(pair->key, key, keyLen);
    pair->key[keyLen] = 0;

    pair->value = pair->key + keyLen + 1;
    memcpy(pair->value, value, valueLen);
    pair->value[valueLen] = 0;

    i->numPairs += 1;
}

const char*
iniFile_getValue( IniFile*  i, const char*  key )
{
    if (i && key) {
        int  nn;

        for (nn = 0; nn < i->numPairs; nn++) {
            if (!strcmp(i->pairs[nn].key,key))
                return i->pairs[nn].value;
        }
    }
    return NULL;
}

int
iniFile_getPairCount( IniFile*  i )
{
    return i ? i->numPairs : 0;
}

void
iniFile_getPair( IniFile*   i, 
                    int           index, 
                    const char*  *pKey,
                    const char*  *pValue )
{
    const char*  key   = NULL;
    const char*  value = NULL;

    if (i && index >= 0 && index < i->numPairs) {
        key   = i->pairs[index].key;
        value = i->pairs[index].value;
    }
    *pKey   = key;
    *pValue = value;
}

/* NOTE: we avoid using <ctype.h> functions to avoid locale-specific
 *       behaviour that can be the source of strange bugs.
 */

static const char*
skipSpaces( const char* p )
{
    while (*p == ' ' || *p == '\t')
        p ++;
    return p;
}

static const char*
skipToEOL( const char*  p )
{
    while (*p && (*p != '\n' && *p != '\r'))
        p ++;

    if (*p) {
        p ++;
        if (p[-1] == '\r' && p[0] == '\n')
            p ++;
    }
    return p;
}

static int
isKeyStartChar( int  c )
{
    return ((unsigned)(c-'a') < 26 ||
            (unsigned)(c-'A') < 26 ||
            c == '_');
}

static int
isKeyChar( int  c )
{
    return isKeyStartChar(c) || ((unsigned)(c-'0') < 10) || (c == '.');
}

IniFile*
iniFile_newFromMemory( const char*  text, const char*  fileName )
{
    const char*  p      = text;
    IniFile*     ini    = iniFile_alloc();
    int          lineno = 0;

    if (!fileName)
        fileName = "<unknownFile>";

    D("%s: parsing as .ini file", fileName);

    while (*p) {
        const char*  key;
        int          keyLen;
        const char*  value;
        int          valueLen;

        lineno += 1;

        /* skip leading whitespace */
        p = skipSpaces(p);

        /* skip comments and empty lines */
        if (*p == 0 || *p == ';' || *p == '#' || *p == '\n' || *p == '\r') {
            p = skipToEOL(p);
            continue;
        }

        /* check the key name */
        key = p++;
        if (!isKeyStartChar(*key)) {
            p = skipToEOL(p);
            W("%s:%d: key name doesn't start with valid character. line ignored",
              fileName, lineno);
            continue;
        }

        while (isKeyChar(*p))
            p++;

        keyLen = p - key;
        p      = skipSpaces(p);

        /* check the equal */
        if (*p != '=') {
            W("%s:%d: missing expected assignment operator (=). line ignored",
              fileName, lineno);
            p = skipToEOL(p);
            continue;
        }
        p += 1;

        /* skip spaces before the value */
        p     = skipSpaces(p);
        value = p;

        /* find the value */
        while (*p && (*p != '\n' && *p != '\r'))
            p += 1;

        /* remove trailing spaces */
        while (p > value && (p[-1] == ' ' || p[-1] == '\t'))
            p --;

        valueLen = p - value;

        iniFile_addPair(ini, key, keyLen, value, valueLen);
        D("%s:%d: KEY='%.*s' VALUE='%.*s'", fileName, lineno,
          keyLen, key, valueLen, value);

        p = skipToEOL(p);
    }

    return ini;
}

IniFile*
iniFile_newFromFile( const char*  filepath )
{
    FILE*        fp = fopen(filepath, "rt");
    char*        text;
    long         size;
    IniFile*     ini = NULL;

    if (fp == NULL) {
        W("could not open .ini file: %s: %s",
          filepath, strerror(errno));
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* avoid reading a very large file that was passed by mistake
     * this threshold is quite liberal.
     */
#define  MAX_INI_FILE_SIZE  655360

    if (size < 0 || size > MAX_INI_FILE_SIZE) {
        W("hardware configuration file '%s' too large (%ld bytes)",
          filepath, size);
        goto EXIT;
    }

    /* read the file, add a sentinel at the end of it */
    text = malloc(size+1);
    fread(text, 1, size, fp);
    text[size] = 0;

    ini = iniFile_newFromMemory(text, filepath);
    free(text);

EXIT:
    fclose(fp);
    return ini;
}

char*
iniFile_getString( IniFile*  f, const char*  key )
{
    const char*  val = iniFile_getValue(f, key);

    if (!val)
        return NULL;

    return qemu_strdup(val);
}

int
iniFile_getInteger( IniFile*  f, const char*  key, int  defaultValue )
{
    const char*  valueStr = iniFile_getValue(f, key);
    int          value    = defaultValue;

    if (valueStr != NULL) {
        char*  end;
        long   l = strtol(valueStr, &end, 10);
        if (end != NULL && end[0] == 0 && (int)l == l)
            value = l;
    }
    return value;
}

double
iniFile_getDouble( IniFile*  f, const char*  key, double  defaultValue )
{
    const char*  valueStr = iniFile_getValue(f, key);
    double       value    = defaultValue;

    if (valueStr != NULL) {
        char*   end;
        double  d = strtod(valueStr, &end);
        if (end != NULL && end[0] == 0)
            value = d;
    }
    return value;
}

int
iniFile_getBoolean( IniFile*  f, const char*  key, const char*  defaultValue )
{
    const char*  value  = iniFile_getValue(f, key);

    if (!value)
        value = defaultValue;

    if (!strcmp(value,"1")    ||
        !strcmp(value,"yes")  ||
        !strcmp(value,"YES")  ||
        !strcmp(value,"true") ||
        !strcmp(value,"TRUE"))
    {
        return 1;
    }
    else
        return 0;
}

int64_t
iniFile_getDiskSize( IniFile*  f, const char*  key, const char*  defaultValue )
{
    const char*    valStr = iniFile_getValue(f, key);
    int64_t        value  = 0;

    if (!valStr)
        valStr = defaultValue;

    if (valStr != NULL) {
        char*  end;

        value = strtoll(valStr, &end, 10);
        if (*end == 'k' || *end == 'K')
            value *= 1024ULL;
        else if (*end == 'm' || *end == 'M')
            value *= 1024*1024ULL;
        else if (*end == 'g' || *end == 'G')
            value *= 1024*1024*1024ULL;
    }
    return value;
}
