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

#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>
#include "gjay.h"
#include "analysis.h"
#include "images/play.h"
#include "images/logo.h"
#include "rgbhsv.h"

#define APP_WIDTH 450
#define APP_HEIGHT 420
#define PLAYLIST_WIDTH APP_WIDTH - 10
#define PLAYLIST_HEIGHT APP_HEIGHT - 120

#define PLAYLIST_SONG_TITLE_LEN 20

/* Strs for setting gobject data */
#define CC_HB    "cc_hb"
#define CS_LIST  "cs_list"
#define CS_LABEL "cs_label"

static char * tabs[APP_LAST] = {
    "Songs",
    "Make Playlist",
    "About"
};

typedef struct {
    GtkWidget * widget;
    gpointer data;
} widget_and_data;


GtkWidget * app_window;
GtkWidget * song_list;
GtkWidget * analysis_label;
GtkWidget * analysis_progress;
app_tabs    current_tab;
static GtkWidget * song_list_swin;
GList     * dialog_list = NULL;


static gint        analyze_timer_callback ( gpointer data );
static gint        add_files_dialog ( GtkWidget *widget,
                                      GdkEventButton *event,
                                      gpointer user_data);
static gint        set_attrs ( GtkWidget *widget,
                               GdkEventButton *event,
                               gpointer user_data);
static void        update_attrs (gpointer data,
                                 gpointer user_data);
static gint        add_files ( GtkWidget *widget,
                               GdkEventButton *event,
                               gpointer user_data);
static gboolean    song_list_click (GtkWidget *widget,
                                    GdkEventButton *event,
                                    gpointer user_data);
static void        song_list_column_click (GtkCList *clist,
                                           gint column,
                                           gpointer user_data);
static gboolean    song_list_key_press  (GtkWidget *widget,
                                         GdkEventKey *event,
                                         gpointer user_data);
static void playlist_start_toggle ( GtkToggleButton *togglebutton,
                                    gpointer user_data );
static void playlist_toggle ( GtkToggleButton *togglebutton,
                              gpointer user_data );
static gint playlist_adj_int_changed   ( GtkAdjustment * adjustment,
                                     gpointer user_data);
static gint playlist_adj_double_changed ( GtkAdjustment * adjustment,
                                          gpointer user_data);
static gint playlist_make ( GtkWidget *widget,
                            GdkEventButton *event,
                            gpointer user_data);
static void make_playlist_window ( GList * list);
static void playlist_free_list     (GtkWidget * widget,
                                    GdkEvent *event,
                                    gpointer user_data);
static void playlist_window_play   (GtkWidget * widget,
                                    gpointer user_data);
static void playlist_window_save   (GtkWidget * widget,
                                    gpointer user_data);
static void playlist_fsel_save     (GtkWidget * widget,
                                    gpointer user_data);
static void fsel_destroy           (GtkWidget * widget,
                                    gpointer user_data);
static void playlist_fsel_save_overwrite (GtkWidget * widget,
                                          gpointer user_data);
static void app_songs_play         (GtkWidget * widget,
                                    gpointer user_data);
static void choose_color           (GtkWidget * widget,
                                    gpointer user_data);
static void choose_color_ok        (GtkWidget * widget,
                                    gpointer user_data);
static void choose_color_cancel    (GtkWidget * widget,
                                    gpointer user_data);
static void choose_color_unrealize (GtkWidget * widget,
                                    gpointer user_data);
static void choose_song            (GtkWidget * widget,
                                    gpointer user_data);
static void choose_song_ok         (GtkWidget * widget,
                                    gpointer user_data);
static void choose_song_cancel     (GtkWidget * widget,
                                    gpointer user_data);
static void choose_song_unrealize  (GtkWidget * widget,
                                    gpointer user_data);
static void make_delete_songs_dialog (GList * list);
static void delete_songs_ok        (GtkWidget * widget,
                                    gpointer user_data);
static void delete_songs_cancel    (GtkWidget * widget,
                                    gpointer user_data);
static void delete_songs_unrealize (GtkWidget * widget,
                                    gpointer user_data);
gboolean    about_expose           (GtkWidget *widget, 
                                    GdkEventExpose *event, 
                                    gpointer data);
static void close_all_dialogs      ( void );


