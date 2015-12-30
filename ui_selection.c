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
#include <math.h>
#include <gtk/gtk.h>
#include <string.h>
#include "gjay.h"
#include "ui.h"
#include "ui_private.h"
#include "colors.h"
#include "play_common.h"
#include "ui_explore.h"
#include "ui_selection.h"
#include "i18n.h"
#include "songs.h"

enum {
   ARTIST_COLUMN,
   TITLE_COLUMN,
   FREQ_COLUMN,
   BPM_COLUMN,
   LAST_COLUMN
};

struct SelectUI {
    GtkWidget *widget;
    GtkWidget *selections[N_EXPLORE_SELECTIONS];
    enum explore_show current_selection;
    GtkWidget *select_all;

    /* Song details panel */
    GtkWidget *rating_label;
    GtkWidget *song_icon;
    GtkWidget *song_play;
    GtkWidget *song_artist;
    GtkWidget *song_title;
    GtkWidget *song_freq;
    GtkWidget *song_bpm;
    GtkWidget *song_color;
    GtkWidget *song_rating;

    /* Directory details panel */
    GtkListStore *list_store;
    GtkWidget *dir_icon;
    GtkWidget *dir_show;
    GtkWidget *dir_play;
    GtkWidget *dir_lbox;
    GtkWidget *dir_tree;
};

static void     update_song_has_rating_color ( GjayApp *gjay,
											GjaySong * s );
static void     update_dir_has_rating_color  ( GjayApp *gjay,
											const gchar * dir );
static gchar * parent_dir (const gchar *root_dir, const gchar * path );

/* How many chars should we truncate displayed file names to? */
#define TRUNC_NAME 18

/* Size of color swatch */
#define COLOR_SWATCH_WIDTH 30
#define COLOR_SWATCH_HEIGHT 20

/**
 * Files is a list of UTF8-encoded paths
 */
static void
set_selected_files (GjayApp *gjay, GList * files)
{
    GList * llist;
    GjaySong * s;
    SelectUI *sui;
    GjayGUI *ui;

    if (!files)
        return;

    song_all_clear(gjay);

    for (llist = g_list_first(files); llist; llist = g_list_next(llist)) {
        if (g_hash_table_lookup(gjay->songs->not_hash, llist->data)) {
            g_free(llist->data);
        } else {
            s = g_hash_table_lookup(gjay->songs->name_hash, llist->data);
            if (!s) {
                /* This may happen a directory contains an empty directory, 
                   so the file list includes a directory path and not
                   a song path. */
                g_free(llist->data);
            } else {
                if (s->freq_pixbuf) {
                    g_object_unref(s->freq_pixbuf);
                    s->freq_pixbuf = NULL;
                }
                gjay->selected_songs = g_list_append(gjay->selected_songs, s);
                gjay->selected_files = g_list_append(gjay->selected_files, llist->data);
            } 
        }
    }
    ui = gjay_get_gui(gjay);
    sui = ui->selection_view;
    gtk_widget_show(sui->dir_play);
    gtk_widget_hide(sui->dir_show);
    gtk_widget_show(sui->dir_icon);
    gtk_widget_show(sui->dir_lbox);
    //gtk_label_set_text(GTK_LABEL(label_type), "Contains...");
    gtk_image_set_from_pixbuf (GTK_IMAGE(sui->dir_icon),
                               gjay_get_pixbuf(gjay, PM_ICON_OPEN));
    selection_redraw(sui, gjay->selected_songs);
}

/*
 * Callback function for play button in song details
 * Finds current song and plays it
 */
static gboolean
song_play_clicked(GtkWidget *button,
                  GdkEventButton *event,
                  gpointer user_data)
{
    GjayApp *gjay = user_data;
    GjayGUI *ui = gjay_get_gui(gjay);

    play_songs(gjay->player, ui->main_window, gjay->selected_songs);
    return TRUE;
}


/* 
 * Callback function for play button in directory details
 * Plays all selected songs from the directory list
 */
static gboolean
dir_play_clicked (GtkWidget *widget,
                  GdkEventButton *event,
                  gpointer user_data)
{
    GjayApp *gjay = user_data;
    GjayGUI *ui = gjay_get_gui(gjay);

    play_songs(gjay->player, ui->main_window, gjay->selected_songs);
    return TRUE;
}

/*
 * Callback function for select_all on directory listing
 * shows all songs in the list
 */
