/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002,2003 Chuck Groom
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

#include <math.h>
#include <string.h>
#include <strings.h>
#include "gjay.h"
#include "ui.h"

static GdkPixbuf * create_colorwheel_hs_pixbuf ( gint diameter );
static GdkPixbuf * create_colorwheel_v_pixbuf  ( gint width,
                                                 gint height );
static void        colorwheel_plot_point   ( guchar * data,
                                             HSV color,
                                             gfloat radius,
                                             gfloat angle,
                                             gint rowstride,
                                             gint x,
                                             gint y );
static gboolean drawing_expose_event_callback ( GtkWidget *widget,
                                                GdkEventExpose *event, 
                                                gpointer data);
static gboolean drawing_button_event_callback ( GtkWidget *widget,
                                                GdkEventButton *event,
                                                gpointer user_data);
static gboolean drawing_motion_event_callback ( GtkWidget *widget,
                                                GdkEventMotion *event,
                                                gpointer user_data);
static void     click_in_colorwheel (GtkWidget * widget,
                                     gdouble x, 
                                     gdouble y);
static void     draw_selected_color ( GtkWidget * widget, 
                                       HSV hsv );
static void     draw_swatch_color ( GtkWidget * widget, 
                                    HSV hsv );

#define DATA_HS_PIXBUF  "cw_hs_pb"
#define DATA_V_PIXBUF   "cw_v_pb"
#define DATA_LIST       "cw_list"
#define DATA_COLOR      "cw_color"
#define DATA_FUNC       "cw_func"
#define DATA_USER_DATA  "cw_user_data"


GtkWidget * create_colorwheel (gint diameter, 
                               GList ** list, 
                               GFunc change_func,
                               gpointer user_data) {
    gint width, height, brightness_width;
    GdkPixbuf * colorwheel = NULL, * brightness = NULL;
    GtkWidget * widget;
    HSV * color;

    widget = gtk_drawing_area_new();
    brightness_width = diameter * COLORWHEEL_V_SWATCH_WIDTH;
    width =  diameter + COLORWHEEL_SELECT*4 + COLORWHEEL_SPACING + 
        brightness_width;
    height = diameter + COLORWHEEL_SELECT*2;

    colorwheel = create_colorwheel_hs_pixbuf(diameter);
    brightness = create_colorwheel_v_pixbuf(brightness_width, 
                                            diameter * COLORWHEEL_V_HEIGHT);

    gtk_drawing_area_size(GTK_DRAWING_AREA(widget), width, height);
    gtk_widget_add_events(widget, 
                          GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);
    g_object_set_data(G_OBJECT (widget), DATA_HS_PIXBUF, colorwheel);
    g_object_set_data(G_OBJECT (widget), DATA_V_PIXBUF, brightness);
    g_object_set_data(G_OBJECT (widget), DATA_LIST, list);

    color = g_malloc(sizeof(HSV));
    color->H = 0;
    color->S = 1;
    color->V = 1;
    g_object_set_data(G_OBJECT (widget), DATA_COLOR, color);

    g_object_set_data(G_OBJECT (widget), DATA_FUNC, change_func);
    g_object_set_data(G_OBJECT (widget), DATA_USER_DATA, user_data);

    g_signal_connect (G_OBJECT (widget), "expose_event",  
                      G_CALLBACK (drawing_expose_event_callback), NULL);
    g_signal_connect (G_OBJECT (widget), "button-press-event",  
                      G_CALLBACK (drawing_button_event_callback), NULL);
    g_signal_connect (G_OBJECT (widget), "motion-notify-event",  
                      G_CALLBACK (drawing_motion_event_callback), NULL);
    return widget;
}


HSV get_colorwheel_color ( GtkWidget * widget) {
    return *((HSV *) g_object_get_data(G_OBJECT (widget), DATA_COLOR));
}


void set_colorwheel_color ( GtkWidget * widget,
                            HSV color) {
    *((HSV *) g_object_get_data(G_OBJECT (widget), DATA_COLOR)) = color;
    gtk_widget_queue_draw(widget);
}


static GdkPixbuf * create_colorwheel_v_pixbuf (gint width, gint height) {
    GdkPixbuf * pixbuf = NULL;
    gint rowstride, x, y, v, offset;
    guchar * data;

    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                             TRUE,
                             8,
                             width, height);
    data = gdk_pixbuf_get_pixels (pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    bzero (data, rowstride * height);
    for (y = 0; y < height; y++) {
        v = 255 - (y * 255) / height;
        offset = y * rowstride;
        for (x = 0; x < width; x++, offset += 4) {
            memset(data + offset, v, 3);
            data[offset + 3] = 255;

        }
    }
    return pixbuf;
}