GtkWidget * make_app_ui ( void ) {
    GtkWidget *notebook, *hbox, *app_vbox, *vbox, *button, *table;
    GtkWidget *widget, * frame, *subhbox, *subvbox, * box;
    GdkPixmap * pixmap;
    GdkBitmap * mask;
    GSList * group;
    GtkObject *adj;
    GList * current;
    song * s;
    gchar buffer[BUFFER_SIZE];

    gdk_rgb_init();

    app_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (app_window), "GJay");
    gtk_widget_set_usize(app_window, APP_WIDTH, APP_HEIGHT);
    gtk_widget_realize(app_window);
    gtk_signal_connect (GTK_OBJECT (app_window), "delete_event",
			GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (app_window), 2);
    gtk_widget_realize(app_window);

    app_vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add (GTK_CONTAINER (app_window), app_vbox);
    notebook = gtk_notebook_new(); 
    gtk_box_pack_start(GTK_BOX(app_vbox), notebook, TRUE, TRUE, 5);
    hbox = gtk_hbox_new(TRUE, 2);
    gtk_box_pack_end(GTK_BOX(app_vbox), hbox, FALSE, FALSE, 5);
    analysis_label = gtk_label_new("Idle");
    gtk_box_pack_start(GTK_BOX(hbox), analysis_label, TRUE, TRUE, 5);
    analysis_progress = gtk_progress_bar_new();
    gtk_box_pack_end(GTK_BOX(hbox), analysis_progress, TRUE, TRUE, 5);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
                             vbox,
                             gtk_label_new(tabs[APP_SONGS]));

    song_list_swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (song_list_swin),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), song_list_swin, TRUE, TRUE, 2);
    
    /* Build song clist, automatically pack it into song_list_swin */
    build_song_list();

    hbox = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
   
    button = gtk_button_new();
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        (GtkSignalFunc) app_songs_play,
                        NULL);
    subhbox = gtk_hbox_new(TRUE, 5);
    gtk_container_add (GTK_CONTAINER (button), subhbox);

    box = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(subhbox), box, FALSE, FALSE, 4);
    
    widget = gtk_label_new("Play");
    gtk_box_pack_start(GTK_BOX(box), widget, TRUE, FALSE, 4);
    pixmap = gdk_pixmap_create_from_xpm_d (app_window->window,
                                       &mask,
                                       NULL,
                                       (gchar **) &play_xpm);
    widget = gtk_pixmap_new (pixmap, mask);
    gtk_box_pack_start(GTK_BOX(box), widget, TRUE, FALSE, 4);

    
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);

    button = gtk_button_new_with_label ("Add file(s)...");
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
    gtk_signal_connect (GTK_OBJECT (button), 
                        "button_press_event",
			(GtkSignalFunc) add_files_dialog,
                        NULL);
    button = gtk_button_new_with_label ("Set Color & Rating");
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
    gtk_signal_connect (GTK_OBJECT (button), 
                        "button_press_event",
			(GtkSignalFunc) set_attrs,
                        NULL);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
                             vbox,
                             gtk_label_new(tabs[APP_MAKE_PLAYLIST]));
    gtk_container_set_border_width(GTK_CONTAINER (vbox), 3);
    subhbox = gtk_hbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), subhbox, FALSE, TRUE, 0);

    frame = gtk_frame_new("Start playlist at");
    gtk_box_pack_start(GTK_BOX(subhbox), frame, FALSE, TRUE, 0);
    
    table  = gtk_table_new ( 3, 2, FALSE);
    gtk_container_add (GTK_CONTAINER (frame), table);
    gtk_container_set_border_width(GTK_CONTAINER (table), 3);

    button = gtk_radio_button_new_with_label( NULL, "Random");
    gtk_signal_connect (GTK_OBJECT (button),
                        "toggled",
                        (GtkSignalFunc) playlist_start_toggle,
                        &prefs.start_random);
    if (prefs.start_random) 
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    gtk_table_attach  (GTK_TABLE(table),
                       button,
                       0, 1,
                       0, 1,
                       GTK_FILL, GTK_EXPAND,
                       1, 1);
    group = gtk_radio_button_group( GTK_RADIO_BUTTON(button));
    button = gtk_radio_button_new_with_label( group, "Song");
    gtk_signal_connect (GTK_OBJECT (button),
                        "toggled",
                        (GtkSignalFunc) playlist_start_toggle,
                        &prefs.start_song);
    if (prefs.start_song)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

    gtk_table_attach (GTK_TABLE(table),
                      button,
                      0, 1,
                      1, 2,
                      GTK_FILL, GTK_EXPAND,
                      1, 1);
    group = gtk_radio_button_group( GTK_RADIO_BUTTON(button));
    button = gtk_radio_button_new_with_label( group, "Color");
    gtk_signal_connect (GTK_OBJECT (button),
                        "toggled",
                        (GtkSignalFunc) playlist_start_toggle,
                        &prefs.start_color);
    if (prefs.start_color)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

    gtk_table_attach (GTK_TABLE(table),
                      button,
                      0, 1,
                      2, 3,
                      GTK_FILL, GTK_EXPAND,
                      1, 1);

    button = gtk_button_new();
    snprintf(buffer, PLAYLIST_SONG_TITLE_LEN, "(None)");
    for (current = songs; current; current = g_list_next(current)) {
        s = SONG(current);
        if (s->checksum == prefs.song_cksum) {
            snprintf(buffer, PLAYLIST_SONG_TITLE_LEN, "%s",
                     (strcmp(s->title, "?") ? s->title : s->fname));
            break;
        }
    }
    widget = gtk_label_new(buffer);
    gtk_container_add (GTK_CONTAINER (button), widget);
    gtk_signal_connect (GTK_OBJECT (button),
                        "clicked", 
                        GTK_SIGNAL_FUNC (choose_song),
                        widget);    
    gtk_table_attach (GTK_TABLE(table),
                      button,
                      1, 2,
                      1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_EXPAND,
                      1, 1);


    button = gtk_button_new();
    widget = gtk_pixmap_new(create_color_pixmap(rgb_to_hex(hsv_to_rgb(hb_to_hsv(prefs.color)))), NULL);
    gtk_container_add (GTK_CONTAINER (button), widget);
    gtk_signal_connect (GTK_OBJECT (button),
                        "clicked", 
                        GTK_SIGNAL_FUNC (choose_color),
                        widget);    
    gtk_table_attach (GTK_TABLE(table),
                      button,
                      1, 2,
                      2, 3,
                      GTK_FILL | GTK_EXPAND, GTK_EXPAND,
                      1, 1);

    frame = gtk_frame_new("Selection");
    gtk_box_pack_start(GTK_BOX(subhbox), frame, FALSE, TRUE, 0);

    subvbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add (GTK_CONTAINER (frame), subvbox);
    gtk_container_set_border_width(GTK_CONTAINER (subvbox), 3);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(subvbox), hbox, FALSE, FALSE, 2);
    widget = gtk_label_new("Variance");
    gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
    adj = gtk_adjustment_new(prefs.variance, 
                             MIN_VARIANCE,
                             MAX_VARIANCE,
                             1, 5, 0);
    gtk_signal_connect (GTK_OBJECT (adj),
                        "value-changed",
                        (GtkSignalFunc) playlist_adj_int_changed,
                        &prefs.variance);
    widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
    gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(subvbox), hbox, FALSE, TRUE, 0);
    button = gtk_check_button_new_with_label("Rating cut-off");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), 
                                 prefs.rating_cutoff);
    gtk_signal_connect (GTK_OBJECT (button),
                        "toggled",
                        (GtkSignalFunc) playlist_toggle,
                        &prefs.rating_cutoff);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), 
                                 prefs.rating_cutoff);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);

    widget = make_star_rating(app_window, &prefs.rating);
    gtk_box_pack_start(GTK_BOX(subvbox), widget, FALSE, TRUE, 0);
    
    frame = gtk_frame_new("Importance");
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

    table = gtk_table_new(4, 3, FALSE);
    gtk_container_add (GTK_CONTAINER (frame), table);
    
    widget = gtk_label_new("Hue");
    gtk_table_attach  (GTK_TABLE(table),
                       widget,
                       0, 1,
                       0, 1,
                       GTK_FILL, GTK_EXPAND,
                       2, 2);

    adj = gtk_adjustment_new(prefs.criteria_hue, MIN_CRITERIA, MAX_CRITERIA,
                             1, 1, 0);
    gtk_signal_connect (GTK_OBJECT (adj),
                        "value-changed",
                        (GtkSignalFunc) playlist_adj_double_changed,
                        &prefs.criteria_hue);
    widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
    gtk_table_attach  (GTK_TABLE(table),
                       widget,
                       1, 2,
                       0, 1,
                       GTK_EXPAND | GTK_FILL, GTK_EXPAND,
                       2, 2);

    button = gtk_check_button_new_with_label("Maintain");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), 
                                 prefs.hold_hue);
    gtk_signal_connect (GTK_OBJECT (button),
                        "toggled",
                        (GtkSignalFunc) playlist_toggle,
                        &prefs.hold_hue);
    gtk_table_attach  (GTK_TABLE(table),
                       button,
                       2, 3,
                       0, 1,
                       GTK_FILL, GTK_EXPAND,
                       2, 2);
    
    widget = gtk_label_new("Brightness");
    gtk_table_attach  (GTK_TABLE(table),
                       widget,
                       0, 1,
                       1, 2,
                       GTK_FILL, GTK_EXPAND,
                       2, 2);

    adj = gtk_adjustment_new(prefs.criteria_brightness, 
                             MIN_CRITERIA, MAX_CRITERIA,
                             1, 1, 0);
    gtk_signal_connect (GTK_OBJECT (adj),
                        "value-changed",
                        (GtkSignalFunc) playlist_adj_double_changed,
                        &prefs.criteria_brightness);
    widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
    gtk_table_attach  (GTK_TABLE(table),
                       widget,
                       1, 2,
                       1, 2,
                       GTK_EXPAND | GTK_FILL, GTK_EXPAND,
                       2, 2);

    button = gtk_check_button_new_with_label("Maintain");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), 
                                 prefs.hold_brightness);
    gtk_signal_connect (GTK_OBJECT (button),
                        "toggled",
                        (GtkSignalFunc) playlist_toggle,
                        &prefs.hold_brightness);
    gtk_table_attach  (GTK_TABLE(table),
                       button,
                       2, 3,
                       1, 2,
                       GTK_FILL, GTK_EXPAND,
                       2, 2);

    widget = gtk_label_new("BPM");
    gtk_table_attach  (GTK_TABLE(table),
                       widget,
                       0, 1,
                       2, 3,
                       GTK_FILL, GTK_EXPAND,
                       2, 2);

    adj = gtk_adjustment_new(prefs.criteria_bpm, MIN_CRITERIA, MAX_CRITERIA,
                             1, 1, 0);
    gtk_signal_connect (GTK_OBJECT (adj),
                        "value-changed",
                        (GtkSignalFunc) playlist_adj_double_changed,
                        &prefs.criteria_bpm);
    widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
    gtk_table_attach  (GTK_TABLE(table),
                       widget,
                       1, 2,
                       2, 3,
                       GTK_EXPAND | GTK_FILL, GTK_EXPAND,
                       2, 2);

    button = gtk_check_button_new_with_label("Maintain");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), 
                                 prefs.hold_bpm);
    gtk_signal_connect (GTK_OBJECT (button),
                        "toggled",
                        (GtkSignalFunc) playlist_toggle,
                        &prefs.hold_bpm);
    gtk_table_attach  (GTK_TABLE(table),
                       button,
                       2, 3,
                       2, 3,
                       GTK_FILL, GTK_EXPAND,
                       2, 2);

    widget = gtk_label_new("Frequency");
    gtk_table_attach  (GTK_TABLE(table),
                       widget,
                       0, 1,
                       3, 4,
                       GTK_FILL, GTK_EXPAND,
                       2, 2);

    adj = gtk_adjustment_new(prefs.criteria_freq, MIN_CRITERIA, MAX_CRITERIA,
                             1, 1, 0);
    gtk_signal_connect (GTK_OBJECT (adj),
                        "value-changed",
                        (GtkSignalFunc) playlist_adj_double_changed,
                        &prefs.criteria_freq);
    widget = gtk_hscale_new(GTK_ADJUSTMENT(adj));
    gtk_table_attach  (GTK_TABLE(table),
                       widget,
                       1, 2,
                       3, 4,
                       GTK_EXPAND | GTK_FILL, GTK_EXPAND,
                       2, 2);
    button = gtk_check_button_new_with_label("Maintain");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), 
                                 prefs.hold_freq);
    gtk_signal_connect (GTK_OBJECT (button),
                        "toggled",
                        (GtkSignalFunc) playlist_toggle,
                        &prefs.hold_freq);
    gtk_table_attach  (GTK_TABLE(table),
                       button, 
                       2, 3,
                       3, 4,
                       GTK_FILL, GTK_EXPAND,
                       2, 2);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, FALSE, 2);

    subhbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), subhbox, TRUE, FALSE, 3);

    button = gtk_button_new_with_label ("Make Playlist");
    gtk_box_pack_start(GTK_BOX(subhbox), button, FALSE, FALSE, 5);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);  
    gtk_widget_grab_default (button);  
    gtk_signal_connect (GTK_OBJECT (button),
                        "clicked", 
                        GTK_SIGNAL_FUNC (playlist_make),
                        NULL);


    widget = gtk_label_new("Time (min)");
    gtk_box_pack_start(GTK_BOX(subhbox), widget, FALSE, TRUE, 5);
    adj = gtk_adjustment_new(prefs.time, 
                             MIN_PLAYLIST_TIME,
                             MAX_PLAYLIST_TIME,
                             1, 10, 0);
    gtk_signal_connect (GTK_OBJECT (adj),
                        "value-changed",
                        (GtkSignalFunc) playlist_adj_int_changed,
                        &prefs.time);
    button = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
    gtk_box_pack_start(GTK_BOX(subhbox), button, FALSE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
                             vbox,
                             gtk_label_new(tabs[APP_ABOUT]));
    pixmap = gdk_pixmap_create_from_xpm_d (app_window->window,
                                           &mask,
                                           NULL,
                                           (gchar **) &logo_xpm);
    widget = gtk_drawing_area_new();
    gtk_signal_connect (GTK_OBJECT (widget),
                        "expose_event",
                        (GtkSignalFunc) about_expose,
                        pixmap);

    gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook),
                          1);

    gtk_timeout_add(ANALYZE_TIME, analyze_timer_callback, NULL);
    return app_window;
}