static gboolean
dir_show_clicked (GtkWidget *widget,
                  GdkEventButton *event,
                  gpointer user_data)
{
    GList * list;
    GjayApp *gjay=(GjayApp*)user_data;

    if (gjay->selected_files) {
        list = explore_files_in_dir(gjay->gui->explore_page,
                                    g_list_first(gjay->selected_files)->data,
                                    TRUE);
        set_selected_files(gjay, list);
    }
    return TRUE;
}


static void rating_changed ( GtkRange *range,
                             gpointer user_data )
{
    GList * llist;
    GjaySong * s;
    gboolean had_color_rating;
    gdouble val;
    GjayApp *gjay=(GjayApp*)user_data;

    val = gtk_range_get_value(range);

    for (llist = g_list_first(gjay->selected_songs); llist; 
         llist = g_list_next(llist)) {
        s = (GjaySong *) llist->data;

        if (s->no_color | s->no_rating) {
            had_color_rating = FALSE;
        } else {
            had_color_rating = TRUE;
        }

        s->no_rating = FALSE;
        s->rating = val;
        /* If other songs mirror this one, pass on the change */
        song_set_repeat_attrs(s);

        /* If this song was not previously assigned a color or rating,
           update how it is displayed */
        if (!had_color_rating) {
            update_song_has_rating_color(gjay,s);
        }
    }
    gtk_label_set_text (GTK_LABEL(gjay->gui->selection_view->rating_label), "Rating");

    gjay->songs->dirty = TRUE;
}


static void
dir_color_set(GtkColorButton *button,
              gpointer user_data)
{
    GList * llist;
    GjaySong * s;
    gboolean had_color_rating;
    HSV hsv;
    GjayApp *gjay=(GjayApp*)user_data;

    if (!gjay->selected_songs)
        return;

    colorbutton_get_color(button, &hsv);

    llist = g_list_first(gjay->selected_songs);
    while (llist) {
        s = (GjaySong *) llist->data;
        if (s->no_color | s->no_rating) {
            had_color_rating = FALSE;
        } else {
            had_color_rating = TRUE;
        }

        s->no_color = FALSE;
        s->color = hsv;

        /* If other songs mirror this one, pass on the change */
        song_set_repeat_attrs(s);

        /* If this song was not previously assigned a color or rating,
           update how it is displayed */
        if (!had_color_rating)
            update_song_has_rating_color(gjay, s);

        llist = g_list_next(llist);
    }

    gjay->songs->dirty = TRUE;
}


static GtkWidget *
create_selection_none(GjayApp *gjay, struct SelectUI *sui)
{
    GtkWidget *frame, *label;

    //song_all_clear(gjay);
    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_widget_set_usize(frame, APP_WIDTH * 0.5, -1);

    label = gtk_label_new(_("Nothing Selected"));
    gtk_container_add(GTK_CONTAINER(frame), label);
    return frame;
}

