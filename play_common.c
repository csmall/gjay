/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2010-2011 Craig Small 
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

static void noplayer_init(void);

void
player_init(void)
{
  gboolean player_configured = FALSE;

  switch (gjay->prefs->music_player)
  {
    case PLAYER_NONE:
      /* break out, configured later */
      break;
    case PLAYER_AUDACIOUS:
      player_configured = audacious_init();
      break;
    /*case PLAYER_EXAILE:
      exaile_init();
      break;*/
#ifdef WITH_MPDCLIENT
    case PLAYER_MPDCLIENT:
      player_configured = mpdclient_init();
      break;
#endif /* WITH_MPDCLIENT */
    default:
      g_error("Unknown music player.\n");
  }
  if (player_configured == FALSE)
    noplayer_init();
}

void
play_song(song *s)
{
  GList *list;

  if (gjay->player_play_files==NULL)
    return;

  list = g_list_append(NULL, strdup_to_latin1(s->path));
  gjay->player_play_files(list);
  g_free((gchar*)list->data);
  g_list_free(list);
}

void play_songs (GList *slist) {
  GList *list = NULL;

  if (gjay->player_is_running==NULL || gjay->player_play_files == NULL
      || gjay->player_start == NULL )
    return;
  
  for (; slist; slist = g_list_next(slist)) 
    list = g_list_append(list, strdup_to_latin1(SONG(slist)->path));
  if (!list)
    return;
  

  if (!gjay->player_is_running())
  {
    GtkWidget *dialog;
    gint result;
    gchar *msg;

    msg = g_strdup_printf(_("%s is not running, start %s?"),
        gjay->prefs->music_player_name,
        gjay->prefs->music_player_name);
    dialog = gtk_message_dialog_new(GTK_WINDOW(gjay->main_window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        msg);
    result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(msg);
    if (result == GTK_RESPONSE_YES)
    {
      if (gjay->player_start() == FALSE)
      {
        msg = g_strdup_printf(_("Unable to start %s"), 
            gjay->prefs->music_player_name);
        dialog = gtk_message_dialog_new(GTK_WINDOW(gjay->main_window),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            msg);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_free(msg);
        return;
      }
    } else /* user clicked no */
      return;
  }
  gjay->player_play_files(list);
}

static void 
noplayer_init(void)
{
  gjay->player_get_current_song = NULL;
  gjay->player_is_running = NULL;
  gjay->player_play_files = NULL;
  gjay->player_start = NULL;
}