GtkWidget * build_song_list(void) { 
     GdkPixmap * pm;
     GdkBitmap * bm;
     gchar ** strs;
     guint row, col, num_cols;
     GList * current;
     song * s;

     num_cols = SONG_LAST;
     
     if (song_list) {
        /* Destroy contents of old song list */
         for (row = 0; row < GTK_CLIST(song_list)->rows; row++) {
            for (col = 0; col < GTK_CLIST(song_list)->columns; col++) {
                if (gtk_clist_get_pixmap(GTK_CLIST(song_list), row, col,
                                         &pm, &bm)) {
                    if (pm)
                        gdk_pixmap_unref(pm);
                    if (bm)
                        gdk_bitmap_unref(bm);
                }
            }
         }
         gtk_clist_clear( GTK_CLIST(song_list));
     } else {
         song_list = make_song_clist();
         gtk_container_add(GTK_CONTAINER(song_list_swin), song_list);
         gtk_clist_set_selection_mode(GTK_CLIST(song_list),
                                      GTK_SELECTION_BROWSE | GTK_SELECTION_MULTIPLE);
         gtk_widget_add_events (song_list, GDK_KEY_PRESS_MASK);
         gtk_signal_connect (GTK_OBJECT(song_list),
                             "button-press-event",
                             (GtkSignalFunc) song_list_click,
                             NULL);
         gtk_signal_connect (GTK_OBJECT(song_list),
                             "click-column",
                             (GtkSignalFunc) song_list_column_click,
                             NULL);
         gtk_signal_connect (GTK_OBJECT(song_list),
                             "key_press_event",
                             (GtkSignalFunc) song_list_key_press,
                             NULL);
         
     }
     strs = (gchar **) g_malloc(sizeof(gchar *) * num_cols);
     memset ((void *) strs, 0x00, sizeof(gchar *) * num_cols);
     for (current = songs, row = 0; 
          current && (row < num_songs); 
          row++, current = g_list_next(current)) {
         s = SONG(current);
         gtk_clist_append (GTK_CLIST(song_list), strs);
         build_song_list_row(GTK_CLIST(song_list), row, s);
     }
     g_free(strs);
     return song_list;
}


