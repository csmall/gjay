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

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include "gjay.h"
#include "gjay_xmms.h"
#include "ui.h"
#include "playlist.h"

enum {
   ARTIST_COLUMN,
   TITLE_COLUMN,
   FREQ_COLUMN,
   COLOR_COLUMN,
   BPM_COLUMN,
   LAST_COLUMN
};

#define PLAYLIST_CW_DIAMETER 50
#define PLAYLIST_STR "playlist"

GtkWidget * button_sel_songs, * button_start_song, * button_sel_dir;
GtkWidget * button_start_color, * time_entry;
GtkWidget * rating_hbox;

static void parent_set_callback (GtkWidget *widget,
                                 gpointer user_data);
static void toggled ( GtkToggleButton *togglebutton,
                      gpointer user_data );
static void toggled_start_selected_song ( GtkToggleButton *togglebutton,
                                          gpointer user_data );
static void toggled_start_color ( GtkToggleButton *togglebutton,
                                  gpointer user_data );
static void value_changed ( GtkRange *range,
                            gpointer user_data );
static void playlist_button_clicked (GtkButton *button,
                                     gpointer user_data);
static GtkWidget * create_float_slider_widget (gchar * name,
                                               gchar * description,
                                               float * value);

/* Playlist window -- what to do with the list of songs we've created */
static void make_playlist_window ( GList * list);
static void playlist_window_play ( GtkButton *button,
                                   gpointer user_data );
static void playlist_window_save ( GtkButton *button,
                                   gpointer user_data );
static gboolean playlist_window_delete (GtkWidget *widget,
                                        GdkEvent *event,
                                        gpointer user_data);
static void  populate_playlist_list (GtkListStore * list_store,
                                     GList * list);

/* Playlist save-as window */
static void save_fsel_clicked ( GtkButton *button,
                                gpointer user_data );
static void confirm_save_response (GtkDialog *dialog,
                                   gint arg1,
                                   gpointer user_data);
static void save_playlist_selector (GtkWidget * file_selector);

static void set_prefs_color ( gpointer data,
                              gpointer user_data);