static GtkWidget *
create_selection_dir(GjayApp *gjay, struct SelectUI *sui)
{
    GtkWidget *frame, *vbox, *top_hbox, *swin, *hbox2;
    GtkWidget *dir_color, *dir_rating, *alignment;
    GtkCellRenderer * text_renderer, * pixbuf_renderer;
    GtkTreeViewColumn *column;

    frame = gtk_frame_new(_("Directory Details"));
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_widget_set_usize(frame, APP_WIDTH * 0.5, -1);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    top_hbox = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), top_hbox, FALSE, FALSE, 0);

    sui->dir_icon =  gtk_image_new_from_pixbuf(
                         gjay_get_pixbuf(gjay, PM_ICON_CLOSED));
    gtk_box_pack_start(GTK_BOX(top_hbox), sui->dir_icon, FALSE, FALSE, 0);

    sui->dir_show = gjay_button_new_with_label_pixbuf(gjay, NULL,
                                                      PM_BUTTON_ALL);
    gtk_widget_set_tooltip_text (sui->dir_show,
                                 _("Select all songs in this directory"));
    g_signal_connect(G_OBJECT(sui->dir_show), "clicked",
                     G_CALLBACK(dir_show_clicked), gjay);
    gtk_box_pack_start(GTK_BOX(top_hbox), sui->dir_show, FALSE, FALSE, 0);

    sui->dir_play = gjay_button_new_with_label_pixbuf(gjay, _("Play"),
                                                    PM_BUTTON_PLAY);
    g_signal_connect(G_OBJECT(sui->dir_play), "clicked",
                     G_CALLBACK(dir_play_clicked), gjay);
    gtk_box_pack_start(GTK_BOX(top_hbox), sui->dir_play, FALSE, FALSE, 0);

    sui->dir_lbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox), sui->dir_lbox, TRUE, TRUE, 2);

    sui->list_store = gtk_list_store_new(LAST_COLUMN, G_TYPE_STRING,
                                         G_TYPE_STRING, GDK_TYPE_PIXBUF,
                                         G_TYPE_STRING);
    sui->dir_tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (sui->list_store));
    g_object_unref (G_OBJECT (sui->list_store));
    text_renderer = gtk_cell_renderer_text_new ();
    pixbuf_renderer = gtk_cell_renderer_pixbuf_new ();
    column = gtk_tree_view_column_new_with_attributes ("Artist", text_renderer,
                                                       "text", ARTIST_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (sui->dir_tree), column);
    column = gtk_tree_view_column_new_with_attributes ("Title", text_renderer,
                                                       "text", TITLE_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (sui->dir_tree), column);
    column = gtk_tree_view_column_new_with_attributes ("Freq", pixbuf_renderer,
                                                       "pixbuf", FREQ_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (sui->dir_tree), column);
    column = gtk_tree_view_column_new_with_attributes ("BPM", text_renderer,
                                                       "text", BPM_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (sui->dir_tree), column);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(swin), sui->dir_tree);

    gtk_box_pack_start(GTK_BOX(sui->dir_lbox), swin, TRUE, TRUE, 2);

    hbox2 = gtk_hbox_new (FALSE, 2);
    gtk_box_pack_start(GTK_BOX(sui->dir_lbox), hbox2, FALSE, FALSE, 2);

    dir_color = gtk_color_button_new();
    colorbutton_set_callback(GTK_COLOR_BUTTON(dir_color),
                             G_CALLBACK(dir_color_set),
                             gjay);
    gtk_box_pack_start(GTK_BOX(hbox2), dir_color, FALSE, FALSE, 2);
    alignment = gtk_alignment_new (0, 1, 0.1, 0.1);
    gtk_box_pack_start(GTK_BOX(hbox2), alignment, FALSE, FALSE, 2);

    dir_rating = gtk_vbox_new (FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox2), dir_rating, TRUE, FALSE, 2);
    return frame;
}


static GtkWidget *
create_selection_file(GjayApp *gjay, SelectUI *sui)
{
    GtkWidget *frame, *label, *table, *vbox, *align, *top_hbox;
    int i;
    gchar *headings[] = {"Artist", "Title", "Freq", "BPM", "Color", 0};

    frame = gtk_frame_new(_("Song Details"));
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_widget_set_usize(frame, APP_WIDTH * 0.5, -1);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    top_hbox = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), top_hbox, FALSE, FALSE, 0);

    sui->song_icon =  gtk_image_new_from_pixbuf(
                                    gjay_get_pixbuf(gjay, PM_ICON_CLOSED));
    gtk_box_pack_start(GTK_BOX(top_hbox), sui->song_icon, FALSE, FALSE, 0);

    sui->song_play = gjay_button_new_with_label_pixbuf(gjay, _("Play"),
                                                       PM_BUTTON_PLAY);
    g_signal_connect(G_OBJECT(sui->song_play), "clicked",
                     G_CALLBACK(song_play_clicked), gjay);
    gtk_box_pack_start(GTK_BOX(top_hbox), sui->song_play, FALSE, FALSE, 0);

    table = gtk_table_new(6, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

    for(i=0; headings[i]; i++) {
        gchar *buf;
        label = gtk_label_new(NULL);
        buf = g_strdup_printf("<b>%s</b>", headings[i]);
        gtk_label_set_markup(GTK_LABEL(label), buf);
        g_free(buf);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, i, i+1,
                          GTK_FILL, GTK_FILL, 10, 5);
    }
    sui->song_artist = gtk_label_new(_("Unknown"));
    align = gtk_alignment_new(0, 0.5, 0, 0);
    gtk_container_add(GTK_CONTAINER(align), sui->song_artist);
    gtk_table_attach_defaults(GTK_TABLE(table),
                              align, 1, 2, 0, 1);
    sui->song_title = gtk_label_new(_("Unknown"));
    align = gtk_alignment_new(0, 0.5, 0, 0);
    gtk_container_add(GTK_CONTAINER(align), sui->song_title);
    gtk_table_attach_defaults(GTK_TABLE(table),
                              align, 1, 2, 1, 2);
    sui->song_freq = gtk_image_new_from_pixbuf(
                            gjay_get_pixbuf(gjay, PM_ICON_CLOSED));
    align = gtk_alignment_new(0, 0.5, 0, 0);
    gtk_container_add(GTK_CONTAINER(align), sui->song_freq);
    gtk_table_attach_defaults(GTK_TABLE(table),
                              align, 1, 2, 2, 3);
    sui->song_bpm = gtk_label_new(_("Unknown"));
    align = gtk_alignment_new(0, 0.5, 0, 0);
    gtk_container_add(GTK_CONTAINER(align), sui->song_bpm);
    gtk_table_attach_defaults(GTK_TABLE(table),
                              align, 1, 2, 3, 4);
    sui->song_color = gtk_color_button_new();
    align = gtk_alignment_new(0, 0.5, 0, 0);
    gtk_container_add(GTK_CONTAINER(align), sui->song_color);
    gtk_table_attach_defaults(GTK_TABLE(table),
                              align, 1, 2, 4, 5);

    /* Rating is optional */
    sui->rating_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(sui->rating_label), "<b>Rating:</b>");
    gtk_table_attach(GTK_TABLE(table), sui->rating_label, 0, 1, 5, 6,
                     GTK_FILL, GTK_FILL, 10, 5);
    sui->song_rating = gtk_hscale_new_with_range(MIN_RATING,MAX_RATING, 0.2);
    g_signal_connect (G_OBJECT(sui->song_rating), "value-changed",
                      G_CALLBACK (rating_changed), gjay);
    align = gtk_alignment_new(0, 0.5, 1, 0);
    gtk_container_add(GTK_CONTAINER(align), sui->song_rating);
    gtk_table_attach_defaults(GTK_TABLE(table),
                              align, 1, 2, 5, 6);
    return frame;
}