void rebuild_song_in_list ( song * s) {
    int row = 0;
    GList * current;
    
    for (row = 0, current = songs; current; 
         current = g_list_next(current), row++) {
        if (SONG(current) == s) {
            build_song_list_row(GTK_CLIST(song_list), row, s);
            /* Check to see if sort order has changed */
            if (!correct_sort(current)) {
                sort(&songs);
                build_song_list();
            }
            return;
        }
    }
}


static gboolean song_list_click (GtkWidget *widget,
                                 GdkEventButton *event,
                                 gpointer user_data) {
    gint row, column;
    GList * list;
    song * s;

    if ((event->type == GDK_2BUTTON_PRESS) &&
        gtk_clist_get_selection_info  (GTK_CLIST(widget),
                                       event->x,
                                       event->y,
                                       &row,
                                       &column)) {
        s = SONG(g_list_nth(songs, row));
        list = g_list_append(NULL, s);
        gtk_widget_show_all(make_categorize_dialog(list, update_attrs));
    }
    return TRUE;
}


static void song_list_column_click (GtkCList *clist,
                                    gint column,
                                    gpointer user_data) {
    sort_by sort_type;
    switch ((song_attr) column) {
    case SONG_TITLE:
    case SONG_ARTIST:
        sort_type = ARTIST_TITLE;
        break;
    case SONG_BPM:
        sort_type = BPM;
        break;
    case SONG_COLOR:
        sort_type = COLOR;
        break;
    case SONG_RATING:
        sort_type = RATING;
        break;
    default:
        sort_type = FREQ;
    }
    if (prefs.sort == sort_type)
        return;
    prefs.sort = sort_type;
    sort(&songs);
    build_song_list();
}


static gboolean song_list_key_press (GtkWidget *widget,
                                     GdkEventKey *event,
                                     gpointer user_data) {
    GList * list, * row_list;
    gint i;

    if ((event->keyval == GDK_Delete) ||
        (event->keyval == GDK_KP_Delete) ||
        (event->keyval == GDK_BackSpace)) {
        for (row_list = GTK_CLIST(widget)->row_list, i = 0, list = NULL;
             row_list;
             row_list = g_list_next(row_list), i++) {
            if (((GtkCListRow *) row_list->data)->state == GTK_STATE_SELECTED) {
                list = g_list_append(list, SONG(g_list_nth(songs, i)));
            }
        }
        if (list) 
            make_delete_songs_dialog(list);
    }
    return TRUE;
}


static gint analyze_timer_callback ( gpointer data ) {
    song * s;
    GList * current;
    gchar buffer[BUFFER_SIZE];
    char * str;
    char * analyze_str = "Analyze: ";
    char * analyze_finish_str = "Finishing: ";

    pthread_mutex_lock(&analyze_data_mutex);
    gtk_label_get(GTK_LABEL(analysis_label), &str);
    switch (analyze_state) {
    case ANALYZE:
        gtk_progress_bar_update (GTK_PROGRESS_BAR(analysis_progress),
                                 analyze_percent/100.0); 
        if (!strstr(str, analyze_str)) {
            snprintf(buffer, 120, "%s%s",
                     analyze_str,
                     (strcmp(analyze_song->title, "?") ? analyze_song->title :
                      analyze_song->fname));
            gtk_label_set_text(GTK_LABEL(analysis_label), buffer);
        }
        break;
    case ANALYZE_FINISH:
        gtk_progress_bar_update (GTK_PROGRESS_BAR(analysis_progress),
                                 analyze_percent/100.0); 
        if (!strstr(str, analyze_finish_str)) {
            snprintf(buffer, 120, "%s%s",
                     analyze_finish_str,
                     (strcmp(analyze_song->title, "?") ? analyze_song->title :
                      analyze_song->fname));
            gtk_label_set_text(GTK_LABEL(analysis_label), buffer);
        }
        break;
    case ANALYZE_DONE:
        /* Analysis just finished */
        gtk_progress_bar_update (GTK_PROGRESS_BAR(analysis_progress), 
                                 0);
        gtk_label_set_text(GTK_LABEL(analysis_label), "Idle");
        if (analyze_song)
            rebuild_song_in_list(analyze_song);
        analyze_song = NULL;
        analyze_state = ANALYZE_IDLE;
        /* fall into next */
    case ANALYZE_IDLE:
        /* Scan for songs requiring analysis */
        /* FIXME: stop timer if there aren't more songs to analyze */
        for (current = songs; current; current = g_list_next(current)) {
            s = SONG(current);
            if (((s->flags & BPM_UNK) ||  (s->flags & FREQ_UNK)) && 
                !(s->flags & SONG_ERROR)) {
                analyze(s);
                break;
            }
        }
    }
    pthread_mutex_unlock(&analyze_data_mutex);
    return TRUE;
}


