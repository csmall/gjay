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
 *
 * Categorize.c -- display a dialog which allows the user to give a song 
 * a color, a rating, and title/artist.
 */

#include <string.h>
#include <math.h>
#include "gjay.h"
#include "images/play.h"


typedef struct {
    /* The drawing area for the color wheel and fore/back rects */
    GtkWidget * title_entry;
    GtkWidget * artist_entry;

    /* The frame containing the song/song list */
    GtkWidget * song_frame;

    /* The frame containing rating info. Only applies to single songs. */
    GtkWidget * rating_frame;

    /* Current color */
    HB current; 

    /* Current rating */
    gdouble rating;
    
    GList * data;
    GFunc done;
} categorize_dialog;



static void categorize_ok ( GtkWidget *widget,
                            gpointer user_data);
static void unrealize_categorize_dialog ( GtkWidget *widget,
                                          gpointer user_data);
static void select_all_by_artist ( GtkWidget *widget,
                                   gpointer user_data);
static void create_categorize_song_list (categorize_dialog * c_dialog);
static void play_song_action ( GtkWidget *widget,
                               gpointer user_data);


GtkWidget * make_categorize_dialog ( GList * file_data,
                                     GFunc done_func ) {
    GtkWidget *window, *button, *hbox, *vbox;
    GtkWidget *subbox, *subhbox, *widget, * frame;
    GdkPixmap * pixmap;
    GdkBitmap * mask;

    categorize_dialog * c_dialog;
    gdouble num_files;
    song * s;
    char buffer[BUFFER_SIZE];

    c_dialog = g_malloc(sizeof(categorize_dialog));
    memset (c_dialog, 0x00, sizeof(categorize_dialog));
    assert(file_data);
    c_dialog->data = file_data;
    s = SONG(file_data);
    c_dialog->done = done_func;
    
    num_files = g_list_length(file_data);
    c_dialog->current = hsv_to_hb(average_color(file_data));
    c_dialog->rating = average_rating(file_data);
    
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Song Features");
    gtk_signal_connect (GTK_OBJECT (window), "unrealize",
                        (GtkSignalFunc) unrealize_categorize_dialog, 
                        c_dialog);
    gtk_signal_connect (GTK_OBJECT (window), "unrealize",
                        (GtkSignalFunc) dialog_unrealize, 
                        NULL);
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",  
                        (GtkSignalFunc) gtk_widget_destroy, 
                        (gpointer) window);

    gtk_container_set_border_width (GTK_CONTAINER (window), 2);
    gtk_widget_realize(window);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    hbox = gtk_hbox_new(TRUE, 2);
    gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);
    gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);

    button = gtk_button_new_with_label ("OK");
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);  
    gtk_widget_grab_default (button);     
    gtk_signal_connect (GTK_OBJECT (button), 
                        "clicked",
			(GtkSignalFunc) categorize_ok, 
                        (gpointer) c_dialog);
    gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                               (GtkSignalFunc) gtk_widget_destroy, 
                               (gpointer) window);

    button = gtk_button_new_with_label ("Cancel");
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
    gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                               (GtkSignalFunc) gtk_widget_destroy, 
                               (gpointer) window);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 5);


    vbox = gtk_vbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    c_dialog->song_frame = gtk_frame_new("Song");
    gtk_box_pack_start(GTK_BOX(vbox), c_dialog->song_frame, TRUE, TRUE, 3) ;
    
    if (num_files > 1) {
        create_categorize_song_list(c_dialog);
    } else {
        s = SONG(file_data);
        subbox = gtk_vbox_new (FALSE, 2); 
        gtk_container_set_border_width(GTK_CONTAINER (subbox), 3);
        gtk_container_add (GTK_CONTAINER (c_dialog->song_frame), subbox);
        gtk_container_set_border_width(GTK_CONTAINER (c_dialog->song_frame), 2);
        snprintf(buffer, BUFFER_SIZE, "File: %s\n", s->fname);
        widget = gtk_label_new(buffer);
        gtk_box_pack_start(GTK_BOX(subbox), widget, FALSE, FALSE, 5);

        subhbox = gtk_hbox_new(FALSE, 2);
        gtk_box_pack_start(GTK_BOX(subbox), subhbox, FALSE, FALSE, 0);
        widget = gtk_label_new("Title:");
        gtk_box_pack_start(GTK_BOX(subhbox), widget, FALSE, FALSE, 0);
        c_dialog->title_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(subhbox), c_dialog->title_entry, FALSE, FALSE, 0);
        gtk_entry_set_text(GTK_ENTRY(c_dialog->title_entry), s->title);

        subhbox = gtk_hbox_new(FALSE, 2);
        gtk_box_pack_start(GTK_BOX(subbox), subhbox, FALSE, FALSE, 0);
        widget = gtk_label_new("Artist:");
        gtk_box_pack_start(GTK_BOX(subhbox), widget, FALSE, FALSE, 0);
        c_dialog->artist_entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(subhbox), c_dialog->artist_entry, FALSE, FALSE, 0);
        gtk_entry_set_text(GTK_ENTRY(c_dialog->artist_entry), s->artist);

        subhbox = gtk_hbox_new(FALSE, 2);
        gtk_box_pack_start(GTK_BOX(subbox), subhbox, FALSE, FALSE, 0);

        button = gtk_button_new();
        gtk_widget_set_usize(button, 30, -1);
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                            (GtkSignalFunc) play_song_action,
                            (gpointer) c_dialog);
        pixmap = gdk_pixmap_create_from_xpm_d (app_window->window,
                                               &mask,
                                               NULL,
                                               (gchar **) &play_xpm);
        widget = gtk_pixmap_new (pixmap, mask);
        gtk_container_add (GTK_CONTAINER (button), widget);
        gtk_box_pack_start(GTK_BOX(subhbox), button, TRUE, FALSE, 2);
        
        button = gtk_button_new_with_label("Select all by artist");
        gtk_box_pack_start(GTK_BOX(subhbox), button, TRUE, FALSE, 2);
        gtk_signal_connect (GTK_OBJECT (button), 
                            "clicked",
                            (GtkSignalFunc) select_all_by_artist,
                            (gpointer) c_dialog);


        c_dialog->rating_frame = gtk_frame_new("Rating"); 
        gtk_box_pack_start(GTK_BOX(vbox), c_dialog->rating_frame, FALSE, FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER (c_dialog->rating_frame), 2);

        
        widget = make_star_rating(window, &c_dialog->rating);
         gtk_container_set_border_width(GTK_CONTAINER (widget), 3);
        gtk_container_add (GTK_CONTAINER (c_dialog->rating_frame), widget);
    }

    vbox = gtk_vbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
    gtk_box_pack_end(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    frame = gtk_frame_new("Song Color");
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
    widget = make_color_wheel(&c_dialog->current);
    gtk_container_add (GTK_CONTAINER (frame), widget);
    
    dialog_list = g_list_append(dialog_list, window);
    return window;
}


