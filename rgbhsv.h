/**
 * GJay, copyright (c) 2002 Chuck Groom
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 1, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <gtk/gtk.h>

#ifndef _RGBHSB_H_
#define _RGBHSB_H_

typedef struct {float R, G, B;} RGB; 
typedef struct {float H, S, V;} HSV; 
typedef struct {float H, B;}    HB;

HSV     rgb_to_hsv ( RGB rgb );
RGB     hsv_to_rgb ( HSV hsv );
guint32 rgb_to_hex ( RGB rgb );
HSV     hb_to_hsv  ( HB hb );
HB      hsv_to_hb  ( HSV hsv );

#endif