static gint add_files_dialog ( GtkWidget *widget,
                               GdkEventButton *event,
                               gpointer user_data) { 
    GtkWidget * file_selector, * label;

    file_selector = gtk_file_selection_new("Please select an mp3, ogg, or wav:");
    gtk_signal_connect (GTK_OBJECT (file_selector), "unrealize",
                        (GtkSignalFunc) dialog_unrealize, 
                        NULL);
    if (prefs.add_dir) {
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector),
                                        prefs.add_dir);
    }
    gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));
    gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->ok_button),
                        "clicked", GTK_SIGNAL_FUNC (add_files), 
                        (gpointer) GTK_OBJECT(file_selector));
    
    gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->cancel_button),
                               "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
                               (gpointer) file_selector);
    gtk_clist_set_selection_mode(GTK_CLIST(GTK_FILE_SELECTION(file_selector)->file_list), GTK_SELECTION_BROWSE | GTK_SELECTION_MULTIPLE);

    label = gtk_label_new("Shift/Ctrl + click for multiple files");
    gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(file_selector)->main_vbox), 
                       label, FALSE, FALSE, 1);
    gtk_widget_show(label);

    dialog_list = g_list_append(dialog_list, file_selector);
    gtk_widget_show (file_selector);
    return 0;
}

static gint set_attrs ( GtkWidget *widget,
                        GdkEventButton *event,
                        gpointer user_data) {
    GList * list = NULL, * row_list;
    GtkWidget * dialog;
    GtkCListRow * row;
    guint i;

    for (row_list = GTK_CLIST(song_list)->row_list, i = 0; 
         row_list;
         row_list = g_list_next(row_list), i++) {
        row = row_list->data;
        if (row->state == GTK_STATE_SELECTED) {
            list = g_list_append(list, SONG(g_list_nth(songs, i)));
        }
    }
    
    if (list) {
        dialog = make_categorize_dialog(list, update_attrs);
        gtk_widget_show_all(dialog);
    }
    /* The categorize dialog manages freeing the list */
    return 0;
}

static void update_attrs (gpointer data,
                          gpointer user_data) {
    GList * list;
    song * s;
    for (list = (GList *) data; list; list = g_list_next(list)) {
        s = SONG(list);
        rebuild_song_in_list (s);
    }
}


static gint add_files ( GtkWidget *widget,
                        GdkEventButton *event,
                        gpointer user_data) {
    GtkWidget * w;
    GtkCList * clist;
    song * s;
    gint row, i;
    gchar * fname;
    gchar base[BUFFER_SIZE], buffer[BUFFER_SIZE];
    GList * row_list;
    gchar * text[SONG_LAST];

    memset(text, 0x00, sizeof(gchar *) * SONG_LAST);

    w = gtk_widget_get_toplevel (widget);
    clist = GTK_CLIST(GTK_FILE_SELECTION(w)->file_list);
    fname = gtk_file_selection_get_filename (GTK_FILE_SELECTION(w));
    
        
    /* Get base path for files */
    strncpy(base, fname, BUFFER_SIZE);
    for (i = strlen(base) - 2; i; i--) {
        if (base[i] == '/') {
            base[i+1] = '\0';
            break;
        }
    }
    
    /* Copy path to prefs */
    g_free(prefs.add_dir);
    prefs.add_dir = g_strdup(base);
    
    for (row_list = clist->row_list, i = 0; 
         row_list;
         row_list = g_list_next(row_list), i++) {
        if (((GtkCListRow *)row_list->data)->state == GTK_STATE_SELECTED) {
            gtk_clist_get_text(clist, i, 0, &fname);
            snprintf(buffer, BUFFER_SIZE, "%s%s", base, fname);

            s = new_song_file(buffer);
            if (s) {
                num_songs++;                
                row = song_add(&songs, s);
                gtk_clist_insert(GTK_CLIST(song_list), row, text);
                build_song_list_row(GTK_CLIST(song_list), row, s);
            }            
        }
    }
    gtk_widget_destroy(w);
    return 0;
}


static void playlist_start_toggle ( GtkToggleButton *togglebutton,
                                    gpointer user_data ) {
    *((gboolean *) user_data) = gtk_toggle_button_get_active(togglebutton);
}

static void playlist_toggle ( GtkToggleButton *togglebutton,
                              gpointer user_data ) {
    *((gboolean *) user_data) = !*((gboolean *) user_data);
}


static gint playlist_adj_int_changed (GtkAdjustment * adjustment,
                                      gpointer user_data) {
    gtk_adjustment_set_value (adjustment, (gint) adjustment->value);
    *((gint *) user_data) = adjustment->value; 
   return TRUE;
}

static gint playlist_adj_double_changed (GtkAdjustment * adjustment,
                                  gpointer user_data) {
     *((gdouble *) user_data) = adjustment->value; 
   return TRUE;
}


static gint playlist_make ( GtkWidget *widget,
                            GdkEventButton *event,
                            gpointer user_data) {
    GList * playlist = generate_playlist(prefs.time);
    if (playlist)
        make_playlist_window(playlist);
    return TRUE;
}


void display_message ( gchar * msg ) {
    GtkWidget * win, * label, * button, * vbox;

    win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (win), "GJay: Message");
    gtk_container_set_border_width (GTK_CONTAINER (win), 5);
    
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    label = gtk_label_new(msg);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);

    button = gtk_button_new_with_label("OK");
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 5);

    gtk_signal_connect (GTK_OBJECT (win), "delete_event",  
                        (GtkSignalFunc) gtk_widget_destroy, 
                        (gpointer) win);
    gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                               (GtkSignalFunc) gtk_widget_destroy, 
                               (gpointer) win);
    gtk_widget_show_all(win);
}


