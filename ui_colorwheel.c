#include <math.h>
#include "gjay.h"
#include "ui.h"

static GdkPixbuf * create_colorwheel_pixbuf( gint diameter );
static void        colorwheel_plot_point   ( guchar * data,
                                             gfloat saturation,
                                             gfloat hue,
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


static gchar * data_pixbuf = "cw_pb";
static gchar * data_list =   "cw_list";
static gchar * data_color =  "cw_color";


GtkWidget * create_colorwheel (gint diameter, 
                               GList ** list, 
                               HB * color) {
    GdkPixbuf * colorwheel = NULL;
    GtkWidget * widget;

    colorwheel = create_colorwheel_pixbuf(diameter);
    
    widget = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(widget),
                          diameter + COLORWHEEL_SELECT*2,
                          diameter + COLORWHEEL_SELECT*2);
    gtk_widget_add_events(widget, 
                          GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);
    g_object_set_data(G_OBJECT (widget), data_pixbuf, colorwheel);
    g_object_set_data(G_OBJECT (widget), data_list, list);
    g_object_set_data(G_OBJECT (widget), data_color, color);
    
    g_signal_connect (G_OBJECT (widget), "expose_event",  
                      G_CALLBACK (drawing_expose_event_callback), NULL);
    g_signal_connect (G_OBJECT (widget), "button-press-event",  
                      G_CALLBACK (drawing_button_event_callback), NULL);
    g_signal_connect (G_OBJECT (widget), "motion-notify-event",  
                      G_CALLBACK (drawing_motion_event_callback), NULL);

    return widget;
}


