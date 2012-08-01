/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002-2003 Chuck Groom
 * Copyright (C) 2010 Craig Small 
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
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "gjay.h"
#include "ui.h"
#include "ui_private.h"
#include "ipc.h"
#include "i18n.h"

extern char *music_player_names[];

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
static void set_base_dir (GjayApp *gjay, char *filename);
/*static void toggle_no_filter ( GtkToggleButton *togglebutton,
                               gpointer user_data );*/
static void parent_set_callback (GtkWidget *widget,
                                 gpointer user_data);
static void click_daemon_quit( GtkToggleButton *togglebutton, gpointer user_data);
static void click_daemon_detach( GtkToggleButton *togglebutton, gpointer user_data);
static void click_daemon_ask( GtkToggleButton *togglebutton, gpointer user_data);
static void set_daemon_action ( GtkToggleButton *togglebutton, pref_daemon_action action, gpointer user_data );
/*static void tooltips_toggled ( GtkToggleButton *togglebutton,
                               gpointer user_data );*/
static void player_combo_box_changed(GtkComboBox *combo, gpointer user_data);
static void useratings_toggled ( GtkToggleButton *togglebutton,
                                 gpointer user_data );
static void max_working_set_callback (GtkWidget *widget,
                                 gpointer user_data);
static void prefs_update_song_dir ( GjayPrefs *prefs );

