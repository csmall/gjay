/**
 * GJay, copyright (c) 2002-2003 Chuck Groom
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
#include <string.h>
#include <sys/stat.h>
#include "gjay.h"
#include "ui.h"
#include "ipc.h"


static char * welcome_str =
  "Welcome to GJay!\n\n" \
  "This program generates playlists spanning your music collection. "\
  "These playlists can match a particular mood, artist, or simply wander " \
  "your collection in a sensible way.\n\n" \
  "GJay automatically analyzes songs in the background, looking at " \
  "song frequency and beat. You categorize songs by color. Color means " \
  "whatever you want it to; most people interpret it as genre/mood.\n\n" \
  "To start using GJay, pick the base directory where you store " \
  "your mp3s, oggs, and wavs. (You can change this in the prefs). "
;


static GtkWidget * prefs_label;

static void choose_base_dir  ( GtkButton *button,
                               gpointer user_data );
static void set_base_dir     ( GtkButton *button,
                               gpointer user_data );
static void toggle_no_filter ( GtkToggleButton *togglebutton,
                               gpointer user_data );
static void parent_set_callback (GtkWidget *widget,
                                 gpointer user_data);
static void radio_toggled ( GtkToggleButton *togglebutton,
                            gpointer user_data );
static void tooltips_toggled ( GtkToggleButton *togglebutton,
                               gpointer user_data );
static void useratings_toggled ( GtkToggleButton *togglebutton,
                                 gpointer user_data );

GtkWidget * make_prefs_view ( void ) {
    GtkWidget * vbox1, * vbox2, * alignment, * button, *label;
    GtkWidget * radio1, * radio2, * radio3;
    GtkWidget * hseparator;
    
    vbox1 = gtk_vbox_new (FALSE, 2);
    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 2);
    
    vbox2 = gtk_vbox_new (FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment), vbox2);

    prefs_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox2), prefs_label, TRUE, TRUE, 2);
    
    alignment = gtk_alignment_new(0, 0, 1, 0);
    button = new_button_label_pixbuf("Set base music directory",
                                     PM_BUTTON_DIR);    
    g_signal_connect (G_OBJECT (button),
                      "clicked",
                      G_CALLBACK (choose_base_dir),
                      NULL);
    gtk_container_add(GTK_CONTAINER(alignment), button);
    gtk_box_pack_start(GTK_BOX(vbox2), alignment, TRUE, TRUE, 2);
    
    g_signal_connect (G_OBJECT (vbox1), "parent_set",
                      G_CALLBACK (parent_set_callback), NULL);

    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator, TRUE, TRUE, 2);

    alignment = gtk_alignment_new(0,0,0,0);

    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 0);

    vbox2 = gtk_vbox_new (FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment), vbox2);

    
    label = gtk_label_new("When you quit, song analysis should...");
    radio1 = gtk_radio_button_new_with_label (NULL, "Stop");
    radio2 = gtk_radio_button_new_with_label_from_widget (
        GTK_RADIO_BUTTON (radio1),
        "Continue in background");
    radio3 = gtk_radio_button_new_with_label_from_widget (
        GTK_RADIO_BUTTON (radio2),
        "Ask");
    if (prefs.daemon_action == PREF_DAEMON_QUIT) 
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio1), TRUE);
    else if (prefs.daemon_action == PREF_DAEMON_DETACH) 
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), TRUE);
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio3), TRUE);

    gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), radio1, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), radio2, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), radio3, FALSE, TRUE, 0);
    g_signal_connect (G_OBJECT (radio1), "toggled",
                      G_CALLBACK (radio_toggled), 
                      (void *) PREF_DAEMON_QUIT);
    g_signal_connect (G_OBJECT (radio2), "toggled",
                      G_CALLBACK (radio_toggled), 
                      (void *) PREF_DAEMON_DETACH);
    g_signal_connect (G_OBJECT (radio3), "toggled",
                      G_CALLBACK (radio_toggled), 
                      (void *) PREF_DAEMON_ASK);

    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator, TRUE, TRUE, 2);

    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 0);

    button = gtk_check_button_new_with_label("Show popup tips");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                 !prefs.hide_tips);
    g_signal_connect (G_OBJECT (button), "toggled",
                      G_CALLBACK (tooltips_toggled), NULL);
    gtk_container_add(GTK_CONTAINER(alignment), button);


    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator, TRUE, TRUE, 2);

    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 0);

    button = gtk_check_button_new_with_label("Use song ratings");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                 prefs.use_ratings);
    g_signal_connect (G_OBJECT (button), "toggled",
                      G_CALLBACK (useratings_toggled), NULL);
    gtk_container_add(GTK_CONTAINER(alignment), button);


    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator, TRUE, TRUE, 2);

    alignment = gtk_alignment_new(1, 0, 0.3, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 0);

    button = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_container_add(GTK_CONTAINER(alignment), button);
    g_signal_connect (G_OBJECT (button), 
                      "clicked",
                      G_CALLBACK (hide_prefs_window),
                      NULL);
    return vbox1;
}


/**
 * No root view
 *
 * If the user has not specificed a base music directory, we replace
 * the explore pane with a friendly welcome guide encouraging the user
 * to choose a base directory. Although this is shown in the explore
 * pane, it connects with the prefs system which is why we include it 
 * here.
 */
