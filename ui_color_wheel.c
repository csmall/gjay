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


#include <math.h>
#include "gjay.h"

#define CW_HB        "cw_hb"
#define CW_GC        "cw_gc"
#define CW_BG_PIXMAP "cw_bg_pixmap"
#define CW_FG_PIXMAP "cw_fg_pixmap"

static GdkPixmap * make_color_wheel_pixmap (GtkWidget * window, 
                                            GdkGC * gc);
static void plot_point_radius_angle    ( GdkPixmap * pm,
                                         GdkGC * gc,
                                         gfloat percent_radius,
                                         gfloat angle,
                                         gint x,
                                         gint y );
static void click_change_color ( GtkWidget * widget, 
                                 int xclick, 
                                 int yclick );
static gint color_wheel_configure ( GtkWidget *widget,
                                    GdkEventConfigure *event,
                                    gpointer user_data);
static gint color_wheel_expose ( GtkWidget *widget,
                                 GdkEventExpose *event,
                                 gpointer user_data);
static gint color_wheel_button_press ( GtkWidget *widget,
                                       GdkEventButton *event,
                                       gpointer user_data );
static gint color_wheel_motion( GtkWidget *widget,
                                GdkEventMotion *event,
                                gpointer user_data );
static gboolean color_wheel_unrealize (GtkWidget *widget,
                                       gpointer user_data);


GtkWidget * make_color_wheel ( HB * hb ) {
    GtkWidget * color_wheel;

    color_wheel = gtk_drawing_area_new ();
    gtk_object_set_data(GTK_OBJECT(color_wheel), 
                        CW_HB,
                        hb);
    gtk_drawing_area_size(GTK_DRAWING_AREA(color_wheel),
                          CATEGORIZE_DIAMETER + 2*SELECT_RADIUS,
                          CATEGORIZE_DIAMETER + 2*SELECT_RADIUS);
    gtk_signal_connect (GTK_OBJECT(color_wheel),
                        "configure_event",
			(GtkSignalFunc) color_wheel_configure, 
                        NULL);
    gtk_signal_connect (GTK_OBJECT (color_wheel), 
                        "expose_event",
			(GtkSignalFunc) color_wheel_expose, 
                        NULL);
    gtk_signal_connect (GTK_OBJECT (color_wheel),
                        "motion_notify_event",
			(GtkSignalFunc) color_wheel_motion,
                        NULL);
    gtk_signal_connect (GTK_OBJECT (color_wheel), 
                        "button_press_event",
			(GtkSignalFunc) color_wheel_button_press,
                        NULL);
    gtk_signal_connect (GTK_OBJECT (color_wheel), 
                        "unrealize",
			(GtkSignalFunc) color_wheel_unrealize,
                        NULL);
    gtk_widget_set_events (color_wheel, 
                           GDK_EXPOSURE_MASK |
			   GDK_LEAVE_NOTIFY_MASK |
			   GDK_BUTTON_PRESS_MASK |
			   GDK_POINTER_MOTION_MASK |
			   GDK_POINTER_MOTION_HINT_MASK);
    return color_wheel;
}


static void draw_color_wheel (GtkWidget * color_wheel) {
    RGB rgb;
    gint x, y, diameter, xcenter, ycenter, width, height;
    float angle, radius;
    GdkPixmap * bg, * fg;
    GdkGC * gc;
    HB * cw_hb;
    HSV hsv;
    
    bg = (GdkPixmap *) gtk_object_get_data(GTK_OBJECT(color_wheel), 
                                           CW_BG_PIXMAP);
    fg = (GdkPixmap *) gtk_object_get_data(GTK_OBJECT(color_wheel), 
                                           CW_FG_PIXMAP);
    gc = (GdkGC *) gtk_object_get_data(GTK_OBJECT(color_wheel), 
                                       CW_GC);
    cw_hb = (HB *) gtk_object_get_data(GTK_OBJECT(color_wheel), 
                                        CW_HB);

    diameter = CATEGORIZE_DIAMETER;
    width = CATEGORIZE_DIAMETER + 2*SELECT_RADIUS;
    height = CATEGORIZE_DIAMETER + 2*SELECT_RADIUS;
    xcenter = width / 2;
    ycenter = height / 2;
    
    /* Copy the color wheel (base_pm) pixel map into the components_pm
     * pixel map */
    gdk_draw_pixmap(fg, gc, bg, 
                    0, 0, 0, 0, width, height); 

    /* Draw a 3px radius xor circle around the current color. */
    angle = (cw_hb->H * M_PI) / 3.0;
    radius = (cw_hb->B * diameter) / 2.0;
    x = xcenter + radius * cos (angle);
    y = ycenter + radius * sin (angle);
    hsv = hb_to_hsv(*cw_hb);
    rgb = hsv_to_rgb (hsv);
    gdk_rgb_gc_set_foreground(gc, rgb_to_hex (rgb));
    gdk_draw_arc(fg, gc, FALSE, 
                 x - SELECT_RADIUS, y - SELECT_RADIUS, 
                 SELECT_RADIUS*2, SELECT_RADIUS*2, 
                 0, 360*64);
    hsv.V = 1 - cw_hb->B;
    rgb = hsv_to_rgb (hsv);
    gdk_rgb_gc_set_foreground(gc, rgb_to_hex (rgb));
    gdk_draw_arc(fg, gc, FALSE, 
                 x - SELECT_RADIUS + 1, y - SELECT_RADIUS + 1, 
                 (SELECT_RADIUS - 1)*2, (SELECT_RADIUS - 1)*2, 
                 0, 360*64);

    /* Schedule self for update */
    gtk_widget_queue_draw(color_wheel);
}