static void
selection_panel_show(GtkWidget *w, gpointer user_data)
{
    int i;
    SelectUI *sui = (SelectUI*)user_data;

    for (i=EXPLORE_NONE; i < N_EXPLORE_SELECTIONS; i++) {
        if (i == sui->current_selection)
            gtk_widget_show(sui->selections[i]);
        else
            gtk_widget_hide(sui->selections[i]);
    }
}

static void
selection_set_panel(SelectUI *sui, enum explore_show panel)
{
    sui->current_selection = panel;
    selection_panel_show(sui->widget, sui);
}

/*
 * Show the directory details within the selection panel
 */
void
selection_set_dir(GjayApp *gjay, const gchar *file, const char *short_name)
{
    SelectUI *sui = gjay->gui->selection_view;

    //song_all_clear(gjay);
    selection_set_panel(sui, EXPLORE_DIR);
    if (g_hash_table_lookup(gjay->new_song_dirs_hash, file)) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(sui->dir_icon),
                                  gjay_get_pixbuf(gjay,
                                                  PM_ICON_CLOSED_NEW));
    } else {
        gtk_image_set_from_pixbuf(GTK_IMAGE(sui->dir_icon),
                                  gjay_get_pixbuf(gjay,
                                                  PM_ICON_CLOSED));
    }
    gjay->selected_files = g_list_append(gjay->selected_files, g_strdup(file));
    selection_redraw(sui, gjay->selected_songs);
}


/*
 * Create a new Selection UI object.
 */
SelectUI *
selection_new(GjayApp *gjay)
{
    SelectUI *sui;

    sui = g_malloc0(sizeof(SelectUI));
    sui->current_selection = EXPLORE_NONE;
    sui->widget = gtk_hbox_new(FALSE, 0);

    sui->selections[EXPLORE_NONE] = create_selection_none(gjay, sui);
    gtk_box_pack_start_defaults(GTK_BOX(sui->widget),
                                      sui->selections[EXPLORE_NONE]);
    sui->selections[EXPLORE_DIR] = create_selection_dir(gjay, sui);
    gtk_box_pack_start_defaults(GTK_BOX(sui->widget),
                                      sui->selections[EXPLORE_DIR]);

    sui->selections[EXPLORE_FILE] = create_selection_file(gjay, sui);
    gtk_box_pack_start_defaults(GTK_BOX(sui->widget),
                                      sui->selections[EXPLORE_FILE]);
    g_signal_connect(G_OBJECT(sui->widget), "show",
                     G_CALLBACK(selection_panel_show),sui);
    return sui;
}


