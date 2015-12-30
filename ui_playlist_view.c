/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright 2002,2003 Chuck Groom
 * Copyright 2010-2015 Craig Small 
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

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gjay.h"
#include "ui.h"
#include "ui_private.h"
#include "playlist.h"
#include "play_common.h"
#include "i18n.h"

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

void uiparent_set_callback (GtkWidget *widget,
							GtkWidget *old_parent,
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
static void make_playlist_window ( GjayApp *gjay, GList * list);
static void playlist_window_play ( GtkButton *button,
                                   gpointer user_data );
static void playlist_window_save ( GtkButton *button,
                                   gpointer user_data );
static gboolean playlist_window_delete (GtkWidget *widget,
                                        GdkEvent *event,
                                        gpointer user_data);
static void  populate_playlist_list (GtkListStore * list_store,
                                     GList * list);

static void color_button_color_set(GtkColorButton *button,
                                   gpointer user_data)
{
    colorbutton_get_color(button,(HSV*)user_data);
}

GtkWidget * make_playlist_view ( GjayApp *gjay ) {
    GtkWidget * hbox1, * vbox1, * vbox2, * vbox3;
    char buffer[BUFFER_SIZE];
    GtkWidget * button, * label, * frame, * range, * event_box;
    GtkWidget * color_button;

    vbox1 = gtk_vbox_new(FALSE, 2);

    button_sel_songs = gtk_check_button_new_with_label(
        "Only from selected songs");
    gtk_widget_set_tooltip_text (button_sel_songs,
        "Limit the playlist to the selected songs");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_sel_songs), 
                                 gjay->prefs->use_selected_songs);
    g_signal_connect (G_OBJECT (button_sel_songs), "toggled",
                      G_CALLBACK (toggled),
                      &(gjay->prefs->use_selected_songs));
    gtk_box_pack_start(GTK_BOX(vbox1), button_sel_songs, FALSE, FALSE, 2);
    button_sel_dir = gtk_check_button_new_with_label(
        "Only within selected directory");
    gtk_widget_set_tooltip_text (button_sel_dir,
        "Limit the playlist to songs within this directory");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_sel_dir), 
                                 gjay->prefs->use_selected_dir);
    g_signal_connect (G_OBJECT (button_sel_dir), "toggled",
                      G_CALLBACK (toggled),
                      &(gjay->prefs->use_selected_dir));
    gtk_box_pack_start(GTK_BOX(vbox1), button_sel_dir, FALSE, FALSE, 2);
    
    button_start_song = gtk_check_button_new_with_label(
        "Start at selected song");
    gtk_widget_set_tooltip_text (button_start_song,
        "Use the selected song as the start of the playlist");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_song), 
                                 gjay->prefs->start_selected);
    g_signal_connect (G_OBJECT (button_start_song), "toggled",
                      G_CALLBACK (toggled_start_selected_song), gjay->prefs);
    gtk_box_pack_start(GTK_BOX(vbox1), button_start_song, FALSE, FALSE, 2);

    hbox1 = gtk_hbox_new(FALSE, 0);    
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);
    button_start_color = gtk_check_button_new_with_label("Start at color");
    gtk_widget_set_tooltip_text (button_start_color,
        "Specify a target color for starting the playlist");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_color), 
                                 gjay->prefs->use_color);
    g_signal_connect (G_OBJECT (button_start_color), "toggled",
                      G_CALLBACK (toggled_start_color), gjay->prefs);
    gtk_box_pack_start(GTK_BOX(hbox1), button_start_color, FALSE, FALSE, 2);

    color_button = colorbutton_new(gjay);
    colorbutton_set_color(GTK_COLOR_BUTTON(color_button), &(gjay->prefs->start_color));
    colorbutton_set_callback(GTK_COLOR_BUTTON(color_button),
                             G_CALLBACK(color_button_color_set),
                             &(gjay->prefs->start_color));
    gtk_box_pack_start(GTK_BOX(hbox1), color_button, FALSE, FALSE, 2);

    frame = gtk_frame_new("Criteria");
    gtk_box_pack_start(GTK_BOX(vbox1), frame, TRUE, TRUE, 2);
    
    vbox2 = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(frame), vbox2);

    hbox1 = gtk_hbox_new(FALSE, 2);     
    gtk_box_pack_start(GTK_BOX(vbox2), hbox1, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "Hue", 
        "Hue: Match songs by the color, ignoring lightness and intensity",
        &(gjay->prefs->hue));
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "Bright", 
        "Brightness: Match songs by the color light/darkness",
        &(gjay->prefs->brightness));
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "Satur.", 
        "Saturation: Match songs by the color intensity",
        &(gjay->prefs->saturation));
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "Freq", 
        "Frequency: Match on how songs sound",
        &(gjay->prefs->freq));
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "BPM", 
        "BPM: Match on beat",
        &(gjay->prefs->bpm));
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);

    vbox3 = create_float_slider_widget(
        "File Loc.",
        "File Location: Match songs by their nearness in the file system. Two songs in the same directory are closer than two songs in different directories.",
        &(gjay->prefs->path_weight));
    gtk_box_pack_start(GTK_BOX(hbox1), vbox3, TRUE, TRUE, 2);


    button = gtk_check_button_new_with_label("Wander");
    gtk_widget_set_tooltip_text (button,
        "If wander is set, each song is matched to the previous song. Otherwise, each song is matched to the first song");

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                 gjay->prefs->wander);
    g_signal_connect (G_OBJECT (button), "toggled",
                      G_CALLBACK (toggled), &(gjay->prefs->wander));
    gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 2);

    rating_hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), rating_hbox, FALSE, FALSE, 0);
    button = gtk_check_button_new_with_label("Rating cut-off");
    gtk_widget_set_tooltip_text (button,
        "Ignore all songs below a particular rating");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                 gjay->prefs->rating_cutoff);
    g_signal_connect (G_OBJECT (button), "toggled",
                      G_CALLBACK (toggled), &(gjay->prefs->rating_cutoff));
    range = gtk_hscale_new_with_range (MIN_RATING, MAX_RATING, .1);
    gtk_range_set_value(GTK_RANGE(range), gjay->prefs->rating);
    g_signal_connect (G_OBJECT (range), "value-changed",
                      G_CALLBACK (value_changed), &(gjay->prefs->rating));
    gtk_box_pack_start(GTK_BOX(rating_hbox), button, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(rating_hbox), range, TRUE, TRUE, 2);

    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);
    label = gtk_label_new("Randomness (little)");
    event_box = gtk_event_box_new();
    gtk_container_add (GTK_CONTAINER(event_box), label);
    range = gtk_hscale_new_with_range (MIN_CRITERIA, MAX_CRITERIA, .1);
    gtk_range_set_inverted(GTK_RANGE(range), TRUE);
    gtk_range_set_value(GTK_RANGE(range), gjay->prefs->variance);
    g_signal_connect (G_OBJECT (range), "value-changed",
                      G_CALLBACK (value_changed), &(gjay->prefs->variance));
    gtk_scale_set_draw_value(GTK_SCALE(range), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox1), event_box, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox1), range, TRUE, TRUE, 2);
    label = gtk_label_new("(very)");
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);
    gtk_widget_set_tooltip_text (event_box,
        "Randomness: how tightly focused each song pick should be. Too little randomness, and your playlists will not wander your collection. Too much randomness, and you may as well just shuffle your CDs.");

    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);
    label = gtk_label_new("Time (minutes)");
    time_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(time_entry), 4);
    snprintf(buffer, BUFFER_SIZE, "%d", gjay->prefs->playlist_time);
    gtk_entry_set_text(GTK_ENTRY(time_entry), buffer);
    gtk_widget_set_tooltip_text (time_entry,
        "Time: a target time for how long the playlist should be. The actual playlist length may be +/- a few minutes. CDs tend to be 45-80 minutes long, for what it's worth.");

    gtk_widget_set_size_request(time_entry, 35, -1);
    button = gtk_button_new_with_label("Make Playlist");
    gtk_widget_set_tooltip_text (button,
        "Generate the playlist using your criteria");
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (playlist_button_clicked), gjay);
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox1), time_entry, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, FALSE, 2); 


    g_signal_connect (G_OBJECT (vbox1), "parent_set",
                      G_CALLBACK (uiparent_set_callback), gjay);
 
    return vbox1;
}


