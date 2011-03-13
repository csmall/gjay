/*
 * Gjay - Gtk+ DJ music playlist creator
 * play_audacious.c : Output for Audacious
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <audacious/dbus.h>
#include <audacious/audctrl.h>
#include "gjay.h" 
#include "songs.h" 
#include "dbus.h"
#include "i18n.h"
#include "play_audacious.h"

static void audacious_connect(void);

/* public functions */

void 
audacious_init(void)
{
  gjay->player_get_current_song = &audacious_get_current_song;
  gjay->player_is_running = &audacious_is_running;
  gjay->player_play_files = &audacious_play_files;
  gjay->player_start = &audacious_start;
}

gboolean
audacious_start(void)
{
  GError *error = NULL;
  GAppInfo *aud_app;
  int i;

  if ((aud_app = g_app_info_create_from_commandline(
    AUDACIOUS_BIN,
    "Audacious",
    G_APP_INFO_CREATE_NONE,
    &error)) == NULL)
  {
    g_warning(_("Cannot create audacious g_app: %s\n"), error->message);
    g_error_free(error);
    return FALSE;
  }
  error=NULL;
  if (g_app_info_launch(aud_app, NULL, NULL, &error) == FALSE)
  {
    g_warning(_("Cannot launch audacious g_app: %s"), error->message);
    g_error_free(error);
    return FALSE;
  }
  /* Give it 3 tries */
  for(i=1; i<= 3; i++)
  {
    if (audacious_is_running())
      return TRUE;
    sleep(1);
  }
  return FALSE;
}
gboolean
audacious_is_running(void)
{
  return gjay_dbus_is_running(AUDACIOUS_DBUS_SERVICE);
}

song *
audacious_get_current_song(void) {
  gchar *playlist_file;
  gchar *uri;
  gint pos;
  song *s;

  audacious_connect();
  pos = audacious_remote_get_playlist_pos(gjay->player_proxy);
  playlist_file = audacious_remote_get_playlist_file(gjay->player_proxy, pos);
  if (playlist_file == NULL)
    return NULL;
  uri = g_filename_from_uri(playlist_file, NULL, NULL);
  if (uri == NULL)
    return NULL;
  s = g_hash_table_lookup(gjay->song_name_hash, uri);
  g_free(playlist_file);
  g_free(uri);
  return s;
}

void
audacious_play_files ( GList *list) {
  GList *lptr = NULL;
  gchar *uri = NULL;


  audacious_connect();
  audacious_remote_stop(gjay->player_proxy);
  audacious_remote_playlist_clear(gjay->player_proxy);
  for (lptr=list; lptr; lptr = g_list_next(lptr)) {
      uri = g_filename_to_uri(lptr->data, NULL, NULL);
      audacious_remote_playlist_add_url_string(gjay->player_proxy, uri);
      g_free(uri);
  }
  audacious_remote_play(gjay->player_proxy);
}

/* static functions */

/* Connect and init audacious instance */
static void
audacious_connect(void)
{
  if (gjay->player_proxy != NULL)
    return;

  gjay->player_proxy = dbus_g_proxy_new_for_name(gjay->connection,
      AUDACIOUS_DBUS_SERVICE, AUDACIOUS_DBUS_PATH, AUDACIOUS_DBUS_INTERFACE);

}