void
selection_destroy(SelectUI *sui)
{
    gtk_widget_destroy(sui->widget);
}
#if 0
GtkWidget *
make_selection_view ( GjayApp *gjay ) {
    GtkWidget * vbox1, * vbox2, * hbox1, * hbox2;
    GtkWidget * alignment, * event_box, * swin, *color_button;
    GtkCellRenderer * text_renderer, * pixbuf_renderer;
    GtkTreeViewColumn *column;
	struct play_songs_data *psd;
 
    vbox1 = gtk_vbox_new (FALSE, 2);  
    
    hbox1 = gtk_hbox_new (FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);
    icon = gtk_image_new_from_pixbuf (gjay_get_pixbuf(gjay->gui, PM_ICON_CLOSED));
    gtk_box_pack_start(GTK_BOX(hbox1), icon, FALSE, FALSE, 2);

    vbox2 = gtk_vbox_new(FALSE, 2);
    label_name = gtk_label_new("");
    label_type = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox1), vbox2, FALSE, FALSE, 2);

    alignment = gtk_alignment_new (0, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(alignment), label_name);
    gtk_box_pack_start(GTK_BOX(vbox2), alignment, FALSE, FALSE, 2);

    alignment = gtk_alignment_new (0, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(alignment), label_type);
    gtk_box_pack_start(GTK_BOX(vbox2), alignment, FALSE, FALSE, 2);

    alignment = gtk_alignment_new (1, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), alignment, TRUE, TRUE, 2);

    hbox2 = gtk_hbox_new (FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment), hbox2);

    event_box = gtk_event_box_new ();
    gtk_box_pack_start(GTK_BOX(hbox2), event_box, FALSE, FALSE, 2);

    play = gtk_image_new_from_pixbuf(gjay->gui->pixbufs[PM_BUTTON_PLAY]);
    gtk_widget_set_tooltip_text(event_box,
        "Play the selected songs in the music player");
    gtk_container_add (GTK_CONTAINER(event_box), play);
    gtk_widget_set_events (event_box, GDK_BUTTON_PRESS_MASK);

	psd = g_malloc0(sizeof(struct play_songs_data));
	psd->player = gjay->player;
	psd->main_window = gjay->gui->main_window;
	psd->playlist = gjay->selected_songs;
    g_signal_connect (G_OBJECT(event_box), "button_press_event",
                      G_CALLBACK(play_selected), psd);

    event_box = gtk_event_box_new ();
    gtk_box_pack_start(GTK_BOX(hbox2), event_box, FALSE, FALSE, 2);
    
    event_box = gtk_event_box_new ();
    gtk_box_pack_start(GTK_BOX(hbox2), event_box, FALSE, FALSE, 2);
    select_all_recursive = gtk_image_new_from_pixbuf(gjay->gui->pixbufs[PM_BUTTON_ALL]);
    gtk_widget_set_tooltip_text (event_box,
        "Select all songs in this directory");
    gtk_container_add (GTK_CONTAINER(event_box), select_all_recursive);
    gtk_widget_set_events (event_box, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(event_box), "button_press_event",
                     G_CALLBACK (select_all_selected), gjay);

    vbox_lower = gtk_vbox_new (FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox1), vbox_lower, TRUE, TRUE, 2);

    list_store = gtk_list_store_new(LAST_COLUMN,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    GDK_TYPE_PIXBUF,
                                    G_TYPE_STRING);
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
    column = gtk_tree_view_column_new_with_attributes ("BPM", text_renderer,
                                                       "text", BPM_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
    
    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(swin), tree);

    gtk_box_pack_start(GTK_BOX(vbox_lower), swin, TRUE, TRUE, 2);
    
    
    hbox2 = gtk_hbox_new (FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox_lower), hbox2, FALSE, FALSE, 2);

    color_button = colorbutton_new(gjay);
    colorbutton_set_callback(GTK_COLOR_BUTTON(color_button),
                             G_CALLBACK(update_selected_songs_color),
                             gjay);
    gtk_box_pack_start(GTK_BOX(hbox2), color_button, FALSE, FALSE, 2);
    alignment = gtk_alignment_new (0, 1, 0.1, 0.1);
    gtk_box_pack_start(GTK_BOX(hbox2), alignment, FALSE, FALSE, 2);

    rating_vbox = gtk_vbox_new (FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox2), rating_vbox, TRUE, FALSE, 2);    
    label_rating = gtk_label_new("Rating");
    gtk_box_pack_start(GTK_BOX(rating_vbox), label_rating, FALSE, FALSE, 2);
    
    rating = gtk_vscale_new_with_range  (MIN_RATING,
                                         MAX_RATING,
                                         0.2);
    g_signal_connect (G_OBJECT(rating), "value-changed",
                      G_CALLBACK (rating_changed), gjay);

    gtk_range_set_inverted(GTK_RANGE(rating), TRUE);
    alignment = gtk_alignment_new (0.5, 0, 
                                   0.5, 1);
    gtk_container_add(GTK_CONTAINER(alignment), rating);
    gtk_box_pack_start(GTK_BOX(rating_vbox), alignment, TRUE, TRUE, 2);

    return vbox1;
}