GtkWidget * make_playlist_view ( void ) {
    GtkWidget * hbox1, * vbox1, * vbox2, * vbox3;
    char buffer[BUFFER_SIZE];
    GtkWidget * button, * label, * frame, * range, * event_box;
    GtkWidget * colorwheel;

    vbox1 = gtk_vbox_new(FALSE, 2);
    
    button_sel_songs = gtk_check_button_new_with_label(
        "Only from selected songs");
    gtk_tooltips_set_tip (tips, button_sel_songs,
                          "Limit the playlist to the selected songs",
                          "");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_sel_songs), 
                                 prefs.use_selected_songs);
    g_signal_connect (G_OBJECT (button_sel_songs), "toggled",
                      G_CALLBACK (toggled), &prefs.use_selected_songs);
    gtk_box_pack_start(GTK_BOX(vbox1), button_sel_songs, FALSE, FALSE, 2);
    button_sel_dir = gtk_check_button_new_with_label(
        "Only within selected directory");
    gtk_tooltips_set_tip (tips, button_sel_dir,
                          "Limit the playlist to songs within this directory",
                          "");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_sel_dir), 
                                 prefs.use_selected_dir);
    g_signal_connect (G_OBJECT (button_sel_dir), "toggled",
                      G_CALLBACK (toggled), &prefs.use_selected_dir);
    gtk_box_pack_start(GTK_BOX(vbox1), button_sel_dir, FALSE, FALSE, 2);
    
    button_start_song = gtk_check_button_new_with_label(
        "Start at selected song");
    gtk_tooltips_set_tip (tips, button_start_song,
                          "Use the selected song as the start of the playlist",
                          "");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_song), 
                                 prefs.start_selected);
    g_signal_connect (G_OBJECT (button_start_song), "toggled",
                      G_CALLBACK (toggled_start_selected_song), NULL);
    gtk_box_pack_start(GTK_BOX(vbox1), button_start_song, FALSE, FALSE, 2);

    hbox1 = gtk_hbox_new(FALSE, 0);    
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);
    button_start_color = gtk_check_button_new_with_label("Start at color");
    gtk_tooltips_set_tip (tips, button_start_color,
                          "Specify a target color for starting the playlist",
                          "");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_color), 
                                 prefs.start_color);
    g_signal_connect (G_OBJECT (button_start_color), "toggled",
                      G_CALLBACK (toggled_start_color), NULL);
    gtk_box_pack_start(GTK_BOX(hbox1), button_start_color, FALSE, FALSE, 2);

    colorwheel = create_colorwheel(PLAYLIST_CW_DIAMETER, 
                                   NULL, 
                                   set_prefs_color, 
                                   &prefs.color);
    set_colorwheel_color(colorwheel, prefs.color);
    gtk_box_pack_start(GTK_BOX(hbox1), colorwheel, FALSE, FALSE, 2);

    frame = gtk_frame_new("Criteria");
    gtk_box_pack_start(GTK_BOX(vbox1), frame, TRUE, TRUE, 2);
    
    vbox2 = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(frame), vbox2);

    hbox1 = gtk_hbox_new(FALSE, 2);     
    gtk_box_pack_start(GTK_BOX(vbox2), hbox1, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "Hue", 
        "Hue: Match songs by the color, ignoring lightness and intensity",
        &prefs.hue);
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "Bright", 
        "Brightness: Match songs by the color light/darkness",
        &prefs.brightness);
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "Satur.", 
        "Saturation: Match songs by the color intensity",
        &prefs.saturation);
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "Freq", 
        "Frequency: Match on how songs sound",
        &prefs.freq);
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "BPM", 
        "BPM: Match on beat",
        &prefs.bpm);
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "File Loc.",
        "File Location: Match songs by their nearness in the file system. Two songs in the same directory are closer than two songs in different directories.",
        &prefs.path_weight);
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);


    button = gtk_check_button_new_with_label("Wander");
    gtk_tooltips_set_tip (tips, button,
                          "If wander is set, each song is matched to the previous song. Otherwise, each song is matched to the first song",
                          "");

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                 prefs.wander);
    g_signal_connect (G_OBJECT (button), "toggled",
                      G_CALLBACK (toggled), &prefs.wander);
    gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 2);

    rating_hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), rating_hbox, FALSE, FALSE, 0);
    button = gtk_check_button_new_with_label("Rating cut-off");
    gtk_tooltips_set_tip (tips, button,
                          "Ignore all songs below a particular rating",
                          "");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                 prefs.rating_cutoff);
    g_signal_connect (G_OBJECT (button), "toggled",
                      G_CALLBACK (toggled), &prefs.rating_cutoff);
    range = gtk_hscale_new_with_range (MIN_RATING, MAX_RATING, .1);
    gtk_range_set_value(GTK_RANGE(range), prefs.rating);
    g_signal_connect (G_OBJECT (range), "value-changed",
                      G_CALLBACK (value_changed), &prefs.rating);
    gtk_box_pack_start(GTK_BOX(rating_hbox), button, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(rating_hbox), range, TRUE, TRUE, 2);

    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);
    label = gtk_label_new("Randomness (little)");
    event_box = gtk_event_box_new();
    gtk_container_add (GTK_CONTAINER(event_box), label);
    range = gtk_hscale_new_with_range (MIN_CRITERIA, MAX_CRITERIA, .1);
    gtk_range_set_inverted(GTK_RANGE(range), TRUE);
    gtk_range_set_value(GTK_RANGE(range), prefs.variance);
    g_signal_connect (G_OBJECT (range), "value-changed",
                      G_CALLBACK (value_changed), &prefs.variance);
    gtk_scale_set_draw_value(GTK_SCALE(range), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox1), event_box, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox1), range, TRUE, TRUE, 2);
    label = gtk_label_new("(very)");
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);
    gtk_tooltips_set_tip (tips, event_box,
                          "Randomness: how tightly focused each song pick should be. Too little randomness, and your playlists will not wander your collection. Too much randomness, and you may as well just shuffle your CDs.",
                          "");

    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);
    label = gtk_label_new("Time (minutes)");
    time_entry = gtk_entry_new_with_max_length (4);
    snprintf(buffer, BUFFER_SIZE, "%d", prefs.time);
    gtk_entry_set_text(GTK_ENTRY(time_entry), buffer);
    gtk_tooltips_set_tip (tips, time_entry,
                          "Time: a target time for how long the playlist should be. The actual playlist length may be +/- a few minutes. CDs tend to be 45-80 minutes long, for what it's worth.",
                          "");

    gtk_widget_set_size_request(time_entry, 35, -1);
    button = gtk_button_new_with_label("Make Playlist");
    gtk_tooltips_set_tip (tips, button,
                          "Generate the playlist using your criteria",
                          "");
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (playlist_button_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox1), time_entry, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, FALSE, 2); 


    g_signal_connect (G_OBJECT (vbox1), "parent_set",
                      G_CALLBACK (parent_set_callback), NULL);
 
    return vbox1;
}