void uiparent_set_callback (GtkWidget *widget,
							GtkWidget *old_parent,
                                 gpointer user_data) {
    GjaySong * s;
    GList * ll;
    gint num_songs;
    gboolean is_dir;
	GjayApp *gjay=(GjayApp*)user_data;

    gtk_widget_hide(button_sel_songs);
    gtk_widget_hide(button_sel_dir);
    gtk_widget_hide(button_start_song);

    num_songs = 0;
    is_dir = FALSE;

    if (!gjay->selected_songs && gjay->selected_files) {
        is_dir = TRUE;
    } else if (gjay->selected_songs) {
        for (ll = g_list_first(gjay->selected_songs); 
             ll && (num_songs < 2); ll = g_list_next(ll)) {
            s = (GjaySong *) ll->data;
            if (!s->no_data)
                num_songs++;
        }
    }
    
    if (is_dir) {
        gtk_widget_show(button_sel_dir);
        gjay->prefs->use_selected_songs = FALSE;
        gjay->prefs->start_selected = FALSE;
    } else if (num_songs == 1) {
        /* One song is selected */
        gtk_widget_show(button_start_song);
        gjay->prefs->use_selected_dir = FALSE;
        gjay->prefs->use_selected_songs = FALSE;
    } else if (num_songs > 1) {
        /* Several songs are selected */
        gtk_widget_show(button_sel_songs);
        gjay->prefs->use_selected_dir = FALSE;
        gjay->prefs->start_selected = FALSE;
    } else {
        /* Nothing is selected */
        gjay->prefs->use_selected_songs = FALSE;
        gjay->prefs->use_selected_dir = FALSE;
        gjay->prefs->start_selected = FALSE;
    }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_sel_songs),
                                 gjay->prefs->use_selected_songs);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_sel_dir),
                                 gjay->prefs->use_selected_dir);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_song),
                                 gjay->prefs->start_selected);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_color),
                                 gjay->prefs->use_color);
  
    if (gjay->prefs->start_selected && gjay->prefs->use_color) {
        gjay->prefs->use_color = FALSE;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_color),
                                     gjay->prefs->use_color);
    }
}