void
set_selected_file ( GjayApp *gjay,
					const gchar * file, 
                    char * short_name, 
                    const gboolean is_dir )
{
    char short_name_trunc[BUFFER_SIZE];
    GList * llist;
    int pm_type;
    GjaySong * s;

    song_all_clear(gjay);

    if (file == NULL) {
        /* Hide everything */
        gtk_widget_hide(icon);
        gtk_widget_hide(play);
        gtk_widget_hide(select_all_recursive);
        gtk_widget_hide(vbox_lower);
        gtk_label_set_text(GTK_LABEL(label_name), "Nothing selected");
        return;
    }

    gtk_widget_show(icon);   
        
    strncpy(short_name_trunc, short_name, BUFFER_SIZE);
    if (strlen(short_name) > TRUNC_NAME)
        memcpy(short_name_trunc + TRUNC_NAME, "...\0", 4);
    
    if (is_dir) {
        gtk_label_set_text(GTK_LABEL(label_name), short_name_trunc);
        if (g_hash_table_lookup(gjay->new_song_dirs_hash, file)) {
            gtk_image_set_from_pixbuf (GTK_IMAGE(icon), 
                                       gjay->gui->pixbufs[PM_ICON_CLOSED_NEW]);
            gtk_label_set_text(GTK_LABEL(label_type), 
                               "Has uncategorized songs");
        } else {
            gtk_image_set_from_pixbuf (GTK_IMAGE(icon), 
                                       gjay->gui->pixbufs[PM_ICON_CLOSED]);
            gtk_label_set_text(GTK_LABEL(label_type), "");
        }
        gtk_widget_hide(play);

        gtk_widget_show(select_all_recursive);
        gtk_widget_hide(vbox_lower);
        gjay->selected_files = g_list_append(gjay->selected_files, g_strdup(file));
    } else {
        gtk_widget_hide(select_all_recursive);
    
        s = g_hash_table_lookup(gjay->songs->name_hash, file);
        
        if (s) {
            gtk_label_set_text(GTK_LABEL(label_name), "");

            gtk_widget_show(play);
            if (s) {
                pm_type = PM_ICON_SONG;
                gtk_label_set_text(GTK_LABEL(label_type), "");
            } else {
                pm_type = PM_ICON_PENDING;
                gtk_label_set_text(GTK_LABEL(label_type), "Will be analyzed");
            }
            gjay->selected_songs = g_list_append(gjay->selected_songs, s);
            gtk_widget_show(vbox_lower);
        } else {
            gtk_label_set_text(GTK_LABEL(label_name), short_name_trunc);
            gtk_widget_hide(play);
            gtk_widget_hide(vbox_lower);

            if (g_hash_table_lookup(gjay->songs->not_hash, file)) {
                pm_type = PM_ICON_NOSONG;
                gtk_label_set_text(GTK_LABEL(label_type), "Not a song");
            } else {
                pm_type = PM_ICON_CLOSED;
                gtk_label_set_text(GTK_LABEL(label_type), "Empty");
            }
        }

        gtk_image_set_from_pixbuf (GTK_IMAGE(icon),
                                   gjay->gui->pixbufs[pm_type]);
        gjay->selected_files = g_list_append(gjay->selected_files,
                                             g_strdup(file));
    }
    selection_redraw(gjay->selected_songs);
}
#endif

/**
 * If a directory is selected when we enter playlist view, hide
 * directory selection buttons and show them again when we leave that
 * mode */
void selection_set_playall ( GjayApp *gjay, const gboolean in_view ) {
    gchar * fname;

    if (gjay->selected_files == NULL)
        return;
    if (g_list_length(gjay->selected_files) > 1)
        return;
    fname = (gchar *) gjay->selected_files->data;
    if ( g_hash_table_lookup(gjay->songs->name_hash, fname) ||
         g_hash_table_lookup(gjay->songs->not_hash, fname))
        return;
    if (in_view) {
        gtk_widget_hide(gjay->gui->selection_view->dir_show);
    } else {
        gtk_widget_show(gjay->gui->selection_view->dir_show);
    }
}