static GdkPixbuf * create_colorwheel_hs_pixbuf (gint diameter) {
    GdkPixbuf * colorwheel = NULL;
    gint width, height, rowstride;
    float angle, percent_radius;
    HSV hsv;
    guchar * data;
    gint x, y, draw_x, draw_y, p, xcenter, ycenter;

    hsv.V = 1; 
    width = height = diameter;
    xcenter = width / 2;
    ycenter = height / 2;
    diameter -= 2;
    
    colorwheel = gdk_pixbuf_new  (GDK_COLORSPACE_RGB,
                                  TRUE,
                                  8,
                                  width, height);
    data = gdk_pixbuf_get_pixels (colorwheel);
    rowstride = gdk_pixbuf_get_rowstride(colorwheel);

    x = 0;
    y = diameter/2;
    p = 3 - 2*diameter/2;

    while (x <= y) {
        draw_y = y;
        for (draw_x = x; draw_x >= -x; draw_x--) {
            percent_radius = (2.0 * sqrt (draw_x*draw_x + draw_y*draw_y)) / 
                ((float) diameter);
            angle = atan2((double) draw_y, (double) draw_x);
            colorwheel_plot_point(data, hsv, percent_radius, angle,
                                  rowstride, 
                                  draw_x + xcenter, draw_y + ycenter); 
        }

        draw_y = -y;
        for (draw_x = x; draw_x >= -x; draw_x--) {
            percent_radius = (2.0 * sqrt (draw_x*draw_x + draw_y*draw_y)) / 
                ((float) diameter);
            angle = atan2((double) draw_y, (double) draw_x);
            colorwheel_plot_point(data, hsv, percent_radius, angle, rowstride,
                                  draw_x + xcenter, draw_y + ycenter); 
        }

        draw_y = x;
        for (draw_x = y; draw_x >= -y; draw_x--) {
            percent_radius = (2.0 * sqrt (draw_x*draw_x + draw_y*draw_y)) / 
                ((float) diameter);
            angle = atan2((double) draw_y, (double) draw_x);
            colorwheel_plot_point(data, hsv, percent_radius, angle, rowstride,
                                  draw_x + xcenter, draw_y + ycenter); 
        }

        draw_y = -x;
        for (draw_x = y; draw_x >= -y; draw_x--) {
            percent_radius = (2.0 * sqrt (draw_x*draw_x + draw_y*draw_y)) / 
                ((float) diameter);
            angle = atan2((double) draw_y, (double) draw_x);
            colorwheel_plot_point(data, hsv, percent_radius, angle, rowstride,
                                  draw_x + xcenter, draw_y + ycenter); 
        }
        if (p < 0) 
            p += 4 * x++ + 6; 
        else 
            p += 4 * (x++ - y--) + 10; 
    }
    return colorwheel;
}


static void colorwheel_plot_point ( guchar * data,
                                    HSV hsv,
                                    gfloat radius,
                                    gfloat angle,
                                    gint rowstride,
                                    gint x,
                                    gint y ) {
    int offset;
    RGB rgb;
    if (angle > 2*M_PI) {
        angle -= 2*M_PI;
    } else if (angle < 0) {
        angle += 2*M_PI;
    }
    /* Yep, folks -- hue is 0...6, not 0...2pi! */
    hsv.H = (angle / (2*M_PI)) * 6.0;
    hsv.S = radius;

    hsv.V = 1;
    
    rgb = hsv_to_rgb (hsv);
    offset = rowstride * y + 4 * x;
    data[offset] =   MAX(0, MIN(255, rgb.R * 255));
    data[offset+1] = MAX(0, MIN(255, rgb.G * 255));
    data[offset+2] = MAX(0, MIN(255, rgb.B * 255));
    data[offset+3]=255;
}



