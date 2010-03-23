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
#include <errno.h>
#include <sys/stat.h>
#include "gjay.h"
#include "ui.h"
#include "ipc.h"
#include "i18n.h"


static char * welcome_str =
  "Welcome to GJay!\n\n" \
  "This program generates playlists spanning your music collection. "\
  "These playlists can match a particular mood, artist, or simply wander " \
  "your collection in a sensible way.\n\n" \
  "GJay automatically analyzes songs in the background, looking at " \
  "song frequency and beat. You categorize songs by color. Color means " \
  "whatever you want it to; most people interpret it as genre/mood.\n\n" \
  "To start using GJay, pick the base directory where you store " \
  "your mp3s, oggs, flacs and wavs. (You can change this in the prefs). "
;



static void choose_base_dir (GtkWidget *button, gpointer user_data);
static void file_chooser_cb  ( GtkWidget *button,
                               gpointer user_data );
static void set_base_dir (char *filename);
/*static void toggle_no_filter ( GtkToggleButton *togglebutton,
                               gpointer user_data );*/
static void parent_set_callback (GtkWidget *widget,
                                 gpointer user_data);
static void click_daemon_radio ( GtkToggleButton *togglebutton,
                                gpointer user_data );
static void tooltips_toggled ( GtkToggleButton *togglebutton,
                               gpointer user_data );
static void useratings_toggled ( GtkToggleButton *togglebutton,
                                 gpointer user_data );
static void max_working_set_callback (GtkWidget *widget,
                                 gpointer user_data);

GtkWidget * make_prefs_view ( void ) {
    GtkWidget * vbox1, * vbox2, * alignment, *dir_label, * button, *label;
    GtkWidget * radio1, * radio2, * radio3;
    GtkWidget * hseparator, * hbox1, *max_working_set_entry;
    char buffer[BUFFER_SIZE];
    
    vbox1 = gtk_vbox_new (FALSE, 2);

    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 2);
    vbox2 = gtk_vbox_new (FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment), vbox2);

    dir_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(dir_label),_("<b>Base Directory</b>"));
    gtk_box_pack_start(GTK_BOX(vbox2), dir_label, TRUE, TRUE, 2);


    alignment = gtk_alignment_new(0, 0, 1, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 2);
    vbox2 = gtk_vbox_new (FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment), vbox2);

    button = gtk_file_chooser_button_new(_("Set base music directory"),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(button),
        (gjay->prefs->song_root_dir?gjay->prefs->song_root_dir:g_get_home_dir()));
    gtk_signal_connect(GTK_OBJECT(button), "file-set",
        GTK_SIGNAL_FUNC (file_chooser_cb), gjay->main_window);
    gtk_box_pack_start(GTK_BOX(vbox2), button, TRUE, TRUE, 2);

    g_signal_connect (G_OBJECT (vbox1), "parent_set",
                      G_CALLBACK (parent_set_callback), NULL);

    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator, TRUE, TRUE, 2);

    alignment = gtk_alignment_new(0,0,0,0);

    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 0);

    vbox2 = gtk_vbox_new (FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment), vbox2);

    
    label = gtk_label_new(_("When you quit, song analysis should..."));
    radio1 = gtk_radio_button_new_with_label (NULL, _("Stop"));
    radio2 = gtk_radio_button_new_with_label_from_widget (
        GTK_RADIO_BUTTON (radio1),
        _("Continue in background"));
    radio3 = gtk_radio_button_new_with_label_from_widget (
        GTK_RADIO_BUTTON (radio2),
        _("Ask"));
    if (gjay->prefs->daemon_action == PREF_DAEMON_QUIT) 
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio1), TRUE);
    else if (gjay->prefs->daemon_action == PREF_DAEMON_DETACH) 
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), TRUE);
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio3), TRUE);

    gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), radio1, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), radio2, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), radio3, FALSE, TRUE, 0);
    g_signal_connect (G_OBJECT (radio1), "clicked",
                      G_CALLBACK (click_daemon_radio), 
                      (gpointer)PREF_DAEMON_QUIT);
    g_signal_connect (G_OBJECT (radio2), "clicked",
                      G_CALLBACK (click_daemon_radio), 
                      (gpointer)PREF_DAEMON_DETACH);
    g_signal_connect (G_OBJECT (radio3), "clicked",
                      G_CALLBACK (click_daemon_radio), 
                      (gpointer)PREF_DAEMON_ASK);

    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator, TRUE, TRUE, 2);

    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 0);

    /*button = gtk_check_button_new_with_label("Show popup tips");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                 !(gjay->prefs->hide_tips));
    g_signal_connect (G_OBJECT (button), "toggled",
                      G_CALLBACK (tooltips_toggled), NULL);
    gtk_container_add(GTK_CONTAINER(alignment), button);
*/

    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator, TRUE, TRUE, 2);

    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 0);

    button = gtk_check_button_new_with_label(_("Use song ratings"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                 gjay->prefs->use_ratings);
    g_signal_connect (G_OBJECT (button), "toggled",
                      G_CALLBACK (useratings_toggled), NULL);
    gtk_container_add(GTK_CONTAINER(alignment), button);


    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator, TRUE, TRUE, 2);

    label = gtk_label_new(_("Max working set"));
    max_working_set_entry = gtk_entry_new_with_max_length (6);
    snprintf(buffer, BUFFER_SIZE, "%d", gjay->prefs->max_working_set);
    gtk_entry_set_text(GTK_ENTRY(max_working_set_entry), buffer);
    gtk_widget_set_tooltip_text (max_working_set_entry,
                          _("On large collections, building playlists can take a long time. So gjay first picks this number of songs randomly, then continues the  selection from that subset. Increase this number if you're willing to tolerate longer waiting times for increased accuracy."));

    gtk_widget_set_size_request(max_working_set_entry, 60, -1);
    g_signal_connect (G_OBJECT (max_working_set_entry), "changed",
                      G_CALLBACK (max_working_set_callback), NULL);
    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox1), max_working_set_entry, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, TRUE, TRUE, 0);

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
    GtkWidget * vbox1, * vbox2, * alignment, * label, *button;
    
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
    button = new_button_label_pixbuf(_("Choose base music directory"),
        PM_BUTTON_DIR);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
        G_CALLBACK (choose_base_dir), NULL);
    gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 5);
    return vbox1;
}