static
void get_selected_color (GList *selected_songs,
                         HSV * color,
                         gboolean * no_color,
                         gboolean * multiple_colors) {
    RGB rgb, rgb_sum;
    int num = 0;
    GList * llist;
    GjaySong * s;

    assert(color && no_color && multiple_colors);
    *no_color = TRUE;
    bzero(&rgb_sum, sizeof(RGB));
    for (llist = g_list_first(selected_songs);
         llist;
         llist = g_list_next(llist)) {
        s = (GjaySong *) llist->data;
        assert(s);
        if (!s->no_color) {
            num++;
            *no_color = FALSE;
            hsv_to_rgb(&(s->color), &rgb);
            rgb_sum.R += rgb.R;
            rgb_sum.G += rgb.G;
            rgb_sum.B += rgb.B;
        }
    }
    if (num > 1) {
        *multiple_colors = TRUE;
        rgb_sum.R /= (float) num;
        rgb_sum.G /= (float) num;
        rgb_sum.B /= (float) num;
    } else {
        *multiple_colors = FALSE;
    }
    rgb_to_hsv(&rgb_sum, color);
}


static void
populate_selected_list (SelectUI *sui,
                        GList *selected_songs)
{
    GList * llist;
    GjaySong * s;
    gchar * artist, * title;
    gchar bpm[20];
    GtkTreeIter iter;

    gtk_list_store_clear (GTK_LIST_STORE(sui->list_store));
    for (llist = g_list_first(selected_songs);
         llist;
         llist = g_list_next(llist)) {
        s = (GjaySong *) llist->data;
        assert(s);
        if (s->artist)
            artist = s->artist;
        else
            artist = NULL;
        if (s->title && strlen(s->title) > 1)
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
        gtk_list_store_append (GTK_LIST_STORE(sui->list_store), &iter);
        gtk_list_store_set (GTK_LIST_STORE(sui->list_store), &iter,
                            ARTIST_COLUMN, artist,
                            TITLE_COLUMN, title,
                            FREQ_COLUMN, s->freq_pixbuf,
                            BPM_COLUMN, bpm,
                            -1);
    }
    gtk_tree_view_columns_autosize(GTK_TREE_VIEW(sui->dir_tree));
}

static void
redraw_rating (SelectUI *sui, GList *selected_songs)
{
    GList * llist;
    GjaySong * s;
    gdouble lower, upper, total;
    gint n, k;

    lower = MAX_RATING + 1;
    upper = -1;
    total = 0;
    for (n = 0, k = 0, llist = g_list_first(selected_songs);
         llist; 
         llist = g_list_next(llist)) {
        s = (GjaySong *) llist->data;
        if (!s->no_rating) {
            total += s->rating;
            lower = MIN(lower, s->rating);
            upper = MAX(upper, s->rating);
            k++;
        } 
        n++;
    }
    if (k == 0) {
        gtk_range_set_value (GTK_RANGE(sui->song_rating), DEFAULT_RATING);
        gtk_label_set_text (GTK_LABEL(sui->rating_label), "Not rated");
    } else if ((n == k) && (lower == upper)) {
        gtk_range_set_value (GTK_RANGE(sui->song_rating), total / k);
        gtk_label_set_text (GTK_LABEL(sui->rating_label), "Rating");
    } else {
        gtk_range_set_value (GTK_RANGE(sui->song_rating), total / k);
        gtk_label_set_text (GTK_LABEL(sui->rating_label), "Avg. rating");
    }
}

void
selection_redraw (SelectUI *sui, GList *selected_songs)
{
    HSV color;
    gboolean no_color;
    gboolean multiple_colors;

    populate_selected_list(sui, selected_songs);
    get_selected_color(selected_songs, &color, &no_color, &multiple_colors);

    redraw_rating(sui, selected_songs);
}


static void update_song_has_rating_color ( GjayApp *gjay, GjaySong * s ) {
    gchar * dir;
    
    for (; s->repeat_prev; s = s->repeat_prev)
        ;
    for (; s; s = s->repeat_next) {
        dir = parent_dir(gjay->prefs->song_root_dir, s->path);
        update_dir_has_rating_color(gjay, dir);
        g_free(dir);
    }
}