static gboolean drawing_expose_event_callback (GtkWidget *widget,
                                               GdkEventExpose *event, 
                                               gpointer data) {
    GList * ll, ** list;
    HSV * hsv, prev_hsv;
    song * s;
    gint xcenter, ycenter, width, height, num_colors;
    GdkPixbuf * colorwheel, * brightness;
    gboolean set;

    set = FALSE;
    num_colors = 0;
    list = g_object_get_data(G_OBJECT (widget), DATA_LIST);
    hsv = g_object_get_data(G_OBJECT (widget), DATA_COLOR);
    assert(hsv);
    prev_hsv.H = -1;
    prev_hsv.S = -1;
    prev_hsv.V = -1;

    colorwheel = g_object_get_data(G_OBJECT (widget), DATA_HS_PIXBUF);
    brightness = g_object_get_data(G_OBJECT (widget), DATA_V_PIXBUF);

    gdk_draw_rectangle  (widget->window,
                         widget->style->bg_gc[GTK_WIDGET_STATE (widget)],
                         TRUE,
                         event->area.x,
                         event->area.y,
                         event->area.width,
                         event->area.height);
    
    width = gdk_pixbuf_get_width(brightness);
    height = gdk_pixbuf_get_height(brightness);
    gdk_pixbuf_render_to_drawable_alpha(
        brightness, 
        widget->window,
        0, 0,
        widget->allocation.width - width - COLORWHEEL_SELECT,
        COLORWHEEL_SELECT,
        width,
        height,
        GDK_PIXBUF_ALPHA_FULL,
        127,
        GDK_RGB_DITHER_NORMAL, 
        0, 0);

    width = gdk_pixbuf_get_width(colorwheel);
    height = gdk_pixbuf_get_height(colorwheel);
    xcenter = widget->allocation.height / 2.0; // Use height on purpose
    ycenter = widget->allocation.height / 2.0;
    gdk_pixbuf_render_to_drawable_alpha(
        colorwheel, 
        widget->window,
        0, 0,
        xcenter - width/2,
        ycenter - height/2,
        width,
        height,
        GDK_PIXBUF_ALPHA_FULL,
        127,
        GDK_RGB_DITHER_NORMAL, 
        0, 0);

    if (list) {
        for (ll = g_list_first(gjay->selected_songs); ll; ll = g_list_next(ll)) {
            s = (song *) ll->data;
            if (!s->no_color) {
                set = TRUE;
                *hsv = s->color;
                if (!((hsv->H == prev_hsv.H) &&
                      (hsv->S == prev_hsv.S) &&
                      (hsv->V == prev_hsv.V))) {
                    draw_selected_color(widget, s->color);
                    num_colors++;   
                    prev_hsv = *hsv;
             }
            }
        }    
        if (set) {
            if (num_colors == 1) {
                draw_swatch_color(widget, *hsv);
            }
        } else {
            width = gdk_pixbuf_get_width(gjay->pixbufs[PM_NOT_SET]);
            height = gdk_pixbuf_get_height(gjay->pixbufs[PM_NOT_SET]);
            gdk_pixbuf_render_to_drawable_alpha(
                gjay->pixbufs[PM_NOT_SET],
                widget->window,
                0, 0,
                xcenter - width/2, ycenter - height/2,
                width, height,
                GDK_PIXBUF_ALPHA_FULL,
                127,
                GDK_RGB_DITHER_NORMAL, 
                0, 0);

            // When the user does set a color, make it full brightness
            hsv->V = 1;
        }
    } else {
        draw_selected_color(widget, *hsv);
        draw_swatch_color(widget, *hsv);
    }
    return TRUE;
}


void draw_selected_color (GtkWidget * widget, 
                          HSV hsv) {
    GdkGC * gc;
    float angle, radius;
    GdkPixbuf * colorwheel, * brightness;
    gint xcenter, ycenter, x, y, width, height;

    gc = gdk_gc_new(widget->window);
    gdk_rgb_gc_set_foreground(gc, 0);
    
    colorwheel = g_object_get_data(G_OBJECT (widget), DATA_HS_PIXBUF);
    brightness = g_object_get_data(G_OBJECT (widget), DATA_V_PIXBUF);

    width = gdk_pixbuf_get_width(brightness);
    height = gdk_pixbuf_get_height(brightness);
    x = widget->allocation.width - 
        gdk_pixbuf_get_width(brightness) -
        2*COLORWHEEL_SELECT;
    y = (1.0 - hsv.V) * (float) gdk_pixbuf_get_height(brightness);
    gdk_draw_line(
        widget->window, gc,
        x, y + COLORWHEEL_SELECT,
        widget->allocation.width, COLORWHEEL_SELECT + y);
    
    /* Hue is 0..6 */
    angle = (hsv.H / 3) * (M_PI);
    radius = (hsv.S * gdk_pixbuf_get_width(colorwheel)) / 2.0;
    x = radius * cos (angle);
    y = radius * sin (angle);

    width = gdk_pixbuf_get_width(gjay->pixbufs[PM_COLOR_SEL]);
    height = gdk_pixbuf_get_height(gjay->pixbufs[PM_COLOR_SEL]);
    xcenter = widget->allocation.height / 2.0; // Use height on purpose
    ycenter = widget->allocation.height / 2.0;    
    gdk_pixbuf_render_to_drawable_alpha(
        gjay->pixbufs[PM_COLOR_SEL],
        widget->window,
        0, 0,
        (xcenter + x) - width/2, (ycenter + y) - height/2, 
        width,
        height,
        GDK_PIXBUF_ALPHA_FULL,
        127,
        GDK_RGB_DITHER_NORMAL, 
        0, 0);
    gdk_gc_unref(gc);
}