static GdkPixbuf * create_colorwheel_pixbuf (gint diameter) {
    GdkPixbuf * colorwheel = NULL;
    gint width, height, rowstride;
    float angle, percent_radius;
    guchar * data;
    gint x, y, draw_x, draw_y, p, xcenter, ycenter;

    width = height = diameter;
    xcenter = width / 2;
    ycenter = height / 2;
    
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
            colorwheel_plot_point(data, percent_radius, angle,
                                  rowstride, 
                                  draw_x + xcenter, draw_y + ycenter); 
        }

        draw_y = -y;
        for (draw_x = x; draw_x >= -x; draw_x--) {
            percent_radius = (2.0 * sqrt (draw_x*draw_x + draw_y*draw_y)) / 
                ((float) diameter);
            angle = atan2((double) draw_y, (double) draw_x);
            colorwheel_plot_point(data, percent_radius, angle, rowstride,
                                  draw_x + xcenter, draw_y + ycenter); 
        }

        draw_y = x;
        for (draw_x = y; draw_x >= -y; draw_x--) {
            percent_radius = (2.0 * sqrt (draw_x*draw_x + draw_y*draw_y)) / 
                ((float) diameter);
            angle = atan2((double) draw_y, (double) draw_x);
            colorwheel_plot_point(data, percent_radius, angle, rowstride,
                                  draw_x + xcenter, draw_y + ycenter); 
        }

        draw_y = -x;
        for (draw_x = y; draw_x >= -y; draw_x--) {
            percent_radius = (2.0 * sqrt (draw_x*draw_x + draw_y*draw_y)) / 
                ((float) diameter);
            angle = atan2((double) draw_y, (double) draw_x);
            colorwheel_plot_point(data, percent_radius, angle, rowstride,
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
                                    gfloat saturation,
                                    gfloat hue,
                                    gint rowstride,
                                    gint x,
                                    gint y ) {
    int offset;
    
    RGB rgb;
    HB hb;
    if (hue > 2*M_PI) {
        hue -= 2*M_PI;
    } else if (hue < 0) {
        hue += 2*M_PI;
    }
    hb.H = (3.0 * hue) / M_PI;
    hb.B = MIN(1, saturation);
    rgb = hsv_to_rgb (hb_to_hsv(hb));
    if (y == 150)
        y--;
    offset = rowstride * y + 4 * x;
    data[offset] =   MIN(255, rgb.R * 255);
    data[offset+1] = MIN(255, rgb.G * 255);
    data[offset+2] = MIN(255, rgb.B * 255);
    if (saturation > 0.99) 
        data[offset+3] = 180;
    else
        data[offset+3] = 255;
    
}



static gboolean drawing_expose_event_callback (GtkWidget *widget,
                                               GdkEventExpose *event, 
                                               gpointer data) {
    GList * ll, ** list;
    HB * hb;
    song * s;
    float angle, radius, old_angle = -1, old_radius = -1;
    gint xcenter, ycenter, x, y, width, height;
    GdkPixbuf * colorwheel;
    gboolean set = FALSE;
    
    colorwheel = g_object_get_data(G_OBJECT (widget), data_pixbuf);
    list = g_object_get_data(G_OBJECT (widget), data_list);
    hb = g_object_get_data(G_OBJECT (widget), data_color);
    
    xcenter = widget->allocation.width  / 2.0;
    ycenter = widget->allocation.height / 2.0;
    width = gdk_pixbuf_get_width(colorwheel);
    height = gdk_pixbuf_get_height(colorwheel);

    gdk_draw_rectangle  (widget->window,
                         widget->style->bg_gc[GTK_WIDGET_STATE (widget)],
                         TRUE,
                         event->area.x,
                         event->area.y,
                         event->area.width,
                         event->area.height);
    
    gdk_pixbuf_render_to_drawable_alpha(colorwheel, 
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
        for (ll = g_list_first(selected_songs); ll; ll = g_list_next(ll)) {
            s = (song *) ll->data;
            if (!s->no_color) {
                set = TRUE;
                angle = s->color.H;
                radius = (s->color.B * gdk_pixbuf_get_width(colorwheel)) / 2.0;
                if ((old_angle != angle) || (old_radius != radius)) {
                    x = radius * cos (angle);
                    y = radius * sin (angle);
                    
                    width = gdk_pixbuf_get_width(pixbufs[PM_COLOR_SEL]);
                    height = gdk_pixbuf_get_height(pixbufs[PM_COLOR_SEL]);
                    
                    gdk_pixbuf_render_to_drawable_alpha(
                        pixbufs[PM_COLOR_SEL],
                        widget->window,
                        0, 0,
                        (xcenter + x) - width/2, (ycenter + y) - height/2, 
                        width,
                        height,
                        GDK_PIXBUF_ALPHA_FULL,
                        127,
                        GDK_RGB_DITHER_NORMAL, 
                        0, 0);
                    old_angle = angle;
                    old_radius = radius;
                }
            }
        }    
        if (!set) {
            width = gdk_pixbuf_get_width(pixbufs[PM_NOT_SET]);
            height = gdk_pixbuf_get_height(pixbufs[PM_NOT_SET]);
            gdk_pixbuf_render_to_drawable_alpha(
                pixbufs[PM_NOT_SET],
                widget->window,
                0, 0,
                xcenter - width/2, ycenter - height/2,
                width, height,
                GDK_PIXBUF_ALPHA_FULL,
                127,
                GDK_RGB_DITHER_NORMAL, 
                0, 0);
        }
    } else if (hb) {
        angle = hb->H;
        radius = (hb->B * gdk_pixbuf_get_width(colorwheel)) / 2.0;
        x = radius * cos (angle);
        y = radius * sin (angle);
        
        width = gdk_pixbuf_get_width(pixbufs[PM_COLOR_SEL]);
        height = gdk_pixbuf_get_height(pixbufs[PM_COLOR_SEL]);
        
        gdk_pixbuf_render_to_drawable_alpha(
            pixbufs[PM_COLOR_SEL],
            widget->window,
            0, 0,
            (xcenter + x) - width/2, (ycenter + y) - height/2, 
            width,
            height,
            GDK_PIXBUF_ALPHA_FULL,
            127,
            GDK_RGB_DITHER_NORMAL, 
            0, 0);
    }
    return TRUE;
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
    gdouble radius, angle;
    gint xcenter, ycenter, diameter;
    GdkPixbuf * colorwheel;
    GList ** list;
    HB * hb;

    colorwheel = g_object_get_data(G_OBJECT (widget), data_pixbuf);
    list = g_object_get_data(G_OBJECT (widget), data_list);
    hb = g_object_get_data(G_OBJECT (widget), data_color);

    diameter = gdk_pixbuf_get_width(colorwheel);

    xcenter = widget->allocation.width  / 2.0;
    ycenter = widget->allocation.height / 2.0;

    x -= xcenter;
    y -= ycenter;

    if ((x < - diameter / 2) || 
        (x >   diameter / 2) || 
        (y < - diameter / 2) || 
        (y >   diameter / 2)) 
        return;
    
    radius = sqrt(x*x + y*y);
    if (radius > diameter/2.0)
        return;
    radius /= diameter/2.0;

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
        
    if (list)
        update_selected_songs_color(angle, radius);
    if (hb) {
        hb->H = angle;
        hb->B = radius;
    }
    gtk_widget_queue_draw(widget);
}