static void update_dir_has_rating_color (GjayApp *gjay, const gchar * dir ) {
    gchar * parent;
    gchar * str;
    
	if (dir == NULL)
	  return;
    str = g_hash_table_lookup(gjay->new_song_dirs_hash, dir);
    if (!str) 
        return;

    if (explore_dir_has_new_songs(gjay->gui->explore_page,
                                  gjay->songs->name_hash, dir,
                                  gjay->verbosity))
        return;

    /* This directory used to be marked, but no longer is. Change its
       icon, remove info */
    explore_path_set_icon(gjay->gui->explore_page, dir,
                          gjay->gui->pixbufs[PM_DIR_CLOSED]);
    
    g_hash_table_remove(gjay->new_song_dirs_hash, str);
    gjay->new_song_dirs = g_list_remove(gjay->new_song_dirs, str);
    g_free(str);
    
    /* Check the parent directory */
    parent = parent_dir (gjay->prefs->song_root_dir, gjay->prefs->song_root_dir);
    update_dir_has_rating_color(gjay, parent);
    g_free(parent);
} 

void
selection_set_rating_visible(SelectUI *sui, gboolean is_visible)
{
    if (is_visible) {
        gtk_widget_show(sui->rating_label);
        gtk_widget_show(sui->song_rating);
    } else {
        gtk_widget_hide(sui->rating_label);
        gtk_widget_hide(sui->song_rating);
    }
}

/**
 * Get the parent directory of path. The returned value should be freed.
 * If the parent is above root, return NULL.
 */
static gchar * parent_dir (const gchar *root_dir, const gchar * path ) {
  int path_len, root_len;

  if (g_str_has_prefix(path, root_dir) == FALSE)
	return NULL;

  path_len = strlen(path);
  root_len = strlen(root_dir);

  if (path_len <= root_len)
    return NULL;
  if (path_len == 0)
    return NULL;

  for (; path_len > root_len; path_len--)
	if (path[path_len] == '/') 
	  return g_strndup(path, path_len);

  if (path[path_len - 1] == '/')
    path_len--;
  return g_strndup(path, path_len);
}

GtkWidget*
selection_get_widget(SelectUI *sui)
{
    return sui->widget;
}


static void
song_color_set(GtkColorButton *button,
               gpointer user_data)
{
    GjayApp *gjay = (GjayApp*)user_data;
    GjaySong *s;

    s = SONG(gjay->selected_songs);

    colorbutton_get_color(button, &(s->color));
    gjay->songs->dirty = TRUE;
}


void
selection_set_none(GjayApp *gjay)
{
    SelectUI *sui = gjay->gui->selection_view;

    //song_all_clear(gjay);

    selection_set_panel(sui, EXPLORE_NONE);
}

/*
 * Show the song details in the selection panel
 */
void
selection_set_file(GjayApp *gjay, const gchar *file, const char *short_name)
{
    GjaySong *s;
    SelectUI *sui;

    song_all_clear(gjay);

    sui = gjay->gui->selection_view;
    selection_set_panel(sui, EXPLORE_FILE);

    s = song_by_filename(gjay, file);
    if (s) {
        gchar *bpm;
        gtk_image_set_from_pixbuf(GTK_IMAGE(sui->song_icon),
                                  gjay_get_pixbuf(gjay, PM_ICON_SONG));
        gtk_widget_show(sui->song_play);
        gtk_label_set_text(GTK_LABEL(sui->song_artist), s->artist);
        gtk_label_set_text(GTK_LABEL(sui->song_title), s->title);
        bpm = song_bpm(s);
        gtk_label_set_text(GTK_LABEL(sui->song_bpm), bpm);
        g_free(bpm);
        song_set_freq_pixbuf(s);
        gtk_image_set_from_pixbuf(GTK_IMAGE(sui->song_freq),
                                  s->freq_pixbuf);
        colorbutton_set_color(GTK_COLOR_BUTTON(sui->song_color),
                              &(s->color));
        colorbutton_set_callback(GTK_COLOR_BUTTON(sui->song_color),
                                 G_CALLBACK(song_color_set),
                                 gjay);

        gjay->selected_songs = g_list_append(gjay->selected_songs, s);
    } else {
        /* not a song? */
        gtk_image_set_from_pixbuf(GTK_IMAGE(sui->song_icon),
                                  gjay_get_pixbuf(gjay, PM_ICON_NOSONG));
        gtk_widget_hide(sui->song_play);
    }
    gjay->selected_files = g_list_append(gjay->selected_files, g_strdup(file));
}