void draw_swatch_color (GtkWidget * widget, 
                        HSV hsv) {
    GdkGC * black_gc, * color_gc;
    RGB rgb;
    guint32 rgb32 = 0;
    gint width, height, x, y;
    GdkPixbuf * brightness;

    brightness = g_object_get_data(G_OBJECT (widget), DATA_V_PIXBUF);

    rgb = hsv_to_rgb(hsv);
    rgb32 = 
        ((int) (rgb.R * 255.0)) << 16 |
        ((int) (rgb.G * 255.0)) << 8 |
        ((int) (rgb.B * 255.0));
    black_gc = gdk_gc_new(widget->window);
    color_gc = gdk_gc_new(widget->window);
    gdk_rgb_gc_set_foreground(black_gc, 0);
    gdk_rgb_gc_set_foreground(color_gc, rgb32);
    
    width = gdk_pixbuf_get_width(brightness);
    height = (widget->allocation.height -                  
              2*COLORWHEEL_SELECT) * COLORWHEEL_SWATCH_HEIGHT;
    x = widget->allocation.width - 
        width - 
        COLORWHEEL_SELECT;            
    y = widget->allocation.height - COLORWHEEL_SELECT - height;
    gdk_draw_rectangle  (widget->window,
                         black_gc,
                         FALSE,
                         x, y,
                         width - 1, height - 1 );
    gdk_draw_rectangle  (widget->window,
                         color_gc,
                         TRUE,
                         x + 1, y + 1,
                         width - 2, height - 2 );
    gdk_gc_unref(black_gc);
    gdk_gc_unref(color_gc);            
}


static gboolean drawing_button_event_callback (GtkWidget *widget,
                                               GdkEventButton *event,
                                               gpointer user_data) {
    click_in_colorwheel(widget, event->x, event->y);
    return TRUE;
}


static gboolean drawing_motion_event_callback  (GtkWidget *widget,
                                                GdkEventMotion *event,
                                                gpointer user_data) {
    if (event->state & GDK_BUTTON1_MASK) 
        click_in_colorwheel(widget, event->x, event->y);
    return TRUE;
}


static void click_in_colorwheel (GtkWidget * widget,
                                 gdouble x, 
                                 gdouble y) {
    gboolean updateHS = TRUE;
    gboolean updateV  = TRUE;
    gdouble radius, angle;
    gint xcenter, ycenter, diameter;
    GdkPixbuf * colorwheel, * brightness;
    GList ** list;
    HSV * hsv;
    GFunc change_func;
    gpointer user_data;

    colorwheel = g_object_get_data(G_OBJECT (widget), DATA_HS_PIXBUF);
    brightness = g_object_get_data(G_OBJECT (widget), DATA_V_PIXBUF);
    list = g_object_get_data(G_OBJECT (widget), DATA_LIST);
    hsv = g_object_get_data(G_OBJECT (widget), DATA_COLOR);
    assert (hsv);

    /* Check for v */
    if ((x >= widget->allocation.width - gdk_pixbuf_get_width(brightness) - COLORWHEEL_SELECT) &&
        (x <= widget->allocation.width - COLORWHEEL_SELECT)) {
        hsv->V = MIN(1.0, 
                     MAX(0, 1.0 - 
                         (((float) (y - COLORWHEEL_SELECT)) / 
                          ((float) gdk_pixbuf_get_height(brightness)))));
    } else {
        updateV = FALSE;
    }

    /* Check for h-s */
    diameter = gdk_pixbuf_get_width(colorwheel);
    xcenter = widget->allocation.height / 2.0; // Use height on purpose
    ycenter = widget->allocation.height / 2.0;

    x -= xcenter;
    y -= ycenter;

    if ((x < - diameter / 2) || 
        (x >   diameter / 2) || 
        (y < - diameter / 2) || 
        (y >   diameter / 2)) 
        updateHS = FALSE;
    
    radius = (2.0 * sqrt(x*x + y*y)) / diameter;
    if (radius > 1.0)
        updateHS = FALSE;

    if (updateHS) {
        if (x == 0) {
            if (y > 0)
                angle = M_PI/2.0;
            else 
                angle = (3.0*M_PI)/2.0;
        } else if (y == 0) {
            if (x > 0)
                angle = 0;
            else
                angle = M_PI;
        } else {
            angle = atan(y / x);
            if (x < 0)
                angle += M_PI;
            else if (y < 0)
                angle += 2*M_PI;
        }
        
        hsv->H = (3.0 * angle) / M_PI;
        hsv->S = radius;
    } 

    if (updateHS || updateV) {
        change_func = g_object_get_data(G_OBJECT (widget), DATA_FUNC);
        user_data = g_object_get_data(G_OBJECT (widget), DATA_USER_DATA);
        if (change_func) {
            change_func(widget, user_data);
        }
        gtk_widget_queue_draw(widget);
    }
}