static void make_playlist_window ( GList * list) {
   GtkWidget * win, *widget,  * button, * vbox, * clist, * s_win, * hbox;
   GList * current;
   gint row;
   song * s;
   gchar * list_data[SONG_LAST], buffer[BUFFER_SIZE];
   GdkPixmap *pixmap;
   GdkBitmap * mask;
   gint time;
   
   win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_signal_connect (GTK_OBJECT (win), "delete_event",
                       (GtkSignalFunc) playlist_free_list,
                       list);
   gtk_signal_connect (GTK_OBJECT (win), "delete_event",  
                       (GtkSignalFunc) gtk_widget_destroy, 
                       (gpointer) win);
   gtk_signal_connect (GTK_OBJECT (win), "unrealize",
                       (GtkSignalFunc) dialog_unrealize, 
                       NULL);

   gtk_widget_realize(win);
   gtk_window_set_title (GTK_WINDOW (win), "GJay Playlist");
   gtk_container_set_border_width (GTK_CONTAINER (win), 5);
   
   vbox = gtk_vbox_new(FALSE, 2);
   gtk_container_add(GTK_CONTAINER(win), vbox);
   
   s_win = gtk_scrolled_window_new (NULL, NULL);
  
   gtk_widget_set_usize(s_win, PLAYLIST_WIDTH, PLAYLIST_HEIGHT);
   gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (s_win),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);

   gtk_box_pack_start(GTK_BOX(vbox), s_win, FALSE, FALSE, 5);
   clist = make_song_clist();
   gtk_container_add(GTK_CONTAINER(s_win), clist);
    
   memset(list_data, 0x00, SONG_LAST * sizeof(gchar *));

   for (current = list, row = 0, time = 0;
        current; 
        current = g_list_next(current), row++) {
       s = SONG(current);
       time += s->length;
       gtk_clist_append( GTK_CLIST(clist), list_data);
       build_song_list_row(GTK_CLIST(clist), row, s);
   }
 
   hbox = gtk_hbox_new(FALSE, 2);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);

   button = gtk_button_new_with_label("Save Playlist");
   gtk_signal_connect (GTK_OBJECT (button), "clicked",
                       (GtkSignalFunc) playlist_window_save,
                       list);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 2);
   
   widget = gtk_vseparator_new();
   gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 2);
   
   button = gtk_button_new();
   gtk_signal_connect (GTK_OBJECT (button), "clicked",
                       (GtkSignalFunc) playlist_window_play,
                       list);
   pixmap = gdk_pixmap_create_from_xpm_d (win->window,
                                          &mask,
                                          NULL,
                                          (gchar **) &play_xpm);
   widget = gtk_pixmap_new (pixmap, mask);
   gtk_container_add (GTK_CONTAINER (button), widget);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);

   widget = gtk_vseparator_new();
   gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 2);

   snprintf(buffer, BUFFER_SIZE, "%dh:%02dm:%02ds", 
            time / 3600, (time / 60) % 60, time % 60);
   widget = gtk_label_new(buffer);
   gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 5);

   dialog_list = g_list_append(dialog_list, win);
   gtk_widget_show_all(win);
}

static void playlist_free_list (GtkWidget * widget, 
                                GdkEvent *event,
                                gpointer user_data) {
    g_list_free((GList *) user_data);
    return;
}

static void playlist_window_play (GtkWidget * widget,
                                  gpointer user_data) {
    play_songs((GList *) user_data);
}

static void playlist_window_save (GtkWidget * widget,
                                  gpointer user_data) {
    GtkWidget * file_selector;
    GList * list = g_list_copy((GList *) user_data);

    file_selector = gtk_file_selection_new("Save playlist");
    gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));

    gtk_signal_connect (GTK_OBJECT (file_selector), "delete_event",
                        (GtkSignalFunc) playlist_free_list,
                        (gpointer) list);
    gtk_signal_connect (GTK_OBJECT (file_selector), "delete_event",  
                       (GtkSignalFunc) gtk_widget_destroy, 
                       (gpointer) file_selector);
    gtk_signal_connect (GTK_OBJECT (file_selector), "unrealize",
                       (GtkSignalFunc) dialog_unrealize, 
                       NULL);
    
    gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->ok_button),
                        "clicked", 
                        GTK_SIGNAL_FUNC (playlist_fsel_save), 
                        (gpointer) list);
    gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->cancel_button),
                               "clicked", 
                               GTK_SIGNAL_FUNC (gtk_widget_destroy),
                               (gpointer) file_selector);
    dialog_list = g_list_append(dialog_list, file_selector);
    gtk_widget_show_all(file_selector);
}


static void playlist_fsel_save (GtkWidget * widget,
                                gpointer user_data) {
    gchar * fname;
    GtkWidget * win, * label, * button, * vbox, *hbox, *top;
    char buffer[BUFFER_SIZE];

    top = gtk_widget_get_toplevel(widget);
    fname = gtk_file_selection_get_filename (GTK_FILE_SELECTION(top));

    if (!access(fname, R_OK)) {
        /* File already exists */
        win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title (GTK_WINDOW (win), "GJay - File Exists");
        gtk_container_set_border_width (GTK_CONTAINER (win), 5);
        gtk_signal_connect (GTK_OBJECT (win), "delete_event",  
                            (GtkSignalFunc) gtk_widget_destroy, 
                            (gpointer) win);

        vbox = gtk_vbox_new(FALSE, 2);
        gtk_container_add(GTK_CONTAINER(win), vbox);
        
        snprintf(buffer, BUFFER_SIZE, "File %s exists; replace?",
                 fname);
        label = gtk_label_new(buffer);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
        
        hbox = gtk_hbox_new(TRUE, 2);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

        button = gtk_button_new_with_label("Cancel");
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
        gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                   (GtkSignalFunc) gtk_widget_destroy, 
                                   (gpointer) win);
        GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);  
        gtk_widget_grab_default (button);    

        button = gtk_button_new_with_label("Replace");
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
        gtk_object_set_data(GTK_OBJECT (button),
                            "fselector",
                            top);
        gtk_signal_connect (GTK_OBJECT (button), 
                            "clicked",
                            (GtkSignalFunc) playlist_fsel_save_overwrite, 
                            user_data);
        
        gtk_signal_connect (GTK_OBJECT (top), "unrealize",
                            (GtkSignalFunc) fsel_destroy,
                            (gpointer) win);

        gtk_signal_connect (GTK_OBJECT (win), "delete_event",  
                            (GtkSignalFunc) gtk_widget_destroy, 
                            (gpointer) win);
        gtk_widget_show_all(win);
        return;
    }
    
    save_playlist((GList *) user_data, fname);
    gtk_widget_destroy(top);
}


