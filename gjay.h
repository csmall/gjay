/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002-2004 Chuck Groom
 * Copyright (C) 2010-2012 Craig Small 
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

#ifndef GJAY_H
#define GJAY_H

#include "util.h"

/* Global definitions */
#define AUDACIOUS_BIN "/usr/bin/audacious"
#define EXAILE_BIN "/usr/bin/exaile"

#include <stdio.h>
#include <glib.h>
#ifdef WITH_GUI
#include <gtk/gtk.h>
#endif /* WITH_GUI */
#include <assert.h>
#include <limits.h>
#include <math.h>

#ifdef WITH_DBUSGLIB
#include <dbus/dbus-glib.h>
#endif /* WITH_DBUSGLIB */

typedef struct _GjayApp GjayApp;

#include "constants.h"
#include "rgbhsv.h"
#include "songs.h"
#include "prefs.h"
#include "ui.h"

/* Helper programs */
#define OGG_DECODER_APP "ogg123"
#define MP3_DECODER_APP1 "mpg123"
#define MP3_DECODER_APP2 "mpg321"
#define FLAC_DECODER_APP "flac"


typedef enum {
    UI = 0,
    DAEMON_INIT,    /* Pre-daemon mode, waiting for UI process activation */
    DAEMON,
    DAEMON_DETACHED,
    PLAYLIST,        /* Generate a playlist and quit */
    ANALYZE_DETACHED /* Analyze one file and quit */
} gjay_mode;


/* State */
extern gjay_mode mode;

/* User options */
extern gboolean      skip_verify;

/* Utilities */
void    read_line             ( FILE * f, 
                                char * buffer, 
                                int buffer_len);

typedef struct _GjayPlayer {
  gchar *name;
  GjaySong* (*get_current_song)(struct _GjayPlayer *player, GHashTable *song_name_hash);
  gboolean (*is_running)(struct _GjayPlayer *player);
  void (*play_files)(struct _GjayPlayer *player, GList *list);
  gboolean (*start)(struct _GjayPlayer *player);
#ifdef WITH_DBUSGLIB
  DBusGConnection *connection;
  DBusGProxy *proxy;
#endif /* WITH_DBUSGLIB */
#ifdef WITH_MPDCLIENT
  struct mpd_connection *mpdclient_connection;
#ifdef WITH_GUI
  GtkWidget *main_window; /* copy from GjayApp */
#endif /* WITH_GUI */
  gchar   * song_root_dir;
#endif /* WITH_MPDCLIENT */
} GjayPlayer;

struct _GjayApp {
  GjayPrefs *prefs;
  struct _GjayIPC	*ipc;
  GjayPlayer *player;
  struct _GjaySongLists *songs;
#ifdef WITH_GUI
  GjayGUI      * gui;
#else
  gpointer		*gui;
#endif /* WITH_GUI */

  guint verbosity;

  /* Player connections/handles */




  GList       * selected_songs, * selected_files; 
  /* We store a list of the directories which contain new songs (ie. lack
   * rating/color info */
  GList       * new_song_dirs;
  GHashTable  * new_song_dirs_hash;
  gint           tree_depth;               /* How deep does the tree go */

  /* Supported filetypes */
  gboolean ogg_supported;
  gboolean flac_supported;
};

/* From daemon.c */
void gjay_init_daemon(void);

#endif /* GJAY_H */
