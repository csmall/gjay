/**
 * GJay, copyright (c) 2009 Craig Small
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

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <audacious/dbus.h>
#include <audacious/audctrl.h>
#include "gjay.h" 
#include "songs.h" 
#include "dbus.h"

static void play_files ( GList *list);

gboolean
audacious_is_running(void)
{
  return gjay_dbus_is_running(AUDACIOUS_DBUS_SERVICE);
}

/* Connect and init audacious instance */
static void
audacious_connect(void)
{
  if (gjay->audacious_proxy != NULL)
    return;

  gjay->audacious_proxy = dbus_g_proxy_new_for_name(gjay->connection,
      AUDACIOUS_DBUS_SERVICE, AUDACIOUS_DBUS_PATH, AUDACIOUS_DBUS_INTERFACE);

}

void
play_song(song *s) {
  GList *list;

  list = g_list_append(NULL, strdup_to_latin1(s->path));
  play_files(list);
  g_free((gchar*)list->data);
  g_list_free(list);
}

void play_songs (GList *slist) {
  GList *list = NULL;
  
  for (; slist; slist = g_list_next(slist)) 
    list = g_list_append(list, strdup_to_latin1(SONG(slist)->path));
  play_files(list);
}

static void
play_files ( GList *list) {
  GList *lptr = NULL;
  gchar *uri = NULL;

  if (!list)
    return;
  audacious_connect();
  audacious_remote_stop(gjay->audacious_proxy);
  audacious_remote_playlist_clear(gjay->audacious_proxy);
  for (lptr=list; lptr; lptr = g_list_next(lptr)) {
      uri = g_filename_to_uri(lptr->data, NULL, NULL);
      audacious_remote_playlist_add_url_string(gjay->audacious_proxy, uri);
      g_free(uri);
  }
  audacious_remote_play(gjay->audacious_proxy);
}

song *
get_current_audacious_song(void) {
  gchar *playlist_file;
  gchar *uri;
  gint pos;
  song *s;

  audacious_connect();
  pos = audacious_remote_get_playlist_pos(gjay->audacious_proxy);
  playlist_file = audacious_remote_get_playlist_file(gjay->audacious_proxy, pos);
  if (playlist_file == NULL)
    return NULL;
  uri = g_filename_from_uri(playlist_file, NULL, NULL);
  if (uri == NULL)
    return NULL;
  s = g_hash_table_lookup(song_name_hash, uri);
  g_free(playlist_file);
  g_free(uri);
  return s;
}



