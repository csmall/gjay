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

#include "gjay.h"
#include "images/star.h"

#define SR_PM     "sr_pm"
#define SR_RATING "sr_rating"


static gint stars_expose ( GtkWidget *widget,
                           GdkEventExpose *event,
                           gpointer user_data);
static gint stars_unrealize ( GtkWidget *widget,
                              gpointer user_data);
static gint stars_rating_changed ( GtkAdjustment * adjustment,
                                   gpointer user_data );


GtkWidget * make_star_rating ( GtkWidget * window,
                               gdouble * rating ) {
    GtkWidget * vbox, * drawing, * scale;
    GtkObject * adj;
    GdkPixmap *pm;
    gint width, height;

    vbox = gtk_vbox_new (FALSE, 2);
    pm = gdk_pixmap_create_from_xpm_d (window->window,
                                       NULL,
                                       NULL,
                                       (gchar **) &star_xpm);
    drawing = gtk_drawing_area_new ();
    gdk_window_get_size(pm, &width, &height);
    gtk_widget_set_usize(drawing, -1, height);
    gtk_object_set_data(GTK_OBJECT(drawing),
                        SR_PM, 
                        pm);
    gtk_object_set_data(GTK_OBJECT(drawing), 
                        SR_RATING, 
                        rating);
    gtk_signal_connect (GTK_OBJECT(drawing),
                        "expose_event",
                        (GtkSignalFunc) stars_expose, 
                        NULL);
    gtk_signal_connect (GTK_OBJECT(drawing),
                        "unrealize",
                        (GtkSignalFunc) stars_unrealize,
                        NULL);
    adj = gtk_adjustment_new(*rating, MIN_RATING, MAX_RATING, 
                             .1, .5, 0);
    gtk_signal_connect (GTK_OBJECT (adj),
                        "value-changed",
                        (GtkSignalFunc) stars_rating_changed,
                        drawing);
    scale = gtk_hscale_new(GTK_ADJUSTMENT(adj));
    gtk_box_pack_start(GTK_BOX(vbox), drawing, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), scale, TRUE, TRUE, 0);
    return vbox;
}



static gint stars_expose (GtkWidget *widget,
                          GdkEventExpose *event,
                          gpointer user_data) {
    GdkGC * gc; 
    gdouble * rating;
    GdkPixmap * pm;
    GdkColor white;
    GdkColor black;
    gint start, w, draw_width, s_width, s_height;
    
    pm = (GdkPixmap *) gtk_object_get_data(GTK_OBJECT(widget), 
                                           SR_PM);
    rating = (gdouble *) gtk_object_get_data(GTK_OBJECT(widget), 
                                             SR_RATING);
    gc = gdk_gc_new(widget->window);
    gdk_color_white (gdk_window_get_colormap(widget->window),
                     &white);
    gdk_color_black (gdk_window_get_colormap(widget->window),
                     &black);
    gdk_gc_set_foreground (gc, &white);
    gdk_window_get_size(pm, &s_width, &s_height);
    
    draw_width = widget->allocation.width * (*rating) / MAX_RATING;
    /* Fill right section with white */
    gdk_draw_rectangle (widget->window,
                        gc,
                        TRUE,
                        draw_width, 
                        0,                        
                        widget->allocation.width - draw_width,
                        s_height);
    
    for(start = 0; start < draw_width; ) {
        w = MIN(s_width, draw_width - start);
        gdk_draw_pixmap ( widget->window,
                          gc,
                          pm,
                          0, 0, 
                          start, 0,
                          w, 
                          s_height);
        start += w;
        if (start < draw_width) {
            w = MIN((widget->allocation.width - MAX_RATING*s_width) /
                    (MAX_RATING - 1), draw_width - start);
            gdk_draw_rectangle (widget->window,
                                gc,
                                TRUE,
                                start, 0,
                                w, s_height);    
            start += w;
        }
    }
    gdk_gc_set_foreground (gc, &black);
    gdk_draw_rectangle (widget->window,
                        gc,
                        FALSE,
                        0, 0,
                        widget->allocation.width - 1,
                        widget->allocation.height - 1);
    gdk_gc_unref(gc);
    return TRUE;
}


static gint stars_rating_changed (GtkAdjustment * adjustment,
                                  gpointer user_data) {
    GtkWidget * drawing = (GtkWidget *) user_data;
    gdouble * rating;
    rating = gtk_object_get_data(GTK_OBJECT(drawing), 
                                 SR_RATING);
    *rating = adjustment->value;
    gtk_widget_queue_draw(drawing);
    return TRUE;
}


static gboolean stars_unrealize (GtkWidget *widget,
                                 gpointer user_data) {
    GdkPixmap * pm = (GdkPixmap *) gtk_object_get_data(GTK_OBJECT(widget), 
                                                       SR_PM);
    gdk_pixmap_unref(pm);
    return FALSE;
} 



