/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2010-2015 Craig Small 
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

#include "gjay.h" 
#include "play_audacious.h"
#ifdef WITH_MPDCLIENT
#include "play_mpdclient.h"
#endif /* WITH_MPDCLIENT */
/*#include "play_exaile.h"*/
#include "i18n.h"
#ifdef WITH_DBUSGLIB
#include "dbus.h"
#endif /* WITH_DBUSGLIB */

extern char *music_player_names[];
static void noplayer_init(GjayPlayer *player);

void
set_player_name(GjayPlayer *player, const gushort selected_player)
{
  if (player->name)
    g_free(player->name);
  if (selected_player < PLAYER_LAST)
      player->name = g_strdup(music_player_names[selected_player]);
  else
      player->name = g_strdup(music_player_names[PLAYER_NONE]);
}

gboolean
create_player(GjayPlayer **player, const gushort music_player) {
  gboolean player_configured = FALSE;

  *player = g_malloc0(sizeof(GjayPlayer));
  set_player_name((*player), music_player);

#ifdef WITH_DBUSGLIB
  (*player)->connection = gjay_dbus_connection();
#endif /* WITH_DBUSGLIB */

  switch (music_player)
  {
    case PLAYER_NONE:
      /* break out, configured later */
      break;
#ifdef WITH_AUDCLIENT
    case PLAYER_AUDACIOUS:
      player_configured = audacious_init(*player);
      break;
#endif /* WITH_AUDCLIENT */
#ifdef WITH_EXAILE
    case PLAYER_EXAILE:
      exaile_init(*player);
      break;
#endif /* WITH_EXAILE */
#ifdef WITH_MPDCLIENT
    case PLAYER_MPDCLIENT:
      player_configured = mpdclient_init(*player);
      break;
#endif /* WITH_MPDCLIENT */
    default:
      g_error("Unknown music player.\n");
  }
  if (player_configured == FALSE)
    noplayer_init(*player);
  return player_configured;
}

void
play_song(GjayPlayer *player, GjaySong *s)
{
  GList *list;

  if (player->play_files==NULL)
    return;

  list = g_list_append(NULL, strdup_to_latin1(s->path));
  player->play_files(player, list);
  g_free((gchar*)list->data);
  g_list_free(list);
}

#ifdef WITH_GUI
void play_songs (GjayPlayer *player, GtkWidget *main_window, GList *slist) {
#else
void play_songs (GjayPlayer *player, gpointer dummy, GList *slist) {
#endif /*WITH_GUI*/
  GList *list = NULL;

  if (player->is_running==NULL || player->play_files == NULL
      || player->start == NULL )
    return;
  
  for (; slist; slist = g_list_next(slist)) 
    list = g_list_append(list, strdup_to_latin1(SONG(slist)->path));
  if (!list)
    return;
  

  if (!player->is_running(player))
  {
#ifdef WITH_GUI
    GtkWidget *dialog;
    gint result;
    gchar *msg;

    msg = g_strdup_printf(_("%s is not running, start %s?"),
        player->name,
        player->name);
    dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "%s", msg);
    result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(msg);
    if (result == GTK_RESPONSE_YES)
    {
      if (player->start(player) == FALSE)
      {
        msg = g_strdup_printf(_("Unable to start %s"), 
            player->name);
        dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "%s", msg);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_free(msg);
        return;
      }
    } else /* user clicked no */
      return;
#else
    /* with no GUI assume you want it started */
    if (player->start(player) == FALSE)
    {
      g_warning(_("Unable to start %s"), player->name);
      return;
    }
#endif /* WITH_GUI */
  }
  player->play_files(player, list);
}

static void 
noplayer_init(GjayPlayer *player)
{
  player->get_current_song = NULL;
  player->is_running = NULL;
  player->play_files = NULL;
  player->start = NULL;
}