GtkWidget * make_no_root_view ( void ) {
    GtkWidget * vbox1, * vbox2, * alignment, * label, * button;
    
    vbox1 = gtk_vbox_new (FALSE, 2);

    label = gtk_label_new(welcome_str);
    alignment = gtk_alignment_new(0.5, 0.1, .8, .7);
    gtk_container_add(GTK_CONTAINER(alignment), label);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 2);


    alignment = gtk_alignment_new (0.5, 0.3, 0.05, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 2);
    
    vbox2 = gtk_vbox_new (FALSE, 2);  
    gtk_container_add(GTK_CONTAINER(alignment), vbox2);

    button = new_button_label_pixbuf("Choose base music directory",
                                     PM_BUTTON_DIR);    
    g_signal_connect (G_OBJECT (button),
                      "clicked",
                      G_CALLBACK (choose_base_dir),
                      NULL);

    gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 5);
  
    return vbox1;
}


static void choose_base_dir  ( GtkButton *button,
                               gpointer user_data ) {
    GtkWidget * file_selector, * button_filter;
    file_selector = gtk_file_selection_new("Select the base music directory");
    
    gtk_tree_selection_set_mode (gtk_tree_view_get_selection(
        GTK_TREE_VIEW((GTK_FILE_SELECTION(file_selector))->dir_list)),
        GTK_SELECTION_BROWSE);
    gtk_tree_selection_set_mode (gtk_tree_view_get_selection(
        GTK_TREE_VIEW((GTK_FILE_SELECTION(file_selector))->file_list)),
        GTK_SELECTION_NONE);
    gtk_signal_connect (
        GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->ok_button),
        "clicked",
        GTK_SIGNAL_FUNC (set_base_dir), 
        (gpointer) GTK_OBJECT(file_selector)); 
    gtk_signal_connect_object (
        GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->ok_button),
        "clicked", 
        GTK_SIGNAL_FUNC (gtk_widget_destroy),
        (gpointer) file_selector);
    gtk_signal_connect_object (
        GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->cancel_button),
        "clicked", 
        GTK_SIGNAL_FUNC (gtk_widget_destroy),
        (gpointer) file_selector);
    gtk_file_selection_hide_fileop_buttons(
        GTK_FILE_SELECTION(file_selector));
    
    button_filter = gtk_check_button_new_with_label(
        "Filter only .mp3, .ogg, or .wav extension");
    g_signal_connect (
        G_OBJECT (button_filter),
        "toggled",
        G_CALLBACK (toggle_no_filter),
        NULL);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_filter),
                                 prefs.extension_filter);
    gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(file_selector)->main_vbox), 
                       button_filter, FALSE, FALSE, 5);
    
    gtk_widget_show (file_selector);
    gtk_widget_show (button_filter);
    gtk_widget_hide (GTK_FILE_SELECTION(file_selector)->selection_entry);
    gtk_widget_hide (GTK_FILE_SELECTION(file_selector)->selection_text);
}


