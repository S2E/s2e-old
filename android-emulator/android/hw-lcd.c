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
#include "android/hw-lcd.h"
#include "android/boot-properties.h"
#include <stdio.h>

void
hwLcd_setBootProperty(int density)
{
    char  temp[8];

    /* map density to one of our three values for now */
    if (density < (LCD_DENSITY_MIN + LCD_DENSITY_DEFAULT)/2)
        density = LCD_DENSITY_MIN;
    else if (density < (LCD_DENSITY_DEFAULT + LCD_DENSITY_MAX)/2)
        density = LCD_DENSITY_DEFAULT;
    else
        density = LCD_DENSITY_MAX;

    snprintf(temp, sizeof temp, "%d", density);
    boot_property_add("qemu.sf.lcd_density", temp);
}