RGB song_get_rgb (song * s) {
    return hsv_to_rgb(hb_to_hsv(s->color));
}


guint32 song_get_rgb_hex (song * s) {
    return rgb_to_hex(song_get_rgb(s));
}
	

/* OK is the same as cancel, except it first records changes to song
   data */
static void categorize_ok ( GtkWidget *widget,
                            gpointer user_data) { 
    gint num_files;
    GList * current;
    song * s;
    categorize_dialog * c_dialog = (categorize_dialog *) user_data;

    num_files = g_list_length(c_dialog->data);

    if (num_files == 1) {
        s = SONG(c_dialog->data);
        g_free(s->title);
        g_free(s->artist);
        s->title = g_strdup(gtk_entry_get_text(GTK_ENTRY(c_dialog->title_entry)));
        s->artist = g_strdup(gtk_entry_get_text(GTK_ENTRY(c_dialog->artist_entry)));
        s->color = c_dialog->current;
        s->rating = c_dialog->rating;
        if (s->flags & COLOR_UNK)
            s->flags -= COLOR_UNK;
        if (s->flags & RATING_UNK)
            s->flags -= RATING_UNK;
        
    } else {
        for (current = c_dialog->data; 
             current; 
             current = g_list_next(current)) {
            s = SONG(current);
            s->color = c_dialog->current;
            if (s->flags & COLOR_UNK)
                s->flags -= COLOR_UNK;
        }
    }
    if (c_dialog->done) 
        (c_dialog->done) (c_dialog->data, NULL);
}


