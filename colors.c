/**
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright 2002 Chuck Groom
 * Copyright 2010-2012 Craig Small 
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include "colors.h"
#include "string.h"

#define TO_HSV(h, s, v) {hsv.H = h; hsv.S = s; hsv.V = v; return hsv;} 
#define TO_RGB(r, g, b) {rgb.R = r; rgb.G = g; rgb.B = b; return rgb;} 
#define UNDEFINED -1 

#define NUM_KNOWN_COLORS 8
char * known_color_str[NUM_KNOWN_COLORS] = {
    "black",
    "white",
    "red",
    "green",
    "blue",
    "purple",
    "yellow",
    "cyan"
};

RGB known_color_rgb[NUM_KNOWN_COLORS] = {
    { 0.0, 0.0, 0.0 },
    { 1.0, 1.0, 1.0 },
    { 1.0, 0.0, 0.0 },
    { 0.0, 1.0, 0.0 },
    { 0.0, 0.0, 1.0 },
    { 1.0, 0.0, 1.0 },
    { 1.0, 1.0, 0.0 },
    { 0.0, 1.0, 1.0 }
};

void hsv_to_rgb(HSV *hsv, RGB *rgb)
{
    gtk_hsv_to_rgb(hsv->H, hsv->S, hsv->V,
                   &(rgb->R),
                   &(rgb->G),
                   &(rgb->B));
}

void hsv_to_gdkcolor(HSV *hsv, GdkColor *gdk_color)
{
    RGB rgb;

    hsv_to_rgb(hsv, &rgb);
    gdk_color->red   = rgb.R * 65535;
    gdk_color->green = rgb.G * 65535;
    gdk_color->blue  = rgb.B * 65535;
}

void rgb_to_hsv(RGB *rgb, HSV *hsv)
{
    gtk_rgb_to_hsv(rgb->R, rgb->G, rgb->B,
                   &(hsv->H),
                   &(hsv->S),
                   &(hsv->V)
                  );
}

void gdkcolor_to_hsv(GdkColor *gdk_color, HSV *hsv)
{
    RGB rgb;

    rgb.R = (gdouble)(gdk_color->red) / 65535L;
    rgb.G = (gdouble)(gdk_color->green) / 65535L;
    rgb.B = (gdouble)(gdk_color->blue) / 65535L;

    rgb_to_hsv(&rgb, hsv);
}


float min(float a, float b, float c) {
    float ret_val;
    if(a < b) {
        ret_val = a;
    } else {
        ret_val = b;
    }
    if (ret_val < c) {
        return ret_val;
    } 
    return c;
}


float max(float a, float b, float c) {
    float ret_val;
    if(a > b) {
        ret_val = a;
    } else {
        ret_val = b;
    }
    if (ret_val > c) {
        return ret_val;
    } 
    return c;
}






/* Convert a value of 0..1 to a (s,v) pair */
HSV hb_to_hsv (HB hb) {
    HSV hsv;
    hsv.H = hb.H;
    hsv.S = sqrt(hb.B);
    hsv.V = MIN(1, 1.25 - hb.B);
    return hsv;
}


HB hsv_to_hb (HSV hsv) {
    HB hb;
    hb.H = hsv.H;
    hb.B = hsv.S * hsv.S;
    return hb;
}


int get_named_color (char * str, RGB * rgb ) {
    int i;
    for (i = 0; i <NUM_KNOWN_COLORS; i++) {
        if (strncmp(str, known_color_str[i], 
                    strlen( known_color_str[i])) == 0){
            memcpy(rgb, &known_color_rgb[i], sizeof(RGB));
            return 1;
        }
    }
    printf("Did not recognize %s as a color; ignoring.\n", str);
    return 0;
}

char * known_colors(void) {
    return "white, black, red, green, blue, purple, yellow, or cyan";
}

