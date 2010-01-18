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

#include "gjay.h"
#include "gjay_audacious.h"
#include "ui.h"

static void menuitem_currentsong (void);
static void menuitem_quit (void);

static const GtkActionEntry entries[] = {
  { "FileMenu", NULL, "_File" },
  { "EditMenu", NULL, "_Edit" },
  { "HelpMenu", NULL, "_Help" },
  { "CurrentSong", GTK_STOCK_JUMP_TO, "_Go to current song", "<control>G", "Go to current song", menuitem_currentsong},
  { "Quit", GTK_STOCK_QUIT, "_Quit", "<control>Q", "Quit the program", menuitem_quit},
  { "Preferences", GTK_STOCK_PREFERENCES, "_Preferences", NULL, "Edit Preferences", show_prefs_window },
  { "About", GTK_STOCK_ABOUT, "_About", NULL, "About Gjay", show_about_window}
};

static const char *ui_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='FileMenu'>"
"      <menuitem action='CurrentSong'/>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='EditMenu'>"
"      <menuitem action='Preferences'/>"
"    </menu>"
"    <menu action='HelpMenu'>"
"      <menuitem action='About'/>"
"    </menu>"
"  </menubar>"
"</ui>";

GtkWidget * make_menubar ( void ) {
  GtkWidget *menubar;
  GtkActionGroup *action_group;
  GtkUIManager *ui_manager;
  GtkAccelGroup *accel_group;
  GError *error;

  action_group = gtk_action_group_new("MenuActions");
  gtk_action_group_add_actions(action_group, entries, G_N_ELEMENTS(entries), gjay->main_window);
  ui_manager = gtk_ui_manager_new();
  gtk_ui_manager_insert_action_group(ui_manager, action_group, 0);
  accel_group = gtk_ui_manager_get_accel_group(ui_manager);
  gtk_window_add_accel_group(GTK_WINDOW(gjay->main_window), accel_group);
  
  error = NULL;
  if (!gtk_ui_manager_add_ui_from_string(ui_manager, ui_description, -1, &error))
  {
    g_message ("building menus failed: %s", error->message);
    g_error_free(error);
    exit(1);
  }

  menubar = gtk_ui_manager_get_widget(ui_manager, "/MainMenu");
  return menubar;
}


void menuitem_currentsong (void) {
  song * s;
  gchar * msg; 
  GtkWidget * dialog;

  s = get_current_audacious_song();
  if (s) {
    explore_select_song(s);
  } else {
    if (audacious_is_running()) {
      msg = "Sorry, GJay doesn't appear to know that song";
    } else {
        msg = "Sorry, unable to connect to Audacious.\nIs Audacious running?";
    }
    dialog = gtk_message_dialog_new(
        GTK_WINDOW(gjay->main_window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_CLOSE,
        msg);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }
}

static void menuitem_quit (void) {
    quit_app(NULL, NULL, NULL);
}