static void set_base_dir ( GtkButton *button,
                           gpointer user_data ) {
    gchar * base_dir;
    struct stat stat_buf;
    GtkFileSelection * file_sel;
    GtkTreeSelection * select;
    GtkTreeModel * model;
    GtkTreeIter iter;    
    char path[BUFFER_SIZE];
    bzero(path, BUFFER_SIZE);

    file_sel = GTK_FILE_SELECTION(user_data);
    base_dir = (gchar *) gtk_file_selection_get_filename (file_sel);
    strncpy(path, base_dir, BUFFER_SIZE);

    select = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_sel->dir_list));
    
    if (gtk_tree_selection_get_selected(select, &model, &iter)) {
        gchar * dir_data = NULL;
        gtk_tree_model_get(model, &iter, 0, &dir_data, -1);
        if ((strcmp(dir_data, "./") != 0) && (strcmp(dir_data, "../") != 0)) {
            snprintf(path, BUFFER_SIZE, "%s%s", base_dir, dir_data);
        }
        g_free(dir_data);
    }
    
    if (stat (path, &stat_buf)) {
        fprintf(stderr, "Could not stat %s\n", path); 
        return;
    }
    if (!S_ISDIR(stat_buf.st_mode)) {
        fprintf(stderr, "%s is not a directory\n", path); 
        return;
    }
    if (prefs.song_root_dir) {
        g_free(prefs.song_root_dir);
        /* Clear out old stuff the daemon may have been thinking about
         * doing */
        send_ipc(ui_pipe_fd, CLEAR_ANALYSIS_QUEUE);
    }
    prefs.song_root_dir = g_strdup(path);     
    prefs_update_song_dir();
    gtk_idle_add(explore_view_set_root_idle, prefs.song_root_dir);

    set_add_files_progress("Scanning tree...", 0);
    set_analysis_progress_visible(FALSE);
    set_add_files_progress_visible(TRUE);
}


static void toggle_no_filter (GtkToggleButton *togglebutton,
                              gpointer user_data) {
    prefs.extension_filter = gtk_toggle_button_get_active(togglebutton);
}


static void parent_set_callback (GtkWidget *widget,
                                 gpointer user_data) {
    prefs_update_song_dir();
}


void prefs_update_song_dir ( void ) {
    char buffer[BUFFER_SIZE];
    if (prefs.song_root_dir) {
        snprintf(buffer, BUFFER_SIZE,
                 "Base directory is '%s'",  prefs.song_root_dir);   
        save_prefs();
    } else {
        snprintf(buffer, BUFFER_SIZE, "No base directory set");
    }
    gtk_label_set_text(GTK_LABEL(prefs_label), buffer);
}


static void radio_toggled ( GtkToggleButton *togglebutton,
                            gpointer user_data ) {
    gint state = (gint) user_data;
    if (gtk_toggle_button_get_active(togglebutton)) {
        prefs.daemon_action = state;
        save_prefs();
    }
}


static void tooltips_toggled ( GtkToggleButton *togglebutton,
                               gpointer user_data ) {
    prefs.hide_tips = !gtk_toggle_button_get_active(togglebutton);
    if (prefs.hide_tips) 
        gtk_tooltips_disable(tips);  
    else
        gtk_tooltips_enable(tips);
    save_prefs();
}


static void useratings_toggled ( GtkToggleButton *togglebutton,
                               gpointer user_data ) {
    prefs.use_ratings = gtk_toggle_button_get_active(togglebutton);
    set_selected_rating_visible ( prefs.use_ratings );
    set_playlist_rating_visible ( prefs.use_ratings );
    save_prefs();
}