GtkWidget * make_prefs_window ( GjayApp *gjay )
{
  GtkWidget * window;
  GtkWidget * vbox1, * vbox2, * alignment, *dir_label, * button, *label;
  GtkWidget * radio1, * radio2, * radio3;
  GtkWidget * player_cbox;
  GtkWidget * hseparator, *max_working_set_entry, *table;
  char buffer[BUFFER_SIZE];
  int i;
  int table_row = 0;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), _("GJay Preferences"));
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);
    
    vbox1 = gtk_vbox_new (FALSE, 2);
    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 2);
    vbox2 = gtk_vbox_new (FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment), vbox2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label),_("<b>General</b>"));
    gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);

    table = gtk_table_new(2, 2, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox1), table, TRUE, TRUE, 2);
  
  if (music_player_names[1] != NULL) {
    /* There's always, at least, the "None" player.
       If there are also other plays beyond that,
       present the user with a menu.
     */

    /* Make room in the table for this row: */
    gtk_table_resize(GTK_TABLE(table), 3, 2);

    /* Player selection combo box */
    label = gtk_label_new(_("Music Player:"));
  
    player_cbox = gtk_combo_box_new_text();
    i=0;
    while (music_player_names[i] != NULL)
    {
      gtk_combo_box_append_text(GTK_COMBO_BOX(player_cbox), music_player_names[i]);
      i++;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(player_cbox), gjay->prefs->music_player);

    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(alignment), label);
    gtk_table_attach(GTK_TABLE(table), alignment,
        0, 1, table_row, table_row+1,
        (GTK_EXPAND|GTK_FILL),(GTK_EXPAND|GTK_FILL),6,0);
    table_row++;
    alignment = gtk_alignment_new(0, 0, 0, 0);

    gtk_container_add(GTK_CONTAINER(alignment), player_cbox);
    gtk_table_attach(GTK_TABLE(table), alignment, 1, 2, 0, 1,
        (GTK_EXPAND|GTK_FILL),(GTK_EXPAND|GTK_FILL),6,0);
    g_signal_connect (G_OBJECT (player_cbox), "changed",
                      G_CALLBACK (player_combo_box_changed), gjay);
  }

    /* Song Ratings checkbox */
    label = gtk_label_new(_("Use song ratings"));

    button = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                 gjay->prefs->use_ratings);
    g_signal_connect (G_OBJECT (button), "toggled",
                      G_CALLBACK (useratings_toggled), gjay);

    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(alignment), label);
    gtk_table_attach(GTK_TABLE(table), alignment,
        0, 1, table_row, table_row+1,
        (GTK_EXPAND|GTK_FILL),(GTK_EXPAND|GTK_FILL),6,0);
    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(alignment), button);
    gtk_table_attach(GTK_TABLE(table), alignment,
        1, 2, table_row, table_row+1,
        (GTK_EXPAND|GTK_FILL),(GTK_EXPAND|GTK_FILL),6,0);
    table_row++;

    /* Maximum working set box */
    label = gtk_label_new(_("Max working set"));

    max_working_set_entry = gtk_entry_new_with_max_length (6);
    snprintf(buffer, BUFFER_SIZE, "%d", gjay->prefs->max_working_set);
    gtk_entry_set_text(GTK_ENTRY(max_working_set_entry), buffer);
    gtk_widget_set_tooltip_text (max_working_set_entry,
                          _("On large collections, building playlists can take a long time. So gjay first picks this number of songs randomly, then continues the  selection from that subset. Increase this number if you're willing to tolerate longer waiting times for increased accuracy."));

    gtk_widget_set_size_request(max_working_set_entry, 60, -1);
    g_signal_connect (G_OBJECT (max_working_set_entry), "changed",
                      G_CALLBACK (max_working_set_callback), gjay->prefs);

    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(alignment), label);
    gtk_table_attach(GTK_TABLE(table), alignment,
        0, 1, table_row, table_row+1,
        (GTK_EXPAND|GTK_FILL),(GTK_EXPAND|GTK_FILL),6,0);
    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(alignment), max_working_set_entry);
    gtk_table_attach(GTK_TABLE(table), alignment,
        1, 2, table_row, table_row+1,
        (GTK_EXPAND|GTK_FILL),(GTK_EXPAND|GTK_FILL),6,0);
    table_row++;

    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator, TRUE, TRUE, 2);

    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 0);

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
        GTK_SIGNAL_FUNC (file_chooser_cb), gjay);
    gtk_box_pack_start(GTK_BOX(vbox2), button, TRUE, TRUE, 2);

    g_signal_connect (G_OBJECT (vbox1), "parent_set",
                      G_CALLBACK (parent_set_callback), gjay->prefs);

    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator, TRUE, TRUE, 2);

    alignment = gtk_alignment_new(0,0,0,0);

    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 0);

    vbox2 = gtk_vbox_new (FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment), vbox2);

    alignment = gtk_alignment_new(0, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 2);
    vbox2 = gtk_vbox_new (FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment), vbox2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label),_("<b>Song Analysis Daemon</b>"));
    gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 2);
    
    radio1 = gtk_radio_button_new_with_label (NULL, _("Stop daemon when you quit GJay"));
    radio2 = gtk_radio_button_new_with_label_from_widget (
        GTK_RADIO_BUTTON (radio1),
        _("Continue running daemon when you quit"));
    radio3 = gtk_radio_button_new_with_label_from_widget (
        GTK_RADIO_BUTTON (radio2),
        _("Ask each time"));
    if (gjay->prefs->daemon_action == PREF_DAEMON_QUIT) 
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio1), TRUE);
    else if (gjay->prefs->daemon_action == PREF_DAEMON_DETACH) 
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), TRUE);
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio3), TRUE);

    gtk_box_pack_start(GTK_BOX(vbox2), radio1, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), radio2, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), radio3, FALSE, TRUE, 0);
    g_signal_connect (G_OBJECT (radio1), "clicked",
                      G_CALLBACK (click_daemon_quit), 
                      gjay->prefs);
    g_signal_connect (G_OBJECT (radio2), "clicked",
                      G_CALLBACK (click_daemon_detach), 
                      gjay->prefs);
    g_signal_connect (G_OBJECT (radio3), "clicked",
                      G_CALLBACK (click_daemon_ask), 
                      gjay->prefs);

    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), hseparator, TRUE, TRUE, 2);

    alignment = gtk_alignment_new(0, 0, 0.3, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), alignment, TRUE, TRUE, 0);

    button = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_container_add(GTK_CONTAINER(alignment), button);
    g_signal_connect (G_OBJECT (button), 
                      "clicked",
                      G_CALLBACK (hide_prefs_window),
                      NULL);
  gtk_widget_show_all(vbox1);

  gtk_signal_connect (GTK_OBJECT (window), "delete_event",
    GTK_SIGNAL_FUNC (gtk_widget_hide), NULL);
  gtk_container_add (GTK_CONTAINER (window), vbox1);
  return window;
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
GtkWidget * make_no_root_view ( GjayApp *gjay ) {
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
        PM_BUTTON_DIR, gjay->gui->pixbufs);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
        G_CALLBACK (choose_base_dir), gjay);
    gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 5);
    return vbox1;
}