static void fsel_destroy (GtkWidget * widget,
                          gpointer user_data) {
    gtk_widget_destroy((GtkWidget *) user_data);
} 


static void playlist_fsel_save_overwrite (GtkWidget * widget,
                                          gpointer user_data) {
    gchar * fname;
    GtkWidget * top;

    top = (GtkWidget *) gtk_object_get_data (GTK_OBJECT(widget), 
                                             "fselector");
    fname = gtk_file_selection_get_filename (GTK_FILE_SELECTION(top));
    save_playlist((GList *) user_data, fname);
    gtk_widget_destroy(top);

}


static void app_songs_play (GtkWidget * widget,
                            gpointer user_data) {
    GList * list = NULL, * row_list;

    GtkCListRow * row;
    guint i;

    for (row_list = GTK_CLIST(song_list)->row_list, i = 0; 
         row_list;
         row_list = g_list_next(row_list), i++) {
        row = row_list->data;
        if (row->state == GTK_STATE_SELECTED) {
            list = g_list_append(list, SONG(g_list_nth(songs, i)));
        }
    }

    if (list) {
        play_songs(list);
        g_list_free(list);
    }
}

/* Create "choose color" dialog to modify prefs.color. user_data is
   the gtk_pixmap of the color. */
static void choose_color (GtkWidget * widget,
                          gpointer user_data) {
    GtkWidget * window, * vbox, * hbox, * button;
    HB * hb = g_malloc(sizeof(HB));
    memcpy(hb, &prefs.color, sizeof(HB));

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Choose color");
    gtk_container_set_border_width (GTK_CONTAINER (window), 5);
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",  
                        (GtkSignalFunc) gtk_widget_destroy, 
                        (gpointer) window);
    gtk_signal_connect (GTK_OBJECT (window), "unrealize",
                        (GtkSignalFunc) choose_color_unrealize, 
                        hb);
    gtk_signal_connect (GTK_OBJECT (window), "unrealize",
                        (GtkSignalFunc) dialog_unrealize,
                        NULL);
    
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    widget = make_color_wheel(hb);
    gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);

    hbox = gtk_hbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    button = gtk_button_new_with_label("OK");
    gtk_object_set_data(GTK_OBJECT(button), 
                        CC_HB, 
                        hb);
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        (GtkSignalFunc) choose_color_ok,
                        user_data);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
    
    button = gtk_button_new_with_label("Cancel");
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        (GtkSignalFunc) choose_color_cancel, 
                        (gpointer) window);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);

    dialog_list = g_list_append(dialog_list, window);
    gtk_widget_show_all(window);
}

/* The user selected "ok"; copy the HB selection to prefs.color and 
   change the "make playlist" color button's pixmap */
static void choose_color_ok (GtkWidget * widget,
                             gpointer user_data) {
    GtkWidget * button_pm;
    GdkPixmap * pm;
    GdkBitmap * mask;
    HB * hb = gtk_object_get_data(GTK_OBJECT(widget), 
                                  CC_HB);
    /* Update prefs color */
    memcpy(&prefs.color, hb, sizeof(HB));
    
    /* Update pixmap associated with color */
    button_pm = GTK_WIDGET(user_data);
    gtk_pixmap_get (GTK_PIXMAP(button_pm),
                    &pm,
                    &mask);
    gdk_pixmap_unref(pm);
    gtk_pixmap_set (GTK_PIXMAP(button_pm),
                    create_color_pixmap(rgb_to_hex(hsv_to_rgb(hb_to_hsv(prefs.color)))),
                    NULL);
    
    /* Close the window */
    gtk_widget_destroy(gtk_widget_get_toplevel(widget));
}


static void choose_color_cancel (GtkWidget * widget,
                                 gpointer user_data) {
    gtk_widget_destroy(GTK_WIDGET(user_data));
}

static void choose_color_unrealize (GtkWidget * widget,
                                    gpointer user_data) {
    g_free((HB *) user_data);
}


/* Create "choose song" dialog to modify prefs.song_cksum. user_data is
   the label. */
static void choose_song (GtkWidget * widget,
                         gpointer user_data) {
    GtkWidget * window, * swin, * vbox, * hbox, * clist, * button;
    GList * list, * current;
    gint row;
    song * s;
    gchar * list_data[SONG_LAST];
    
    memset(list_data, 0x00, sizeof(gchar *) * SONG_LAST);
    list = g_list_copy(songs);
    list = g_list_sort(list, sort_artist_title);
    
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_usize(window, PLAYLIST_WIDTH, PLAYLIST_HEIGHT);
    gtk_window_set_title (GTK_WINDOW (window), "Choose song");
    gtk_container_set_border_width (GTK_CONTAINER (window), 5);
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",  
                        (GtkSignalFunc) gtk_widget_destroy, 
                        (gpointer) window);
    gtk_signal_connect (GTK_OBJECT (window), "unrealize",
                        (GtkSignalFunc) choose_song_unrealize, 
                        (gpointer) list);
    gtk_signal_connect (GTK_OBJECT (window), "unrealize",
                        (GtkSignalFunc) dialog_unrealize,
                        NULL);
    
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 2);

    clist = make_song_clist();
    gtk_container_add(GTK_CONTAINER(swin), clist);
    for (current = list, row = 0; 
         current; 
         current = g_list_next(current), row++) {
       s = SONG(current);
       gtk_clist_append( GTK_CLIST(clist), list_data);
       build_song_list_row(GTK_CLIST(clist), row, s);
    }
  
    hbox = gtk_hbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    button = gtk_button_new_with_label("OK");
    gtk_object_set_data(GTK_OBJECT (button),
                        CS_LIST,
                        list);
    gtk_object_set_data(GTK_OBJECT (button),
                        CS_LABEL,
                        user_data);
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        (GtkSignalFunc) choose_song_ok,
                        clist);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
    
    button = gtk_button_new_with_label("Cancel");
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        (GtkSignalFunc) choose_song_cancel, 
                        (gpointer) window);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);

    dialog_list = g_list_append(dialog_list, window);
    gtk_widget_show_all(window);
}