static void choose_base_dir (GtkWidget *button, gpointer user_data) {
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new(_("Set base music directory"),
      GTK_WINDOW(gjay->main_window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
      NULL);
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    set_base_dir(filename);
  }
  gtk_widget_destroy(dialog);
}

static void file_chooser_cb (GtkWidget *button, void *user_data) {
  char *path;
  path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));
  set_base_dir(path);
  g_free(path);
}


static void set_base_dir  (  char *path ) {
    GtkWidget * dialog;
    struct stat stat_buf;


    if (stat (path, &stat_buf) != 0) {
      dialog = gtk_message_dialog_new(GTK_WINDOW(gjay->main_window),
          GTK_DIALOG_DESTROY_WITH_PARENT,
          GTK_MESSAGE_ERROR,
          GTK_BUTTONS_CLOSE,
          _("Error getting status of directory '%s': %s"),
          path, g_strerror(errno));
      gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
      return;
    }
    if (!S_ISDIR(stat_buf.st_mode)) {
      dialog = gtk_message_dialog_new(GTK_WINDOW(gjay->main_window),
          GTK_DIALOG_DESTROY_WITH_PARENT,
          GTK_MESSAGE_ERROR,
          GTK_BUTTONS_CLOSE,
          _("Path '%s' is not a directory."),
          path);
      gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
      return;
    }
    if (gjay->prefs->song_root_dir) {
        g_free(gjay->prefs->song_root_dir);
        /* Clear out old stuff the daemon may have been thinking about
         * doing */
        send_ipc(ui_pipe_fd, CLEAR_ANALYSIS_QUEUE);
    }
    gjay->prefs->song_root_dir = g_strdup(path);     
    prefs_update_song_dir();
    gtk_idle_add(explore_view_set_root_idle, NULL);

    set_add_files_progress(_("Scanning tree..."), 0);
    set_analysis_progress_visible(FALSE);
    set_add_files_progress_visible(TRUE);
}


/*static void toggle_no_filter (GtkToggleButton *togglebutton,
                              gpointer user_data) {
    gjay->prefs->extension_filter = gtk_toggle_button_get_active(togglebutton);
}*/


static void parent_set_callback (GtkWidget *widget,
                                 gpointer user_data) {
  prefs_update_song_dir();
}


void prefs_update_song_dir ( void ) {
    char buffer[BUFFER_SIZE];
    if (gjay->prefs->song_root_dir) {
        snprintf(buffer, BUFFER_SIZE,
                 _("Base directory is '%s'"),  gjay->prefs->song_root_dir);   
        save_prefs();
    } else {
        snprintf(buffer, BUFFER_SIZE, _("No base directory set"));
    }
}

static void
click_daemon_radio ( GtkToggleButton *togglebutton, gpointer user_data )
{

  if (gtk_toggle_button_get_active(togglebutton)) {
        gjay->prefs->daemon_action = (pref_daemon_action)user_data;
        save_prefs();
    }
}

static void tooltips_toggled ( GtkToggleButton *togglebutton,
                               gpointer user_data ) {
    /* FIXME
    prefs.hide_tips = !gtk_toggle_button_get_active(togglebutton);
    if (prefs.hide_tips) 
         gtk_tooltips_disable(tips);  
    else
         gtk_tooltips_enable(tips);
    save_prefs();
         */
}


static void useratings_toggled ( GtkToggleButton *togglebutton,
                               gpointer user_data ) {

  gjay->prefs->use_ratings = gtk_toggle_button_get_active(togglebutton);
  set_selected_rating_visible ( gjay->prefs->use_ratings );
  set_playlist_rating_visible ( gjay->prefs->use_ratings );
  save_prefs();
}

static void max_working_set_callback ( GtkWidget *widget,
                               gpointer user_data ) {
  const gchar * text;
  unsigned int val;
  char *buffer;

  text = gtk_entry_get_text(GTK_ENTRY(widget));
  if ( (val = strtoul(text, NULL, 10)) == 0)
  {
    val = DEFAULT_MAX_WORKING_SET;
    buffer = g_strdup_printf("%u", val);
    gtk_entry_set_text(GTK_ENTRY(widget), buffer);
    g_free(buffer);
  }
  gjay->prefs->max_working_set = val;
  save_prefs();
}

