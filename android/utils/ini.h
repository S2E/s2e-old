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
#ifndef _ANDROID_UTILS_INI_H
#define _ANDROID_UTILS_INI_H

#include <stdint.h>

/* the emulator supports a simple .ini file format for its configuration
 * files. Here's the BNF for it:
 *
 *  file             := <line>*
 *  line             := <comment> | <LF> | <assignment>
 *  comment          := (';'|'#') <noLF>* <LF>
 *  assignment       := <space>* <keyName> <space>* '=' <space>* <valueString> <space>* <LF>
 *  keyName          := <keyNameStartChar> <keyNameChar>*
 *  keyNameStartChar := [A-Za-z_]
 *  keyNameChar      := [A-Za-z0-9_.-]
 *  valueString      := <noLF>*
 *  space            := ' ' | '\t'
 *  LF               := '\r\n' | '\n' | '\r'
 *  noLF             := [^<LF>]
 *
 * Or, in English:
 *
 * - no support for sections
 * - empty lines are ignored, as well as lines beginning with ';' or '#'
 * - lines must be of the form: "<keyName> = <value>"
 * - key names must start with a letter or an underscore
 * - other key name characters can be letters, digits, underscores, dots or dashes
 *
 * - leading and trailing space are allowed and ignored before/after the key name
 *   and before/after the value
 *
 * - there is no restriction on the value, except that it can't contain
 *   leading/trailing space/tab characters or newline/charfeed characters
 *
 * - empty values are possible, and will be stored as an empty string.
 * - any badly formatted line is discarded (and will print a warning)
 *
 */

/* an opaque structure used to model an .ini configuration file */
typedef struct IniFile   IniFile;

/* creates a new IniFile object from a config file loaded in memory.
 *  'fileName' is only used when writing a warning to stderr in case
 * of badly formed output
 */
IniFile*  iniFile_newFromMemory( const char*  text, const char*  fileName  );

/* creates a new IniFile object from a file path,
 * returns NULL if the file cannot be opened.
 */
IniFile*  iniFile_newFromFile( const char*  filePath);

/* try to write an IniFile into a given file.
 * returns 0 on success, -1 on error (see errno for error code)
 */
int       iniFile_saveToFile( IniFile*  f, const char*  filePath );

/* free an IniFile object */
void      iniFile_free( IniFile*  f );

/* returns the number of (key.value) pairs in an IniFile */
int       iniFile_getPairCount( IniFile*  f );

/* return a specific (key,value) pair from an IniFile.
 * if the index is not correct, both '*pKey' and '*pValue' will be
 * set to NULL.
 *
 * you should probably use iniFile_getValue() and its variants instead
 */
void      iniFile_getPair( IniFile*     f,
                           int          index,
                           const char* *pKey,
                           const char* *pValue );

/* returns the value of a given key from an IniFile.
 * NULL if the key is not assigned in the corresponding configuration file
 */
const char*  iniFile_getValue( IniFile*  f, const char*  key );

/* returns a copy of the value of a given key, or NULL
 */
char*   iniFile_getString( IniFile*  f, const char*  key );

/* returns an integer value, or a default in case the value string is
 * missing or badly formatted
 */
int     iniFile_getInteger( IniFile*  f, const char*  key, int  defaultValue );

/* returns a 64-bit integer value, or a default in case the value string is
 * missing or badly formatted
 */
int64_t iniFile_getInt64( IniFile*  f, const char*  key, int64_t  defaultValue );

/* returns a double value, or a default in case the value string is
 * missing or badly formatted
 */
double  iniFile_getDouble( IniFile*  f, const char*  key, double  defaultValue );

/* returns a copy of a given key's value, if any, or NULL if it is missing
 * caller must call free() to release it */
char*   iniFile_getString( IniFile*  f, const char*  key );

/* parses a key value as a boolean. Accepted values are "1", "0", "yes", "YES",
 * "no" and "NO". Returns either 1 or 0.
 * note that the default value must be provided as a string too
 */
int     iniFile_getBoolean( IniFile*  f, const char*  key, const char*  defaultValue );

/* parses a key value as a disk size. this means it can be an integer followed
 * by a suffix that can be one of "mMkKgG" which correspond to KiB, MiB and GiB
 * multipliers.
 *
 * NOTE: we consider that 1K = 1024, not 1000.
 */
int64_t  iniFile_getDiskSize( IniFile*  f, const char*  key, const char*  defaultValue );

/* */

#endif /* _ANDROID_UTILS_INI_H */
