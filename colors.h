/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002 Chuck Groom
 * Copyright (C) 2010 Craig Small 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _RGBHSB_H_
#define _RGBHSB_H_

#include <glib.h>
#ifdef WITH_GUI
#include <gtk/gtk.h>
#endif /* WITH_GUI */

typedef struct {gdouble R, G, B;} RGB; 
typedef struct {gdouble H, S, V;} HSV; 
typedef struct {float H, B;}    HB;

void    hsv_to_rgb(HSV *hsv, RGB *rgb);
void    hsv_to_gdkcolor(HSV *hsv, GdkColor *gdk_color);
void    rgb_to_hsv(RGB *rgb, HSV *hsv);
void    gdkcolor_to_hsv(GdkColor *gdk_color, HSV *hsv);

guint32 rgb_to_hex ( RGB rgb );
HSV     hb_to_hsv  ( HB hb );
HB      hsv_to_hb  ( HSV hsv );
int     get_named_color (char * str, RGB * rgb );
char *  known_colors (void);


#endif
