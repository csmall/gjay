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
 *
 * Analysis.c -- manages the background threads and processes involved
 * in song analysis.
 */

#include <math.h>
#include <string.h>
#include "gjay.h"
#include "analysis.h"
#include "images/small_stars.h"

static GdkPixmap * create_song_rating_pixmap ( song * s);
static GdkPixmap * create_song_freq_pixmap   ( song * s);

static char * attrs[SONG_LAST] = {
    "Title",
    "Artist",
    "Freq",
    "BPM",
    "Color",
    "Rating"
};



/* Build a generic CList for holding songs */
GtkWidget * make_song_clist (void) {
    GtkWidget * clist;
    gint k, col, width;

    clist = gtk_clist_new(SONG_LAST);
    for (k = 0, col = 0; k < SONG_LAST; k++) {
        gtk_clist_set_column_title(GTK_CLIST(clist),
                                   col, 
                                   attrs[k]);
         switch(k) {
         case SONG_TITLE:
             width = 120;
             break;
         case SONG_ARTIST:
             width = 100;
             break;
         case SONG_RATING:
             width = 20;
             break;
         case SONG_BPM:
             width = 40;
             break;
         case SONG_COLOR:
             width = COLOR_SWATCH_W + 5;
             break;
         case SONG_FREQ:
             width = FREQ_PM_W + 5;
             break;
         default:
             width = 0;
         }
         gtk_clist_set_column_min_width (GTK_CLIST(clist), 
                                         col, width);
         col++;
     }
     gtk_clist_column_titles_show (GTK_CLIST(clist));
     return clist;
}


void build_song_list_row ( GtkCList * clist, 
                           guint row, 
                           song * s) {
    int col, num_cols, k;
    GdkPixmap * pm;
    gchar * str;
    gchar buffer[BUFFER_SIZE];


    num_cols = SONG_LAST;
    for (k = 0, col = 0; k < num_cols; k++) {
        pm = NULL;
        str = NULL;
        switch (k) {
        case SONG_TITLE:
            str = (strcmp(s->title, "?")) ? s->title : s->fname;
            break;
        case SONG_ARTIST:
            str = s->artist;
            break;
        case SONG_COLOR:
            if (s->flags & COLOR_UNK) {
                snprintf(buffer, BUFFER_SIZE, "?"); 
                str = buffer;
            } else {
                pm = create_color_pixmap(song_get_rgb_hex(s));
            }
            break;
        case SONG_BPM:
            if (s->flags & BPM_UNK) {
                snprintf(buffer, BUFFER_SIZE, "?"); 
            } else if (s->flags & BPM_UNDEF) {
                snprintf(buffer, BUFFER_SIZE, "Unsure"); 
            } else {
                snprintf(buffer, BUFFER_SIZE, "%1.2f", s->bpm);
            } 
            str = buffer;
            break;
        case SONG_RATING:
            pm = create_song_rating_pixmap(s);
            break;
        case SONG_FREQ:
            if (s->flags & FREQ_UNK) {
                snprintf(buffer, BUFFER_SIZE, "?"); 
                str = buffer;
            } else {
                pm = create_song_freq_pixmap(s);
            } 
            break;
        default:
            /* Playing, in playlist ... */
        };
        if (str) 
            gtk_clist_set_text (clist, row, col, str);
        else if (pm) 
            gtk_clist_set_pixmap (clist, row, col, 
                                  pm, NULL);
        col++;
    }
}


GdkPixmap * create_color_pixmap ( guint32 rgb_hex ) {
    GdkPixmap * pm;
    GdkGC * gc; 
    pm = gdk_pixmap_new(app_window->window,
                        COLOR_SWATCH_W,
                        COLOR_SWATCH_H,
                        -1);
    gc = gdk_gc_new(app_window->window);
    gdk_rgb_gc_set_foreground(gc, rgb_hex);
    gdk_draw_rectangle (pm,
                        gc,
                        TRUE,
                        0, 0, 
                        COLOR_SWATCH_W,
                        COLOR_SWATCH_H);
    gdk_rgb_gc_set_foreground(gc, 0x00);
    gdk_draw_rectangle (pm,
                        gc,
                        FALSE,
                        0, 0, 
                        COLOR_SWATCH_W - 1,
                        COLOR_SWATCH_H - 1);
    gdk_gc_unref(gc);
    return pm;
}



static GdkPixmap * create_song_rating_pixmap ( song * s) {
    GdkPixmap * pm;
    GdkGC * gc; 
    guint w, h;

    pm = gdk_pixmap_create_from_xpm_d (app_window->window,
                                       NULL,
                                       NULL,
                                       (gchar **) &small_stars_xpm);
    gdk_window_get_size(pm, &w, &h);
    gc = gdk_gc_new(app_window->window);
    gdk_rgb_gc_set_foreground(gc, 0xFFFFFFFF);
    gdk_draw_rectangle (pm,
                        gc,
                        TRUE,
                        (s->rating / MAX_RATING) * w, 0, 
                        w * (1 - (s->rating / MAX_RATING)), h);
    gdk_gc_unref(gc);
    return pm;
}


static GdkPixmap * create_song_freq_pixmap ( song * s) {
    GdkPixmap * pm;
    GdkGC * gc; 
    guint k;
    HSV hsv;
    gint depth, t;

    gdk_window_get_geometry (app_window->window,
                             &t, &t, &t, &t, &depth);
    pm = gdk_pixmap_new (app_window->window,
                         FREQ_PM_W,
                         FREQ_PM_H,
                         depth);
    gc = gdk_gc_new(app_window->window);
    gdk_rgb_gc_set_foreground(gc, 0x000000);
    gdk_draw_rectangle (pm,
                        gc,
                        TRUE,
                        0, 0, 
                        FREQ_PM_W,
                        FREQ_PM_H);
    hsv.S = 1.0;

    for (k = 0; k < NUM_FREQ_SAMPLES; k++) {
        hsv.V = MIN(1.0,  15 * s->freq[k]);
        hsv.V = MIN(1.0, MAX(0, hsv.V));
        hsv.H = (M_PI * k) / (double) NUM_FREQ_SAMPLES;
        gdk_rgb_gc_set_foreground(gc, rgb_to_hex(hsv_to_rgb (hsv)));
        gdk_draw_line(pm, gc, k + 1, 1, k + 1, FREQ_PM_H - 2);
    }
    gdk_gc_unref(gc);    
    return pm;
}
