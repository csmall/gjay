/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002 Chuck Groom
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

#include "gjay.h"
#include "ui.h"
#include "ui_private.h"
#include "i18n.h"

void menuitem_currentsong ( GtkAction *action, gpointer user_data);
void menuitem_quit ( GtkAction *action, gpointer user_data);
void menuitem_prefs ( GtkAction *action, gpointer user_data);

static const GtkActionEntry entries[] = {
  { "FileMenu", NULL, "_File" },
  { "EditMenu", NULL, "_Edit" },
  { "HelpMenu", NULL, "_Help" },
  { "CurrentSong", GTK_STOCK_JUMP_TO, "_Go to current song", "<control>G", "Go to current song", G_CALLBACK(menuitem_currentsong)},
  { "Quit", GTK_STOCK_QUIT, "_Quit", "<control>Q", "Quit the program", G_CALLBACK(menuitem_quit)},
  { "Preferences", GTK_STOCK_PREFERENCES, "_Preferences", NULL, "Edit Preferences", G_CALLBACK(menuitem_prefs) },
  { "About", GTK_STOCK_ABOUT, "_About", NULL, "About Gjay", G_CALLBACK(show_about_window)}
};
static guint n_entries = G_N_ELEMENTS(entries);

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

GtkWidget * make_menubar ( GjayApp *gjay ) {
  GtkWidget *menubar;
  GtkActionGroup *action_group;
  GtkUIManager *ui_manager;
  GError *error;

  action_group = gtk_action_group_new("MenuActions");
  gtk_action_group_set_translation_domain(action_group, "blah");
  gtk_action_group_add_actions(action_group, entries, n_entries, gjay);
  ui_manager = gtk_ui_manager_new();
  gtk_ui_manager_insert_action_group(ui_manager, action_group, 0);
  gtk_window_add_accel_group(GTK_WINDOW(gjay->gui->main_window),
      gtk_ui_manager_get_accel_group(ui_manager));
  
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


void menuitem_currentsong (GtkAction *action, gpointer user_data) {
  GjaySong * s;
  gchar * msg; 
  GjayApp *gjay = (GjayApp*)user_data;

  if (gjay->player->get_current_song==NULL)
  {
    if (gjay->prefs->music_player == PLAYER_NONE)
      gjay_error_dialog(gjay->gui->main_window, _("Cannot get current song with no player selected."));
    else {
      msg = g_strdup_printf(_("Don't know how to get current song for music player %s."),
          gjay->player->name);
      gjay_error_dialog(gjay->gui->main_window, msg);
      g_free(msg);
    }
    return;
  }
  s = gjay->player->get_current_song(gjay->player,gjay->songs->name_hash);
  if (s) {
    explore_select_song(s);
  } else {
    if (gjay->player->is_running(gjay->player)) {
      gjay_error_dialog(gjay->gui->main_window, _("Sorry, GJay doesn't appear to know that song"));
    } else {
      msg = g_strdup_printf(_("Sorry, unable to connect to %s.\nIs the player running?"),
          gjay->player->name);
      gjay_error_dialog(gjay->gui->main_window, msg);
      g_free(msg);
    }
  }
}

void menuitem_quit (GtkAction *action, gpointer user_data) {
  GjayApp *gjay=(GjayApp*)user_data;
  quit_app(gjay->gui->main_window, NULL, gjay);
}

void menuitem_prefs( GtkAction *action, gpointer user_data) {
  GjayApp *gjay = (GjayApp*)user_data;
  gtk_window_present(GTK_WINDOW(gjay->gui->prefs_window));
}

