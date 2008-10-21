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
#ifndef ANDROID_VIRTUAL_MACHINE_H
#define ANDROID_VIRTUAL_MACHINE_H

/* An Android Virtual Machine (AVM for short) corresponds to
 * a directory containing all system + data disk images for a
 * given virtual device, plus some AVM-specific configuration
 * files.
 */

/* checks that the name of an AVM doesn't contain fancy characters
 * returns 1 on success, 0 on failure
 */
extern int     avm_check_name( const char*  avm_name );

/* bufprint the path of the default root directory where all AVMs are stored
 * this is normally $HOME/.android/$SDK_VERSION/VMs on Unix
 */
extern char*   avm_bufprint_default_root( char*  p, char*  end );

/* bufprint the path of a given AVM's directory
 * if 'root' is non NULL, this will be $root/$avm_name
 * otherwise, it will be the result of avm_bufprint_defalt_root followed by avm_name
 */
extern char*   avm_bufprint_avm_dir( char*  p, char*  end, const char*  avm_name, const char*  root_dir );



#endif /* ANDROID_VIRTUAL_MACHINE_H */
