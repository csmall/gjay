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

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <string.h>
#include "gjay.h"
#include "gjay_xmms.h"
#include "ui.h"
#include "rgbhsv.h"

enum {
   ARTIST_COLUMN,
   TITLE_COLUMN,
   FREQ_COLUMN,
   BPM_COLUMN,
   LAST_COLUMN
};

GList               * selected_songs = NULL; /* Songs which have been
                                                selected */
GList               * selected_files = NULL; /* List of selected file names */
static GtkListStore * list_store;
static GtkWidget    * icon, * play, * select_all_recursive;
static GtkWidget    * label_name, * label_type, * label_rating;
static GtkWidget    * tree, * vbox_lower, * cwheel, * rating;

static void     set_selected_files (GList * files);
static gboolean play_selected (GtkWidget *widget,
                               GdkEventButton *event,
                               gpointer user_data);
static gboolean select_all_selected (GtkWidget *widget,
                                     GdkEventButton *event,
                                     gpointer user_data);
static void     rating_changed ( GtkRange *range,
                                 gpointer user_data );
static void     populate_selected_list (void);
static void     redraw_rating (void);
static void     update_song_has_rating_color ( song * s );
static void     update_dir_has_rating_color  ( gchar * dir );

/* How many chars should we truncate displayed file names to? */
#define TRUNC_NAME 18

GtkWidget * make_selection_view ( void ) {
    GtkWidget * vbox1, * vbox2, * vbox3, * hbox1, * hbox2;
    GtkWidget * alignment, * event_box, * swin;
    GtkCellRenderer * text_renderer, * pixbuf_renderer;
    GtkTreeViewColumn *column;
 
    vbox1 = gtk_vbox_new (FALSE, 2);  
    
    hbox1 = gtk_hbox_new (FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);
    icon = gtk_image_new_from_pixbuf (pixbufs[PM_ICON_CLOSED]);
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

    play = gtk_image_new_from_pixbuf(pixbufs[PM_BUTTON_PLAY]);
    gtk_tooltips_set_tip (tips, event_box,
                          "Play the selected songs in XMMS", "");
    gtk_container_add (GTK_CONTAINER(event_box), play);
    gtk_widget_set_events (event_box, GDK_BUTTON_PRESS_MASK);
    gtk_signal_connect (GTK_OBJECT(event_box), 
                        "button_press_event",
			GTK_SIGNAL_FUNC (play_selected), 
                        NULL);

    event_box = gtk_event_box_new ();
    gtk_box_pack_start(GTK_BOX(hbox2), event_box, FALSE, FALSE, 2);
    
    event_box = gtk_event_box_new ();
    gtk_box_pack_start(GTK_BOX(hbox2), event_box, FALSE, FALSE, 2);
    select_all_recursive = gtk_image_new_from_pixbuf(pixbufs[PM_BUTTON_ALL]);
    gtk_tooltips_set_tip (tips, event_box,
                          "Select all songs in this directory", "");
    gtk_container_add (GTK_CONTAINER(event_box), select_all_recursive);
    gtk_widget_set_events (event_box, GDK_BUTTON_PRESS_MASK);
    gtk_signal_connect (GTK_OBJECT(event_box), 
                        "button_press_event",
			GTK_SIGNAL_FUNC (select_all_selected), 
                        (gpointer *) TRUE);
    
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

    cwheel = create_colorwheel(COLORWHEEL_DIAMETER,
                               &selected_songs,
                               NULL);
    gtk_box_pack_start(GTK_BOX(hbox2), cwheel, FALSE, FALSE, 2);
    
    vbox3 = gtk_vbox_new (FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox2), vbox3, TRUE, FALSE, 2);    
    label_rating = gtk_label_new("Rating");
    gtk_box_pack_start(GTK_BOX(vbox3), label_rating, FALSE, FALSE, 2);
    
    rating=  gtk_vscale_new_with_range  (MIN_RATING,
                                         MAX_RATING,
                                         0.2);
    gtk_signal_connect (GTK_OBJECT(rating),
                        "value-changed",
                        GTK_SIGNAL_FUNC (rating_changed),
                        NULL);
   
    gtk_range_set_inverted(GTK_RANGE(rating), TRUE);
    alignment = gtk_alignment_new (0.5, 0, 
                                   0.5, 1);
    gtk_container_add(GTK_CONTAINER(alignment), rating);
    gtk_box_pack_start(GTK_BOX(vbox3), alignment, TRUE, TRUE, 2);

    return vbox1;
}


/**
 * If a directory is selected when we enter playlist view, hide
 * directory selection buttons and show them again when we leave that
 * mode */