static void parent_set_callback (GtkWidget *widget,
                                 gpointer user_data) {
    song * s;
    GList * ll;
    gint num_songs;
    gboolean is_dir;

    gtk_widget_hide(button_sel_songs);
    gtk_widget_hide(button_sel_dir);
    gtk_widget_hide(button_start_song);

    num_songs = 0;
    is_dir = FALSE;

    if (!selected_songs && selected_files) {
        is_dir = TRUE;
    } else if (selected_songs) {
        for (ll = g_list_first(selected_songs); 
             ll && (num_songs < 2); ll = g_list_next(ll)) {
            s = (song *) ll->data;
            if (!s->no_data)
                num_songs++;
        }
    }
    
    if (is_dir) {
        gtk_widget_show(button_sel_dir);
        prefs.use_selected_songs = FALSE;
        prefs.start_selected = FALSE;
    } else if (num_songs == 1) {
        /* One song is selected */
        gtk_widget_show(button_start_song);
        prefs.use_selected_dir = FALSE;
        prefs.use_selected_songs = FALSE;
    } else if (num_songs > 1) {
        /* Several songs are selected */
        gtk_widget_show(button_sel_songs);
        prefs.use_selected_dir = FALSE;
        prefs.start_selected = FALSE;
    } else {
        /* Nothing is selected */
        prefs.use_selected_songs = FALSE;
        prefs.use_selected_dir = FALSE;
        prefs.start_selected = FALSE;
    }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_sel_songs),
                                 prefs.use_selected_songs);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_sel_dir),
                                 prefs.use_selected_dir);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_song),
                                 prefs.start_selected);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_color),
                                 prefs.start_color);
  
    if (prefs.start_selected && prefs.start_color) {
        prefs.start_color = FALSE;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_color),
                                     prefs.start_color);
    }
}


static void toggled ( GtkToggleButton *togglebutton,
                      gpointer user_data ) {
    *((gboolean *) user_data) = gtk_toggle_button_get_active(togglebutton);
}


static void toggled_start_selected_song ( GtkToggleButton *togglebutton,
                                          gpointer user_data ) {
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_start_song))) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_color),
                                     FALSE);
        prefs.start_selected = TRUE;
    } else {
        prefs.start_selected = FALSE;
    }
}


static void toggled_start_color ( GtkToggleButton *togglebutton,
                                  gpointer user_data ) {
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_start_color))) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_song),
                                     FALSE);
        prefs.start_color= TRUE;
    } else {
        prefs.start_color= FALSE;
    }
}


static void value_changed ( GtkRange *range,
                            gpointer user_data ) {
    *((float *) user_data) = gtk_range_get_value(range);
}


static void playlist_button_clicked (GtkButton *button,
                                     gpointer user_data) {
    GList * playlist;
    const gchar * text;
    char buffer[BUFFER_SIZE];
    int time;
    
    text = gtk_entry_get_text(GTK_ENTRY(time_entry));
    if(sscanf(text, "%d", &time) == 0) {
        time = DEFAULT_PLAYLIST_TIME;
        snprintf(buffer, BUFFER_SIZE, "%d", time);
        gtk_entry_set_text(GTK_ENTRY(time_entry), buffer);
    }
    prefs.time = time;
    playlist = generate_playlist(prefs.time);
    if (playlist)
        make_playlist_window(playlist);
}