static void choose_song_ok (GtkWidget * widget,
                            gpointer user_data) {
    GtkWidget * label;
    GtkCList * clist;
    GList * list, * row_list; 
    GtkCListRow * row;
    gint i;
    song * s;
    gchar buffer[BUFFER_SIZE];

    clist = GTK_CLIST(user_data);
    list = gtk_object_get_data(GTK_OBJECT (widget),
                               CS_LIST);
    label = gtk_object_get_data(GTK_OBJECT (widget),
                               CS_LABEL);

    for (row_list = clist->row_list, i = 0; 
         row_list;
         row_list = g_list_next(row_list), i++) {
        row = row_list->data;
        if (row->state == GTK_STATE_SELECTED) {
            s = SONG(g_list_nth(list, i));
            prefs.song_cksum = s->checksum;
            snprintf(buffer, PLAYLIST_SONG_TITLE_LEN, 
                     "%s", strcmp(s->title, "?") ? s->title: s->fname);
            gtk_label_set_text(GTK_LABEL(label), buffer);
        }
    }
    /* Close the window */
    gtk_widget_destroy(gtk_widget_get_toplevel(widget));
}


static void choose_song_cancel (GtkWidget * widget,
                                gpointer user_data) {
    gtk_widget_destroy(GTK_WIDGET(user_data));
}

static void choose_song_unrealize  (GtkWidget * widget,
                                    gpointer user_data) {
    g_list_free((GList *) user_data);
}


gboolean about_expose (GtkWidget *widget, 
                       GdkEventExpose *event, 
                       gpointer data) {
    gint width, height, w, h;
    GdkGC * gc; 
    GdkPixmap * pm = (GdkPixmap *) data;
    gchar buffer[BUFFER_SIZE];

    width = widget->allocation.width;
    height = widget->allocation.height;

    gc = gdk_gc_new(app_window->window);
    gdk_rgb_gc_set_foreground(gc, 0xFFFFFF);
    gdk_gc_set_clip_rectangle (gc,
                               &event->area);
    gdk_draw_rectangle (widget->window,
                        gc,
                        TRUE,
                        0, 0, 
                        width, height);

    gdk_window_get_size(pm, &w, &h);
    gdk_draw_pixmap ( widget->window,
                      gc,
                      (GdkPixmap *) data,
                      0, 0,
                      (width - w) / 2, 10,
                      w, h);

     gdk_rgb_gc_set_foreground(gc, 0x000000);

     snprintf(buffer, BUFFER_SIZE, "Version %s", GJAY_VERSION);
     w = gdk_text_width(widget->style->font,
                         buffer,
                         strlen(buffer));
     gdk_draw_string (widget->window,
                      widget->style->font,
                      gc, 
                      (width - w) / 2, height * 0.5,
                      buffer);

     snprintf(buffer, BUFFER_SIZE, "Copyright (c) 2002, Chuck Groom");
     w = gdk_text_width(widget->style->font,
                        buffer,
                        strlen(buffer));
     gdk_draw_string (widget->window,
                      widget->style->font,
                      gc, 
                      (width - w) / 2, height * 0.65,
                      buffer);
     gdk_rgb_gc_set_foreground(gc, 0xCC00CC);
     snprintf(buffer, BUFFER_SIZE, "http://gjay.sourceforge.net");
     w = gdk_text_width(widget->style->font,
                         buffer,
                         strlen(buffer));
     gdk_draw_string (widget->window,
                      widget->style->font,
                      gc, 
                      (width - w) / 2, height * 0.8,
                      buffer);


     gdk_gc_unref(gc);

    return TRUE;
}

void make_delete_songs_dialog( GList * list) {
    
    GtkWidget * window, * vbox, * hbox, * button, * label;

    close_all_dialogs();

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Remove songs");
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",  
                        (GtkSignalFunc) gtk_widget_destroy, 
                        (gpointer) window);
    gtk_signal_connect (GTK_OBJECT (window), "unrealize",
                        (GtkSignalFunc) delete_songs_unrealize,
                        (gpointer) list);
    gtk_signal_connect (GTK_OBJECT (window), "unrealize",
                        (GtkSignalFunc) dialog_unrealize,
                        NULL);
    
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    label = gtk_label_new("Are you you sure you\nwant to remove these songs?");
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 2);
    
    hbox = gtk_hbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    button = gtk_button_new_with_label("OK");
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        (GtkSignalFunc) delete_songs_ok,
                        list);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
    
    button = gtk_button_new_with_label("Cancel");
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        (GtkSignalFunc) delete_songs_cancel,
                        (gpointer) window);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 5);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);  
    gtk_widget_grab_default (button); 
    gtk_widget_show_all(window);

    dialog_list = g_list_append(dialog_list, window);
}


static void delete_songs_ok (GtkWidget * widget,
                             gpointer user_data) {
    GList * current;
    song * s;
    for (current = (GList *) user_data; current; current = g_list_next(current)) {
        s = SONG(current);
        songs = g_list_remove(songs, s);
        delete_song(s);
        num_songs--;
    }
    build_song_list();
    
    /* Close the window */
    gtk_widget_destroy(gtk_widget_get_toplevel(widget));
}

static void delete_songs_cancel (GtkWidget * widget,
                                 gpointer user_data) {
     gtk_widget_destroy(GTK_WIDGET(user_data));
}


static void delete_songs_unrealize (GtkWidget * widget,
                                    gpointer user_data) {
    g_list_free((GList *) user_data);
}

/* All dialogs register themselves with dialog_list when they are created,
   and are removed when they are destroyed. */
void dialog_unrealize (GtkWidget * widget,
                       gpointer user_data) {
    dialog_list = g_list_remove(dialog_list, widget);
}


static void close_all_dialogs ( void ) {
    while (dialog_list) 
        gtk_widget_destroy((GtkWidget *) dialog_list->data);
}
