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
#include "rgbhsv.h"
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




HSV rgb_to_hsv( RGB rgb ) 
{ 
    float R = rgb.R, G = rgb.G, B = rgb.B, v, x, f; 
    int i; 
    HSV hsv; 
    x = min(R, G, B); 
    v = max(R, G, B); 
    if(v == x) TO_HSV(UNDEFINED, 0, v); 
    f = (R == x) ? G - B : ((G == x) ? B - R : R - G); 
    i = (R == x) ? 3 : ((G == x) ? 5 : 1); 
    TO_HSV(i - f /(v - x), (v - x)/v, v); 
    return hsv;
} 



RGB hsv_to_rgb( HSV hsv ) 
{ 
    float h = hsv.H, s = hsv.S, v = hsv.V, m, n, f; 
    int i; 
    RGB rgb = { 0.0, 0.0, 0.0}; 
    if (h == UNDEFINED) TO_RGB(v, v, v); 
    i = floor(h); 
    f = h - i; 
    if ( !(i&1) ) f = 1 - f; // if i is even 
    m = v * (1 - s); 
    n = v * (1 - s * f); 
    switch (i) { 
    case 6: 
    case 0: TO_RGB(v, n, m); 
    case 1: TO_RGB(n, v, m); 
    case 2: TO_RGB(m, v, n); 
    case 3: TO_RGB(m, n, v); 
    case 4: TO_RGB(n, m, v); 
    case 5: TO_RGB(v, m, n); 
    } 
    return rgb;
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