static void make_playlist_window ( GList * list) {
    GList * ll;
    GtkWidget * window;
    GtkWidget * vbox, * hbox, * hbox2, * label, * swin, * button;
    GtkWidget * image, * tree;
    GtkCellRenderer * text_renderer, * pixbuf_renderer;
    GtkTreeViewColumn *column;
    GtkListStore * list_store;
    gint time;
    gchar time_buffer[BUFFER_SIZE];

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request(window, PLAYLIST_WIDTH, PLAYLIST_HEIGHT);
    gtk_container_set_border_width (GTK_CONTAINER (window), 3);

    g_signal_connect (G_OBJECT (window),
                      "delete_event",
                      G_CALLBACK (playlist_window_delete),
                      list);
    gtk_window_set_title (GTK_WINDOW (window), "GJay: Playlist");
    
    vbox = gtk_vbox_new (FALSE, 2);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    list_store = gtk_list_store_new(LAST_COLUMN, 
                                    G_TYPE_STRING, 
                                    G_TYPE_STRING,
                                    GDK_TYPE_PIXBUF,
                                    GDK_TYPE_PIXBUF,
                                    G_TYPE_STRING);
    populate_playlist_list(list_store, list);
    tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
    g_object_unref (G_OBJECT (list_store));
    text_renderer = gtk_cell_renderer_text_new ();
    pixbuf_renderer = gtk_cell_renderer_pixbuf_new ();
    column = gtk_tree_view_column_new_with_attributes ("Artist", text_renderer,
                                                       "text", ARTIST_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
    column = gtk_tree_view_column_new_with_attributes ("Title", text_renderer,
                                                       "text", TITLE_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
    column = gtk_tree_view_column_new_with_attributes ("Freq", pixbuf_renderer,
                                                       "pixbuf", FREQ_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
    column = gtk_tree_view_column_new_with_attributes ("Color", 
                                                       pixbuf_renderer,
                                                       "pixbuf", COLOR_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
    column = gtk_tree_view_column_new_with_attributes ("BPM", text_renderer,
                                                       "text", BPM_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(swin), tree);
    gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 2);
    

    hbox = gtk_hbox_new (FALSE, 2);
    for (time = 0, ll = list; ll; ll = g_list_next(ll)) 
        time += ((song *) ll->data)->length;
    snprintf(time_buffer, BUFFER_SIZE, "%d minutes", time/60);
    label = gtk_label_new(time_buffer);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = new_button_label_pixbuf("Play", PM_BUTTON_PLAY);
    g_signal_connect (G_OBJECT (button), 
                      "clicked",
                      G_CALLBACK (playlist_window_play),
                      list);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    hbox2 = gtk_hbox_new (FALSE, 2);
    image = gtk_image_new_from_stock (GTK_STOCK_SAVE, 
                                      GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(hbox2), image, TRUE, TRUE, 0);
    label = gtk_label_new("Save");
    gtk_box_pack_start(GTK_BOX(hbox2), label, TRUE, TRUE, 0);
    button = gtk_button_new();
    g_signal_connect (G_OBJECT (button), 
                      "clicked",
                      G_CALLBACK (playlist_window_save),
                      list);
    gtk_container_add (GTK_CONTAINER (button), hbox2);

    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gtk_widget_show_all(window);
}


static void playlist_window_play ( GtkButton *button,
                                   gpointer user_data ) {
    GList * fnames = NULL, * list;
    for (list = (GList *) user_data; list; list = g_list_next(list)) {
        fnames = g_list_append(fnames, strdup_to_latin1(SONG(list)->path));
    }
    play_files(fnames);
    for (list = g_list_first(fnames); list; list = g_list_next(list)) 
        g_free(list->data);
    g_list_free(fnames);
}


static void playlist_window_save ( GtkButton *button,
                                   gpointer user_data ) {
    GtkWidget * file_selector;
    GList * list;

    list = (GList *) user_data;
    file_selector = gtk_file_selection_new("Save playlist as...");
    g_object_set_data (G_OBJECT (file_selector),
                       PLAYLIST_STR,
                       list);
    g_signal_connect (GTK_OBJECT(GTK_FILE_SELECTION(file_selector)->ok_button),
                      "clicked",
                      G_CALLBACK (save_fsel_clicked),
                      NULL);
    
    g_signal_connect_swapped (GTK_OBJECT (GTK_FILE_SELECTION (file_selector)->cancel_button),
                              "clicked",
                              G_CALLBACK (gtk_widget_destroy),
                              (gpointer) file_selector); 
    gtk_widget_show(file_selector);
}


static void save_fsel_clicked ( GtkButton *button,
                                gpointer user_data ) {
    const gchar *selected_filename;
    GtkWidget * file_selector, * dialog;

    file_selector = gtk_widget_get_toplevel(GTK_WIDGET(button));
    selected_filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (file_selector));
    
    if (access(selected_filename, W_OK) == 0) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(file_selector),
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_MESSAGE_QUESTION,
                                              GTK_BUTTONS_YES_NO,
                                              "Overwrite '%s'?",
                                              selected_filename);
        g_signal_connect (GTK_OBJECT (dialog), 
                          "response",
                          G_CALLBACK (confirm_save_response),
                          file_selector);
        gtk_widget_show(dialog);
    } else {
        save_playlist_selector(file_selector);
    }
}