void set_selected_in_playlist_view ( gboolean in_view ) {
    gchar * fname;
    
    if (selected_files == NULL)
        return;
    if (g_list_length(selected_files) > 1)
        return;
    fname = (gchar *) selected_files->data;
    if ( g_hash_table_lookup(song_name_hash, fname) ||
         g_hash_table_lookup(not_song_hash, fname))
        return;
    if (in_view) {
        gtk_widget_hide(select_all_recursive);
    } else {
        gtk_widget_show(select_all_recursive);
    }
}


void set_selected_file ( char * file, 
                         char * short_name, 
                         gboolean is_dir ) {
    char short_name_trunc[BUFFER_SIZE];
    GList * llist;
    int pm_type;
    song * s;

    for (llist = g_list_first(selected_files); 
         llist;
         llist = g_list_next(llist))
        g_free(llist->data);
    g_list_free(selected_files);
    g_list_free(selected_songs);
    selected_files = NULL;
    selected_songs = NULL;

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
        if (g_hash_table_lookup(new_song_dirs_hash, file)) {
            gtk_image_set_from_pixbuf (GTK_IMAGE(icon), 
                                       pixbufs[PM_ICON_CLOSED_NEW]);
            gtk_label_set_text(GTK_LABEL(label_type), "Has new songs");
        } else {
            gtk_image_set_from_pixbuf (GTK_IMAGE(icon), 
                                       pixbufs[PM_ICON_CLOSED]);
            gtk_label_set_text(GTK_LABEL(label_type), "");
        }
        gtk_widget_hide(play);

        gtk_widget_show(select_all_recursive);
        gtk_widget_hide(vbox_lower);
        selected_files = g_list_append(selected_files, g_strdup(file));
    } else {
        gtk_widget_hide(select_all_recursive);
    
        s = g_hash_table_lookup(song_name_hash, file);
        
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
            selected_songs = g_list_append(selected_songs, s);
            gtk_widget_show(vbox_lower);
        } else {
            gtk_label_set_text(GTK_LABEL(label_name), short_name_trunc);
            gtk_widget_hide(play);
            gtk_widget_hide(vbox_lower);

            if (g_hash_table_lookup(not_song_hash, file)) {
                pm_type = PM_ICON_NOSONG;
                gtk_label_set_text(GTK_LABEL(label_type), "Not a song");
            } else {
                pm_type = PM_ICON_CLOSED;
                gtk_label_set_text(GTK_LABEL(label_type), "Empty");
            }
        }
        
        gtk_image_set_from_pixbuf (GTK_IMAGE(icon), 
                                   pixbufs[pm_type]);
        selected_files = g_list_append(selected_files, g_strdup(file));
    }
    update_selection_area();
}


/**
 * Files is a list of UTF8-encoded paths
 */
static void set_selected_files (GList * files) {
    GList * llist;
    song * s;

    if (!files)
        return; 

    for (llist = g_list_first(selected_files); 
         llist; 
         llist = g_list_next(llist)) {
        g_free(llist->data);
    }
    g_list_free(selected_files);
    g_list_free(selected_songs);
    selected_songs = NULL;
    selected_files = NULL;

    for (llist = g_list_first(files); llist; llist = g_list_next(llist)) {
        if (g_hash_table_lookup(not_song_hash, llist->data)) {
            g_free(llist->data);
        } else {
            s = g_hash_table_lookup(song_name_hash, llist->data);
            if (!s) {
                /* This may happen a directory contains an empty directory, 
                   so the file list includes a directory path and not
                   a song path. */
                g_free(llist->data);
            } else {
                if (s->freq_pixbuf) {
                    gdk_pixbuf_unref(s->freq_pixbuf);
                    s->freq_pixbuf = NULL;
                }
                selected_songs = g_list_append(selected_songs, s);
                selected_files = g_list_append(selected_files, llist->data);
            } 
        }
    }
    
    gtk_widget_show(play);
    gtk_widget_hide(select_all_recursive);
    gtk_widget_show(icon);
    gtk_widget_show(vbox_lower);
    gtk_label_set_text(GTK_LABEL(label_type), "Contains...");
    gtk_image_set_from_pixbuf (GTK_IMAGE(icon), pixbufs[PM_ICON_OPEN]);
    update_selection_area();
}


static gboolean play_selected (GtkWidget *widget,
                               GdkEventButton *event,
                               gpointer user_data) {
    play_songs(selected_songs);
    return TRUE;
}


static gboolean select_all_selected (GtkWidget *widget,
                                     GdkEventButton *event,
                                     gpointer user_data) {
    GList * list;
    if (selected_files) {
        list = explore_files_in_dir(g_list_first(selected_files)->data, 
                                    (gboolean) user_data);
        set_selected_files(list);
    }
    return TRUE;
}



