/**
 * GJay, copyright (c) 2002, 2003 Chuck Groom
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

#include "gjay.h"
#include "ui.h"

static gchar * data_color = "cb_color";
static gboolean expose_event_callback (GtkWidget *widget,
                                       GdkEventExpose *event, 
                                       gpointer data);

GtkWidget * create_colorbox (HSV * hsv) {
    HSV * color;
    GtkWidget * widget;

    widget = gtk_drawing_area_new();
    color = g_malloc(sizeof(HSV));
    if (hsv) {
        *color = *hsv;
    } else {
        color->H = 0;
        color->S = 1;        
        color->V = 1;
    }
    g_object_set_data(G_OBJECT (widget), data_color, color);

    g_signal_connect (G_OBJECT (widget), "expose_event",  
                      G_CALLBACK (expose_event_callback), NULL);
    
    return widget;
}


void set_colorbox_color (GtkWidget * widget,
                         HSV hsv) {
    *((HSV *) g_object_get_data(G_OBJECT (widget), data_color)) = hsv;
    gtk_widget_queue_draw(widget);
}


static gboolean expose_event_callback (GtkWidget *widget,
                                       GdkEventExpose *event, 
                                       gpointer data) {
    HSV * hsv;
    RGB rgb;
    guint32 rgb32 = 0;
    GdkGC * black_gc, * color_gc;

    hsv = g_object_get_data(G_OBJECT (widget), data_color);
    rgb = hsv_to_rgb(*hsv);
    rgb32 = 
        ((int) (rgb.R * 255.0)) << 16 |
        ((int) (rgb.G * 255.0)) << 8 |
        ((int) (rgb.B * 255.0));
    black_gc = gdk_gc_new(widget->window);
    color_gc = gdk_gc_new(widget->window);
    gdk_rgb_gc_set_foreground(black_gc, 0);
    gdk_rgb_gc_set_foreground(color_gc, rgb32);
    
    gdk_draw_rectangle  (widget->window,
                         black_gc,
                         FALSE,
                         0, 0, 
                         widget->allocation.width - 1,
                         widget->allocation.height - 1);
    gdk_draw_rectangle  (widget->window,
                         color_gc,
                         TRUE,
                         1, 1, 
                         widget->allocation.width - 2,
                         widget->allocation.height - 2);

    gdk_gc_unref(black_gc);
    gdk_gc_unref(color_gc);
    return TRUE;
}