static void toggled ( GtkToggleButton *togglebutton,
                      gpointer user_data ) {
    *((gboolean *) user_data) = gtk_toggle_button_get_active(togglebutton);
}


static void toggled_start_selected_song ( GtkToggleButton *togglebutton,
                                          gpointer user_data ) {
  GjayPrefs *prefs=(GjayPrefs*)user_data;
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_start_song))) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_color),
                                     FALSE);
        prefs->start_selected = TRUE;
    } else {
        prefs->start_selected = FALSE;
    }
}


static void toggled_start_color ( GtkToggleButton *togglebutton,
                                  gpointer user_data ) {
  GjayPrefs *prefs=(GjayPrefs*)user_data;
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_start_color))) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_start_song),
                                     FALSE);
        prefs->use_color= TRUE;
    } else {
        prefs->use_color= FALSE;
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
	GjayApp *gjay=(GjayApp*)user_data;
    
    text = gtk_entry_get_text(GTK_ENTRY(time_entry));
    if(sscanf(text, "%d", &time) == 0) {
        time = DEFAULT_PLAYLIST_TIME;
        snprintf(buffer, BUFFER_SIZE, "%d", time);
        gtk_entry_set_text(GTK_ENTRY(time_entry), buffer);
    }
    gjay->prefs->playlist_time = time;
    playlist = generate_playlist(gjay, gjay->prefs->playlist_time);
    if (playlist)
        make_playlist_window(gjay, playlist);
}



static void make_playlist_window ( GjayApp *gjay, GList * playlist) {
    GList * ll;
    GtkWidget * window;
    GtkWidget * vbox, * hbox, * hbox2, * label, * swin, * button;
    GtkWidget * image, * tree;
    GtkCellRenderer * text_renderer, * pixbuf_renderer;
    GtkTreeViewColumn *column;
    GtkListStore * list_store;
    gint time;
    gchar time_buffer[BUFFER_SIZE];
	struct play_songs_data *ps_data;

	ps_data = g_malloc0(sizeof(struct play_songs_data));
	ps_data->player = gjay->player;
	ps_data->main_window = gjay->gui->main_window;

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request(window, PLAYLIST_WIDTH, PLAYLIST_HEIGHT);
    gtk_container_set_border_width (GTK_CONTAINER (window), 3);

    g_signal_connect (G_OBJECT (window),
                      "delete_event",
                      G_CALLBACK (playlist_window_delete),
                      playlist);
    gtk_window_set_title (GTK_WINDOW (window), "GJay: Playlist");
    
    vbox = gtk_vbox_new (FALSE, 2);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    list_store = gtk_list_store_new(LAST_COLUMN, 
                                    G_TYPE_STRING, 
                                    G_TYPE_STRING,
                                    GDK_TYPE_PIXBUF,
                                    GDK_TYPE_PIXBUF,
                                    G_TYPE_STRING);
    populate_playlist_list(list_store, playlist);
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
    for (time = 0, ll = playlist; ll; ll = g_list_next(ll)) 
        time += ((GjaySong *) ll->data)->length;
    snprintf(time_buffer, BUFFER_SIZE, "%d minutes", time/60);
    label = gtk_label_new(time_buffer);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = new_button_label_pixbuf("Play", PM_BUTTON_PLAY, gjay->gui->pixbufs);
	ps_data->playlist = playlist;
    g_signal_connect (G_OBJECT (button), 
                      "clicked",
                      G_CALLBACK (playlist_window_play),
                      ps_data);
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
                      playlist);
    gtk_container_add (GTK_CONTAINER (button), hbox2);

    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gtk_widget_show_all(window);
}


static void playlist_window_play ( GtkButton *button,
                                   gpointer user_data ) {
  struct play_songs_data *psd=(struct play_songs_data*)user_data;
  play_songs(psd->player, psd->main_window, psd->playlist);
}


static void playlist_window_save ( GtkButton *button,
                                   gpointer user_data ) {
    GtkWidget * dialog;
    GList * list;

    list = (GList *) user_data;
    dialog = gtk_file_chooser_dialog_new(
                                _("Save playlist"),
                                NULL,
                                GTK_FILE_CHOOSER_ACTION_SAVE,
                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
                                                   TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), PLAYLIST_STR);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        save_playlist(list, filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
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
    GjaySong * s;
    gchar * artist, * title;
    gchar bpm[20];
    GtkTreeIter iter;

    for (llist = g_list_first(list); 
         llist; 
         llist = g_list_next(llist)) {
        s = (GjaySong *) llist->data;
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
    if (description)
      gtk_widget_set_tooltip_text(event_box,description);
    return vbox;
}
