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


#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <string.h>
#include "gjay.h"
#include "ui.h"

static gboolean expose_about_view (GtkWidget *widget, 
                                   GdkEventExpose *event, 
                                   gpointer data);

GtkWidget * make_about_view ( void ) {
    GtkWidget * widget;
    
    widget = gtk_drawing_area_new();
    gtk_signal_connect (GTK_OBJECT (widget),
                        "expose_event",
                        (GtkSignalFunc) expose_about_view,
                        NULL);
    return widget;
}


static gboolean expose_about_view (GtkWidget *widget, 
                                   GdkEventExpose *event, 
                                   gpointer data) {
    gchar buffer[BUFFER_SIZE];
    gint width, height, w, h;
    GdkGC * gc; 
    GdkFont * font;
    
    width = widget->allocation.width;
    height = widget->allocation.height;
    
    gc = gdk_gc_new(window->window);
    gdk_rgb_gc_set_foreground(gc, 0xFFFFFF);
    gdk_gc_set_clip_rectangle (gc,
                               &event->area);
    gdk_draw_rectangle (widget->window,
                        gc,
                        TRUE,
                        0, 0, 
                        width, height);

    w = gdk_pixbuf_get_width (pixbufs[PM_ABOUT]);
    h = gdk_pixbuf_get_height (pixbufs[PM_ABOUT]);
    
    gdk_pixbuf_render_to_drawable ( pixbufs[PM_ABOUT],
                                    widget->window,
                                    gc,
                                    0, 0,
                                    (width - w) / 2, 10,
                                    w, h,
                                    GDK_RGB_DITHER_NORMAL,
                                    0, 0);
    
     gdk_rgb_gc_set_foreground(gc, 0x000000);

     snprintf(buffer, BUFFER_SIZE, "Version %s", GJAY_VERSION);

     font = gtk_style_get_font (gtk_widget_get_style(widget));
     w = gdk_text_width(font,
                        buffer,
                        strlen(buffer));
     gdk_draw_string (widget->window,
                      font,
                      gc, 
                      (width - w) / 2, height * 0.5,
                      buffer);

     snprintf(buffer, BUFFER_SIZE, "Copyright (c) 2002, Chuck Groom");
     w = gdk_text_width(font,
                        buffer,
                        strlen(buffer));
     gdk_draw_string (widget->window,
                      font,
                      gc, 
                      (width - w) / 2, height * 0.65,
                      buffer);
     gdk_rgb_gc_set_foreground(gc, 0xCC00CC);
     snprintf(buffer, BUFFER_SIZE, "http://gjay.sourceforge.net");
     w = gdk_text_width(font,
                        buffer,
                        strlen(buffer));
     gdk_draw_string (widget->window,
                      font,
                      gc, 
                      (width - w) / 2, height * 0.8,
                      buffer);


     gdk_gc_unref(gc);

    return TRUE;

    
}