void update_selection_area (void) {
    populate_selected_list();
    redraw_rating();
    gtk_widget_queue_draw(cwheel);
}


void populate_selected_list (void) {
    GList * llist;
    song * s;
    gchar * artist, * title;
    gchar bpm[20];
    GtkTreeIter iter;

    gtk_list_store_clear (GTK_LIST_STORE(list_store)); 
    for (llist = g_list_first(selected_songs); 
         llist; 
         llist = g_list_next(llist)) {
        s = (song *) llist->data;
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
        gtk_list_store_append (GTK_LIST_STORE(list_store), &iter);
        gtk_list_store_set (GTK_LIST_STORE(list_store), &iter,
                            ARTIST_COLUMN, artist,
                            TITLE_COLUMN, title,
                            FREQ_COLUMN, s->freq_pixbuf,
                            BPM_COLUMN, bpm,
                            -1);
    }
    gtk_tree_view_columns_autosize(GTK_TREE_VIEW(tree));
}


void update_selected_songs_color ( gdouble angle, 
                                   gdouble radius ) {
    GList * llist;
    song * s;
    gboolean had_color_rating;
    gchar * dir = NULL;

    if (!selected_songs)
        return;

    s = SONG(selected_songs);
    dir = parent_dir(s->path);

    
    for (llist = g_list_first(selected_songs); llist; 
         llist = g_list_next(llist)) {
        s = (song *) llist->data;

        if (s->no_color | s->no_rating) {
            had_color_rating = FALSE;
        } else {
            had_color_rating = TRUE;
        }
        
        s->no_color = FALSE;
        s->color.H = (float) angle;
        s->color.B = (float) radius;
        
        /* If other songs mirror this one, pass on the change */
        song_set_repeat_attrs(s);

        /* If this song was not previously assigned a color or rating,
           update how it is displayed */
        if (!had_color_rating) 
            update_song_has_rating_color(s);
    }

    songs_dirty = TRUE;
}


void redraw_rating (void) {
    GList * llist;
    song * s;
    gdouble lower, upper, total;
    gint n, k;
    
    lower = MAX_RATING + 1;
    upper = -1;
    total = 0;
    for (n = 0, k = 0, llist = g_list_first(selected_songs);
         llist; 
         llist = g_list_next(llist)) {
        s = (song *) llist->data;
        if (!s->no_rating) {
            total += s->rating;
            lower = MIN(lower, s->rating);
            upper = MAX(upper, s->rating);
            k++;
        } 
        n++;
    }
    if (k == 0) {
        gtk_range_set_value (GTK_RANGE(rating), DEFAULT_RATING);
        gtk_label_set_text (GTK_LABEL(label_rating), "Not rated");
    } else if ((n == k) && (lower == upper)) {
        gtk_range_set_value (GTK_RANGE(rating), total / k);
        gtk_label_set_text (GTK_LABEL(label_rating), "Rating");
    } else {
        gtk_range_set_value (GTK_RANGE(rating), total / k);
        gtk_label_set_text (GTK_LABEL(label_rating), "Avg. rating");
    }
}


static void rating_changed ( GtkRange *range,
                             gpointer user_data ) {
    GList * llist;
    song * s;
    gboolean had_color_rating;
    gdouble val;

    val = gtk_range_get_value(range);

    for (llist = g_list_first(selected_songs); llist; 
         llist = g_list_next(llist)) {
        s = (song *) llist->data;

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
            update_song_has_rating_color(s);
        }
    }
    gtk_label_set_text (GTK_LABEL(label_rating), "Rating");

    songs_dirty = TRUE;
}



static void update_song_has_rating_color ( song * s ) {
    gchar * dir;
    
    for (; s->repeat_prev; s = s->repeat_prev)
        ;
    for (; s; s = s->repeat_next) {
        dir = parent_dir(s->path);
        update_dir_has_rating_color(dir);
        g_free(dir);
    }
}


static void update_dir_has_rating_color ( gchar * dir ) {
    gchar * parent;
    gchar * str;
    
    str = g_hash_table_lookup(new_song_dirs_hash, dir);
    if (!str) 
        return;

    if (explore_dir_has_new_songs(dir))
        return;

    /* This directory used to be marked, but no longer is. Change its
       icon, remove info */
    explore_update_path_pm(dir, PM_DIR_CLOSED);
    
    g_hash_table_remove(new_song_dirs_hash, str);
    new_song_dirs = g_list_remove(new_song_dirs, str);
    g_free(str);
    
    /* Check the parent directory */
    parent = parent_dir (dir);
    update_dir_has_rating_color(parent);
    g_free(parent);
} 