static void confirm_save_response (GtkDialog *dialog,
                                   gint arg1,
                                   gpointer user_data) {
    if (arg1 == GTK_RESPONSE_YES) {
        save_playlist_selector(GTK_WIDGET(user_data));
    } else {
        gtk_widget_destroy(GTK_WIDGET(dialog));
    }
}



static gboolean playlist_window_delete ( GtkWidget *widget,
                                         GdkEvent *event,
                                         gpointer user_data) {
    GList * list = (GList *) user_data;
    /* Fixme: Free color_pixbuf */
    g_list_free(list);
    gtk_widget_destroy(widget);
    return TRUE;
}



static void  populate_playlist_list(GtkListStore * list_store,
                                    GList * list) {
    GList * llist;
    song * s;
    gchar * artist, * title;
    gchar bpm[20];
    GtkTreeIter iter;

    for (llist = g_list_first(list); 
         llist; 
         llist = g_list_next(llist)) {
        s = (song *) llist->data;
        if (s->artist)
            artist = s->artist;
        else
            artist = NULL;
        if (s->title && (strlen(s->title) > 1) ) 
            title = s->title;
        else 
            title = s->fname;
        if (s->no_data)
            sprintf(bpm, "?");
        else if (s->bpm_undef)
            sprintf(bpm, "Unsure"); 
        else
            sprintf(bpm, "%3.2f", s->bpm);
        song_set_freq_pixbuf(s);
        song_set_color_pixbuf(s);
        gtk_list_store_append (list_store, &iter);
        gtk_list_store_set (list_store, &iter,
                            ARTIST_COLUMN, artist,
                            TITLE_COLUMN, title,
                            FREQ_COLUMN, s->freq_pixbuf,
                            COLOR_COLUMN, s->color_pixbuf,
                            BPM_COLUMN, bpm,
                            -1);
    }
}


static void save_playlist_selector(GtkWidget * file_selector) {
    GList * list;
    const gchar *selected_filename;
    
    selected_filename = gtk_file_selection_get_filename (
        GTK_FILE_SELECTION (file_selector));
    list = (GList *) g_object_get_data(G_OBJECT(file_selector), PLAYLIST_STR);
    
    save_playlist(list, (char *) selected_filename);
    gtk_widget_destroy(file_selector);
}


void set_playlist_rating_visible ( gboolean is_visible ) {
    if (is_visible) {
        gtk_widget_show(rating_hbox);
    } else {
        gtk_widget_hide(rating_hbox);
    }
}

GtkWidget * create_float_slider_widget (gchar * name,
                                        gchar * description,
                                        float * value) {
    GtkWidget * vbox, * event_box, * range, * label;

    vbox = gtk_vbox_new(FALSE, 2);
    label = gtk_label_new(name);
    event_box = gtk_event_box_new();
    gtk_container_add (GTK_CONTAINER(event_box), label);
    range = gtk_vscale_new_with_range (MIN_CRITERIA, MAX_CRITERIA, .1);
    gtk_range_set_value(GTK_RANGE(range), *value);
    g_signal_connect (G_OBJECT (range), "value-changed",
                      G_CALLBACK (value_changed), value);
    gtk_scale_set_draw_value(GTK_SCALE(range), FALSE);
    gtk_range_set_inverted(GTK_RANGE(range), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), event_box, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox), range, TRUE, TRUE, 2);
    if (tips && description) {
        gtk_tooltips_set_tip (tips, 
                              event_box,
                              description,
                              "");
    }
    return vbox;
}


static void set_prefs_color ( gpointer data,
                              gpointer user_data) {
    assert(data && user_data);
    *((HSV *) user_data) = get_colorwheel_color(GTK_WIDGET(data));
}

