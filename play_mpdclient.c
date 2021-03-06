/*
 * Gjay - Gtk+ DJ music playlist creator
 * play_mpd.c : Output for mpd
 * Copyright (C) 2012 Craig Small 
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

#ifdef WITH_MPDCLIENT

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <mpd/client.h>
#include "gjay.h"
#include "i18n.h"
#include "play_mpdclient.h"
#ifdef WITH_GUI
#include "ui.h"
#endif /* WITH_GUI */

/* dlsym-ed functions */
const char *(*gjmpd_connection_get_error_message)(const struct mpd_connection *connection);
struct mpd_connection *(*gjmpd_connection_new)(const char *host, unsigned port, unsigned timeout);
struct mpd_status *(*gjmpd_run_status)(struct mpd_connection *conn);
void (*gjmpd_status_free)(struct mpd_status *status);
struct mpd_song *(*gjmpd_run_current_song)(struct mpd_connection *conn);
void (*gjmpd_song_free)(struct mpd_song *song);
const char *(*gjmpd_song_get_uri)(const struct mpd_song *song);
bool (*gjmpd_run_stop)(struct mpd_connection *conn);
bool (*gjmpd_run_clear)(struct mpd_connection *conn);
bool (*gjmpd_run_add)(struct mpd_connection *conn, const char *uri);
bool (*gjmpd_run_play)(struct mpd_connection *conn);


static gboolean mpdclient_connect(GjayPlayer *player);

gboolean 
mpdclient_init(GjayPlayer *player)
{
  void *lib;

  if ( (lib = dlopen("libmpdclient.so.2", RTLD_GLOBAL | RTLD_LAZY)) == NULL)
  {
      gjay_error(player->main_window,
                 _("Unable to open mpd client library"));
    return FALSE;
  }
  
  if ( (gjmpd_connection_new = gjay_dlsym(lib, "mpd_connection_new")) == NULL)
    return FALSE;
  if ( (gjmpd_connection_get_error_message = gjay_dlsym(lib, "mpd_connection_get_error_message")) == NULL)
    return FALSE;
  if ( (gjmpd_run_status = gjay_dlsym(lib, "mpd_run_status")) == NULL)
    return FALSE;
  if ( (gjmpd_status_free = gjay_dlsym(lib, "mpd_status_free")) == NULL)
    return FALSE;
  if ( (gjmpd_run_current_song = gjay_dlsym(lib, "mpd_run_current_song")) == NULL)
    return FALSE;
  if ( (gjmpd_song_free = gjay_dlsym(lib, "mpd_song_free")) == NULL)
    return FALSE;
  if ( (gjmpd_song_get_uri = gjay_dlsym(lib, "mpd_song_get_uri")) == NULL)
    return FALSE;
  if ( (gjmpd_run_stop = gjay_dlsym(lib, "mpd_run_stop")) == NULL)
    return FALSE;
  if ( (gjmpd_run_clear = gjay_dlsym(lib, "mpd_run_clear")) == NULL)
    return FALSE;
  if ( (gjmpd_run_add = gjay_dlsym(lib, "mpd_run_add")) == NULL)
    return FALSE;
  if ( (gjmpd_run_play = gjay_dlsym(lib, "mpd_run_play")) == NULL)
    return FALSE;
  
  /* Success! we update the function pointers */
  player->get_current_song = &mpdclient_get_current_song;
  player->is_running = &mpdclient_is_running;
  player->play_files = &mpdclient_play_files;
  player->start = &mpdclient_start;
  return TRUE;
}

gboolean
mpdclient_start(GjayPlayer *player)
{
  return TRUE;
}

gboolean
mpdclient_is_running(GjayPlayer *player)
{
  struct mpd_status *status;

  if (mpdclient_connect(player) == FALSE)
    return FALSE;
  if ( (status=(*gjmpd_run_status)(player->mpdclient_connection)) == NULL) {
    g_warning("mpd_run_status returned NULL");
    return FALSE;
  }
  (*gjmpd_status_free)(status);
  return TRUE;
}

GjaySong *
mpdclient_get_current_song(GjayPlayer *player, GHashTable *song_name_hash) {
  GjaySong *s;
  struct mpd_song *ms;
  const char *uri;
  gchar *song_file;

  mpdclient_connect(player);
  if ( (ms = (*gjmpd_run_current_song)(player->mpdclient_connection)) == NULL)
    return NULL;
  uri = (*gjmpd_song_get_uri)(ms);
  song_file = g_strdup_printf("%s/%s",player->song_root_dir,uri);
  /* FIXME - how do we determine its a file or not? */
  s = g_hash_table_lookup(song_name_hash, song_file);
  (*gjmpd_song_free)(ms);
  g_free(song_file);
  return s;
}

void
mpdclient_play_files ( GjayPlayer *player, GList *list) {
  GList *lptr;
  gsize srd_len;
  gchar *song_fname;
  gchar *errmsg;

  if (mpdclient_connect(player) == FALSE)
    return;
  srd_len = strlen(player->song_root_dir);
  if ( (*gjmpd_run_stop)(player->mpdclient_connection) == FALSE) {
    errmsg = g_strdup_printf(_("Cannot stop Music Player Daemon: %s"),
        (*gjmpd_connection_get_error_message)(player->mpdclient_connection));
    gjay_error(player->main_window, errmsg);
    g_free(errmsg);
  }
  if ( (*gjmpd_run_clear)(player->mpdclient_connection) == FALSE) {
    errmsg = g_strdup_printf(_("Cannot clear Music Player Daemon queue: %s"),
        (*gjmpd_connection_get_error_message)(player->mpdclient_connection));
    gjay_error(player->main_window,errmsg);
    g_free(errmsg);
  }
  for (lptr=list; lptr; lptr= g_list_next(lptr)) {
    song_fname = lptr->data;
    if (g_ascii_strncasecmp(player->song_root_dir, song_fname, srd_len) == 0)
    {
      song_fname += srd_len;
      if (*song_fname == '\0')
        continue;
      song_fname++;
      if ( (*gjmpd_run_add)(player->mpdclient_connection, song_fname) == FALSE) {
        errmsg = g_strdup_printf(_("Cannot add song \"%s\" to Music Player Daemon Queue: %s"),
            song_fname, 
            (*gjmpd_connection_get_error_message)(player->mpdclient_connection));
        gjay_error(player->main_window, errmsg);
        g_free(errmsg);
      }
    }
  }//for
  if ( (*gjmpd_run_play)(player->mpdclient_connection) == FALSE) {
    errmsg = g_strdup_printf(_("Cannot play playlist: %s"),
        (*gjmpd_connection_get_error_message)(player->mpdclient_connection));
    gjay_error(player->main_window, errmsg);
    g_free(errmsg);
  }
}

/* static functions */

/* Connect and init mpd instance */
static gboolean
mpdclient_connect(GjayPlayer *player)
{
  if (player->mpdclient_connection != NULL)
    return TRUE;
  
  if ( (player->mpdclient_connection = (*gjmpd_connection_new)(NULL,0,0)) == NULL) {
    gjay_error(player->main_window, _("Unable to connect of Music Player Daemon"));
    return FALSE;
  }
  return TRUE;
}

#endif /* WITH_MPDCLIENT */