static void unrealize_categorize_dialog (GtkWidget * widget,
                                         gpointer user_data) {
    categorize_dialog * c_dialog = (categorize_dialog *) user_data;
    g_list_free(c_dialog->data);
}


/* Try to select all songs by a particular artist. */
static void select_all_by_artist ( GtkWidget *widget,
                                   gpointer user_data) { 
    GList * list;
    gchar * artist, * title;
    song * s;
    
    categorize_dialog * c_dialog = (categorize_dialog *) user_data;
    s = SONG(c_dialog->data);
    artist = gtk_entry_get_text(GTK_ENTRY(c_dialog->artist_entry));
    title = gtk_entry_get_text(GTK_ENTRY(c_dialog->title_entry));
    if ((strlen(artist) == 0) || (strcmp(artist, "?") == 0)) {
        display_message("Sorry, there needs to be\na full artist name");
        return;
    }
    g_free(s->artist);
    g_free(s->title);
    s->artist = g_strdup(artist);
    s->title = g_strdup(title);
    s->rating = c_dialog->rating;
    list = songs_by_artist(songs, artist);
    if (g_list_length(list) == 1) {
        display_message("This is the only song by the artist");
        g_list_free(list);
    } else {
        g_list_free(c_dialog->data);
        c_dialog->data = list;
        gtk_widget_destroy(c_dialog->rating_frame);
        create_categorize_song_list(c_dialog);
    }
}


static void create_categorize_song_list (categorize_dialog * c_dialog) {
    GList * current;
    GtkWidget * s_win, * clist, * hbox, * vbox, * widget, * button;
    gchar * list_data[2];
    song * s;
    gint width, height;
    GdkPixmap * pixmap;
    GdkBitmap * mask;

    width = MAX(c_dialog->song_frame->allocation.width, 
                CATEGORIZE_DIAMETER * 0.8);
    height = c_dialog->song_frame->allocation.height;

    gtk_container_foreach(GTK_CONTAINER(c_dialog->song_frame),
                          (GtkCallback) gtk_widget_destroy, 
                          NULL);
    
    gtk_frame_set_label (GTK_FRAME(c_dialog->song_frame), "Songs");
    
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
    gtk_container_add (GTK_CONTAINER (c_dialog->song_frame), vbox);
     
    s_win = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (s_win),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), s_win, TRUE, TRUE, 2);
    clist = gtk_clist_new(2);
    gtk_clist_set_column_title(GTK_CLIST(clist), 0, "Title");
    gtk_clist_set_column_title(GTK_CLIST(clist), 1, "Artist");
    gtk_clist_set_column_width(GTK_CLIST(clist), 0, 0.66 * width);
    gtk_clist_column_titles_show (GTK_CLIST(clist));
    gtk_container_add(GTK_CONTAINER(s_win), clist);
    
    for (current = c_dialog->data; current; current = g_list_next(current)) {
        s = SONG(current);
        list_data[0] = s->title;
        list_data[1] = s->artist;
        gtk_clist_append( GTK_CLIST(clist), list_data);
    } 
    gtk_widget_set_usize(c_dialog->song_frame, width, height);

    hbox = gtk_hbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    button = gtk_button_new();
    gtk_widget_set_usize(button, 30, -1);
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        (GtkSignalFunc) play_song_action,
                        (gpointer) c_dialog);
    pixmap = gdk_pixmap_create_from_xpm_d (app_window->window,
                                           &mask,
                                           NULL,
                                           (gchar **) &play_xpm);
    widget = gtk_pixmap_new (pixmap, mask);
    gtk_container_add (GTK_CONTAINER (button), widget);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 2);    

    gtk_widget_show_all(c_dialog->song_frame);
}


static void play_song_action ( GtkWidget *widget,
                               gpointer user_data) {
    play_songs(((categorize_dialog *) user_data)->data);
}
