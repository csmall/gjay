/*
 * Gjay - Gtk+ DJ music playlist creator
 * play_audacious.c : Output for Audacious
 * Copyright 2010-2012 Craig Small 
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

#ifdef WITH_AUDCLIENT

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <audacious/dbus.h>
#include <audacious/audctrl.h>
#include <gio/gio.h>
#include "gjay.h" 
#include "songs.h" 
#include "dbus.h"
#include "i18n.h"
#include "ui.h"
#include "play_audacious.h"

static void audacious_connect(GjayPlayer *player);

/* dlym'ed functions */
gint (*gjaud_get_playlist_pos)(DBusGProxy *proxy);
gchar *(*gjaud_get_playlist_file)(DBusGProxy *proxy, guint pos);
void (*gjaud_stop)(DBusGProxy *proxy);
void (*gjaud_playlist_clear)(DBusGProxy *proxy);
void (*gjaud_playlist_add_url_string)(DBusGProxy *proxy, gchar *string);
void (*gjaud_play)(DBusGProxy *proxy);

/* public functions */

gboolean 
audacious_init(GjayPlayer *player)
{
  void *lib;

  if ( (lib = dlopen("libaudclient.so.2", RTLD_GLOBAL | RTLD_LAZY)) == NULL)
  {
    gjay_error_dialog(player->main_window,
		_("Unable to open audcious client library, defaulting to no play."));
    return FALSE;
  }
  if ( (gjaud_get_playlist_pos = gjay_dlsym(lib, "audacious_remote_get_playlist_pos")) == NULL)
    return FALSE;
  if ( (gjaud_get_playlist_file = gjay_dlsym(lib, "audacious_remote_get_playlist_file")) == NULL)
    return FALSE;
  if ( (gjaud_stop = gjay_dlsym(lib, "audacious_remote_stop")) == NULL)
    return FALSE;
  if ( (gjaud_play = gjay_dlsym(lib, "audacious_remote_play")) == NULL)
    return FALSE;
  if ( (gjaud_playlist_clear = gjay_dlsym(lib, "audacious_remote_playlist_clear")) == NULL)
    return FALSE;
  if ( (gjaud_playlist_add_url_string = gjay_dlsym(lib, "audacious_remote_playlist_add_url_string")) == NULL)
    return FALSE;

  player->get_current_song = &audacious_get_current_song;
  player->is_running = &audacious_is_running;
  player->play_files = &audacious_play_files;
  player->start = &audacious_start;
  return TRUE;
}

gboolean
audacious_start(GjayPlayer *player)
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
    if (audacious_is_running(player))
      return TRUE;
    sleep(1);
  }
  return FALSE;
}

gboolean
audacious_is_running(GjayPlayer *player)
{
  return gjay_dbus_is_running(AUDACIOUS_DBUS_SERVICE,player->connection);
}

GjaySong *
audacious_get_current_song(GjayPlayer *player, GHashTable *song_name_hash) {
  gchar *playlist_file;
  gchar *uri;
  gint pos;
  GjaySong *s;

  audacious_connect(player);
  pos = (*gjaud_get_playlist_pos)(player->proxy);
  playlist_file = (*gjaud_get_playlist_file)(player->proxy, pos);
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

void
audacious_play_files ( GjayPlayer *player, GList *list) {
  GList *lptr = NULL;
  gchar *uri = NULL;


  audacious_connect(player);
  (*gjaud_stop)(player->proxy);
  (*gjaud_playlist_clear)(player->proxy);
  for (lptr=list; lptr; lptr = g_list_next(lptr)) {
      uri = g_filename_to_uri(lptr->data, NULL, NULL);
      (*gjaud_playlist_add_url_string)(player->proxy, uri);
      g_free(uri);
  }
  (*gjaud_play)(player->proxy);
}

/* static functions */

/* Connect and init audacious instance */
static void
audacious_connect(GjayPlayer *player)
{
  if (player->proxy != NULL)
    return;

  player->proxy = dbus_g_proxy_new_for_name(player->connection,
      AUDACIOUS_DBUS_SERVICE, AUDACIOUS_DBUS_PATH, AUDACIOUS_DBUS_INTERFACE);

}

#endif /* WITH_AUDCLIENT */