static GdkPixmap * make_color_wheel_pixmap (GtkWidget * window, 
                                            GdkGC * gc) {
    GdkPixmap * pm;
    float angle, percent_radius;
    gint x, y, draw_x, draw_y, p, diameter, xcenter, ycenter;
    diameter = CATEGORIZE_DIAMETER;
    xcenter = (CATEGORIZE_DIAMETER + 2*SELECT_RADIUS) / 2;
    ycenter = (CATEGORIZE_DIAMETER + 2*SELECT_RADIUS) / 2;

    pm = gdk_pixmap_new(window->window, 
                        CATEGORIZE_DIAMETER + 2*SELECT_RADIUS,
                        CATEGORIZE_DIAMETER + 2*SELECT_RADIUS,
                        -1);
    if (!pm)
        return NULL;
    
    /* Fill with bkgnd color */
    gdk_draw_rectangle (pm,
                        window->style->bg_gc[GTK_WIDGET_STATE (window)],
                        TRUE,
                        0, 0,
                        CATEGORIZE_DIAMETER + 2*SELECT_RADIUS,
                        CATEGORIZE_DIAMETER + 2*SELECT_RADIUS);

    x = 0;
    y = diameter/2;
    p = 3 - 2*diameter/2;

    while (x <= y) {
        draw_y = y;
        for (draw_x = x; draw_x >= -x; draw_x--) {
            percent_radius = (2.0 * sqrt (draw_x*draw_x + draw_y*draw_y)) / 
                ((float) diameter);
            angle = atan2((double) draw_y, (double) draw_x);
            plot_point_radius_angle(pm, gc, 
                                    percent_radius, angle, 
                                    draw_x + xcenter, draw_y + ycenter); 
        }

        draw_y = -y;
        for (draw_x = x; draw_x >= -x; draw_x--) {
            percent_radius = (2.0 * sqrt (draw_x*draw_x + draw_y*draw_y)) / 
                ((float) diameter);
            angle = atan2((double) draw_y, (double) draw_x);
            plot_point_radius_angle(pm, gc, 
                                    percent_radius, angle, 
                                    draw_x + xcenter, draw_y + ycenter); 
        }

        draw_y = x;
        for (draw_x = y; draw_x >= -y; draw_x--) {
            percent_radius = (2.0 * sqrt (draw_x*draw_x + draw_y*draw_y)) / 
                ((float) diameter);
            angle = atan2((double) draw_y, (double) draw_x);
            plot_point_radius_angle(pm, gc,
                                    percent_radius, angle, 
                                    draw_x + xcenter, draw_y + ycenter); 
        }

        draw_y = -x;
        for (draw_x = y; draw_x >= -y; draw_x--) {
            percent_radius = (2.0 * sqrt (draw_x*draw_x + draw_y*draw_y)) / 
                ((float) diameter);
            angle = atan2((double) draw_y, (double) draw_x);
            plot_point_radius_angle(pm, gc,
                                    percent_radius, angle, 
                                    draw_x + xcenter, draw_y + ycenter); 
        }
        
        if (p < 0) 
            p += 4 * x++ + 6; 
        else 
            p += 4 * (x++ - y--) + 10; 
    }
    return pm;
}
 

/*
 * Draw a point of the categorize pixmap pm at (x,y), using the
 * percent of the radius as the saturation, the angle as the hue, and
 * the brightness of current.
 */
static void plot_point_radius_angle ( GdkPixmap * pm,
                                      GdkGC * gc,
                                      gfloat percent_radius,
                                      gfloat angle,
                                      gint x,
                                      gint y ) {
    RGB rgb;
    HB hb;
    if (angle > 2*M_PI) {
        angle -= 2*M_PI;
    } else if (angle < 0) {
        angle += 2*M_PI;
    }
    hb.H = (3.0 * angle) / M_PI;
    hb.B = percent_radius;
    rgb = hsv_to_rgb (hb_to_hsv(hb));
    gdk_rgb_gc_set_foreground(gc, rgb_to_hex (rgb));
    gdk_draw_point(pm, gc, x, y);
}