static void choose_base_dir (GtkWidget *button, gpointer user_data) {
  GtkWidget *dialog;
  GjayApp *gjay = (GjayApp*)user_data;

  dialog = gtk_file_chooser_dialog_new(_("Set base music directory"),
      GTK_WINDOW(gjay->gui->main_window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
      NULL);
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    set_base_dir(gjay, filename);
  }
  gtk_widget_destroy(dialog);
}

static void file_chooser_cb (GtkWidget *button, gpointer user_data) {
  char *path;
  path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(button));
  set_base_dir((GjayApp*)user_data, path);
  g_free(path);
}


static void set_base_dir  ( GjayApp *gjay, char *path ) {
    GtkWidget * dialog;
    struct stat stat_buf;


    if (stat (path, &stat_buf) != 0) {
      dialog = gtk_message_dialog_new(GTK_WINDOW(gjay->gui->main_window),
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
      dialog = gtk_message_dialog_new(GTK_WINDOW(gjay->gui->main_window),
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
        send_ipc(gjay->ipc->ui_fifo, CLEAR_ANALYSIS_QUEUE);
    }
    gjay->prefs->song_root_dir = g_strdup(path);     
	gjay->player->song_root_dir = gjay->prefs->song_root_dir;
    prefs_update_song_dir(gjay->prefs);
    gtk_idle_add(explore_view_set_root_idle, gjay);

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
  prefs_update_song_dir((GjayPrefs*)user_data);
}


static void prefs_update_song_dir ( GjayPrefs *prefs ) {
  if (prefs->song_root_dir)
    save_prefs(prefs);
}

static void
click_daemon_quit( GtkToggleButton *togglebutton, gpointer user_data)
{
  set_daemon_action(togglebutton, PREF_DAEMON_QUIT, user_data);
}

static void
click_daemon_detach( GtkToggleButton *togglebutton, gpointer user_data)
{
  set_daemon_action(togglebutton, PREF_DAEMON_DETACH, user_data);
}

static void
click_daemon_ask( GtkToggleButton *togglebutton, gpointer user_data)
{
  set_daemon_action(togglebutton, PREF_DAEMON_ASK, user_data);
}

static void
set_daemon_action ( GtkToggleButton *togglebutton, pref_daemon_action action, gpointer user_data )
{
  GjayPrefs *prefs=(GjayPrefs*)user_data;

  if (gtk_toggle_button_get_active(togglebutton)) {
    prefs->daemon_action = action;
    save_prefs(prefs);
  }
}

static void player_combo_box_changed(GtkComboBox *combo, gpointer user_data)
{
  gint new_player;
  GjayApp *gjay = (GjayApp*)user_data;

  new_player = gtk_combo_box_get_active(combo);
  if (new_player >= 0)
  {
	set_player_name(gjay->player, new_player);
    gjay->prefs->music_player = new_player;
    save_prefs(gjay->prefs);
  }
}

/* FIXME - the tooltips */
#if 0
static void tooltips_toggled ( GtkToggleButton *togglebutton,
                               gpointer user_data ) {
    prefs.hide_tips = !gtk_toggle_button_get_active(togglebutton);
    if (prefs.hide_tips) 
         gtk_tooltips_disable(tips);  
    else
         gtk_tooltips_enable(tips);
    save_prefs();
}
#endif


static void useratings_toggled ( GtkToggleButton *togglebutton,
                               gpointer user_data ) {
  GjayApp *gjay = (GjayApp*)user_data;

  gjay->prefs->use_ratings = gtk_toggle_button_get_active(togglebutton);
  set_selected_rating_visible ( gjay->prefs->use_ratings );
  set_playlist_rating_visible ( gjay->prefs->use_ratings );
  save_prefs(gjay->prefs);
}

static void max_working_set_callback ( GtkWidget *widget,
                               gpointer user_data ) {
  const gchar * text;
  unsigned int val;
  char *buffer;
  GjayPrefs *prefs=(GjayPrefs*)user_data;

  text = gtk_entry_get_text(GTK_ENTRY(widget));
  if ( (val = strtoul(text, NULL, 10)) == 0)
  {
    val = DEFAULT_MAX_WORKING_SET;
    buffer = g_strdup_printf("%u", val);
    gtk_entry_set_text(GTK_ENTRY(widget), buffer);
    g_free(buffer);
  }
  prefs->max_working_set = val;
  save_prefs(prefs);
}