static gint color_wheel_configure ( GtkWidget *widget,
                                    GdkEventConfigure *event,
                                    gpointer user_data) {
    GdkPixmap * pm;
    GtkWidget * window = gtk_widget_get_toplevel(widget);
    GdkGC * gc;

    /* Check to see if we've already allocated data */
    pm = (GdkPixmap *) gtk_object_get_data(GTK_OBJECT(widget), 
                                           CW_FG_PIXMAP);
    if (pm)
        return TRUE;

    /* Initialize the GDK RGB tools, which make color allocation easier. */
    gdk_rgb_init();

    gc = gdk_gc_new(widget->window);
    gtk_object_set_data(GTK_OBJECT(widget), 
                        CW_GC, 
                        gc);
    pm = make_color_wheel_pixmap(window, gc);
    gtk_object_set_data(GTK_OBJECT(widget), 
                        CW_BG_PIXMAP,
                        pm);
    pm = gdk_pixmap_new(window->window, 
                        CATEGORIZE_DIAMETER + 2*SELECT_RADIUS,
                        CATEGORIZE_DIAMETER + 2*SELECT_RADIUS,
                        -1);
    gtk_object_set_data(GTK_OBJECT(widget), 
                        CW_FG_PIXMAP,
                        pm);
    draw_color_wheel(widget);
    return TRUE;
}


static gint color_wheel_expose ( GtkWidget      *widget,
                                 GdkEventExpose *event,
                                 gpointer user_data ) {
    GdkPixmap * pm;
    GdkGC * gc;
    
    pm = (GdkPixmap *) gtk_object_get_data(GTK_OBJECT(widget), 
                                           CW_FG_PIXMAP);
    gc = (GdkGC *) gtk_object_get_data(GTK_OBJECT(widget), 
                                       CW_GC);
    gdk_draw_pixmap(widget->window,
                    gc, 
                    pm,
                    event->area.x, event->area.y,
                    event->area.x, event->area.y,
                    event->area.width, event->area.height);
    return FALSE;
}


/*
 * To determine the color which corresponds to the click location, we
 * must calculate the distance between the click and the center of the
 * color wheel to determine the saturation, then we must calculate the
 * angle between the 3 o'clock angle and the click location to
 * determine the hue.
 */
static void click_change_color( GtkWidget *widget,
                                int xclick, 
                                int yclick ) 
{
    float x, y; 
    float angle, radius;
    HB * hb;

    hb = (HB *) gtk_object_get_data(GTK_OBJECT(widget), 
                                    CW_HB);
    
    /* Determine click coordinate relative to the center of the circle. */
    x = widget->allocation.width/2 - xclick;
    y = widget->allocation.height/2 - yclick;
    
    /* Calculate radius and angle of click in color wheel */
    radius = sqrt (x*x + y*y);
    if (radius == 0) {
        angle = M_PI;
    } else {
        if(x==0) {
            if(y>0) {
                angle = (3.0 * M_PI) / 2.0;
            } else {
                angle = M_PI / 2.0;
            }
        } else {
            angle = M_PI + atan2(y, x);
        }
    }
    
    if (radius <= CATEGORIZE_DIAMETER/2.0 + 2) {
        if (radius > CATEGORIZE_DIAMETER/2.0) {
            radius = CATEGORIZE_DIAMETER/2.0;
        }
        hb->H = (3.0 * angle) / M_PI;
        hb->B = (2.0 * radius) / ((float) CATEGORIZE_DIAMETER);
        draw_color_wheel(widget);
    }
}


/*
 * This function is called when the user clicks on the
 * color_wheel object. It invokes the click_change_color
 * function if the user clicked the first button.
 */
static gint color_wheel_button_press ( GtkWidget *widget,
                                       GdkEventButton *event,
                                       gpointer user_data ) {
    click_change_color (widget, event->x, event->y);
    return TRUE;
}


/*
 * This function is called when the mouse moves in the
 * categorize_drawing object. If the first button is down, then we
 * call click_change_color to see if the user is dragging within the
 * color wheel to select a color.
 */
static gint color_wheel_motion( GtkWidget *widget,
                                GdkEventMotion *event,
                                gpointer user_data ) {
    int x, y;
    GdkModifierType state;
        
    if (event->is_hint) {
        gdk_window_get_pointer (event->window, &x, &y, &state);
    } else {
        x = event->x;
        y = event->y;
        state = event->state;
    }
    if (state & GDK_BUTTON1_MASK) 
        click_change_color (widget, event->x, event->y);
    return TRUE;
}

static gboolean color_wheel_unrealize (GtkWidget *widget,
                                       gpointer user_data) {
    GdkPixmap * bg, * fg;
    GdkGC * gc;
        
    bg = (GdkPixmap *) gtk_object_get_data(GTK_OBJECT(widget), 
                                           CW_BG_PIXMAP);
    fg = (GdkPixmap *) gtk_object_get_data(GTK_OBJECT(widget), 
                                           CW_FG_PIXMAP);
    gc = (GdkGC *) gtk_object_get_data(GTK_OBJECT(widget), 
                                       CW_GC);
    gdk_pixmap_unref(bg);
    gdk_pixmap_unref(fg);
    gdk_gc_unref(gc);

    return TRUE;
}


guint32 rgb_to_hex( RGB rgb) {
    gint r, g, b;
    r = 255.0 * rgb.R;
    g = 255.0 * rgb.G;
    b = 255.0 * rgb.B;
    return  ((r << 16) | (g << 8) | b);
}
