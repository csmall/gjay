/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002-2004 Chuck Groom
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

#ifndef GJAY_H
#define GJAY_H

#include "util.h"

/* Global definitions */
#define AUDACIOUS_BIN "/usr/bin/audacious2"
#define EXAILE_BIN "/usr/bin/exaile"

#include <stdio.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <dbus/dbus-glib.h>

typedef struct _GjayApp GjayApp;
extern GjayApp *gjay;

#include "constants.h"
#include "rgbhsv.h"
#include "songs.h"
#include "prefs.h"
//#include "ui.h"

/* Helper programs */
#define OGG_DECODER_APP "ogg123"
#define MP3_DECODER_APP1 "mpg321"
#define MP3_DECODER_APP2 "mpg123"
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
extern gboolean      verbosity;
extern gboolean      skip_verify;

/* Utilities */
void    read_line             ( FILE * f, 
                                char * buffer, 
                                int buffer_len);
gchar * parent_dir            ( const char * path );


struct _GjayApp {
  GjayPrefs *prefs;
  /* Player connections/handles */
  DBusGConnection *connection;
  DBusGProxy *player_proxy;
#ifdef WITH_MPDCLIENT
  struct mpd_connection *mpdclient_connection;
#endif /* WITH_MPDCLIENT */


  //GdkPixbuf   * pixbufs[PM_LAST];
  GdkPixbuf   * pixbufs[50]; //FIXME
  GtkTooltips * tips;
  GtkWidget   * explore_view, * selection_view, * playlist_view,
     * no_root_view, * prefs_view, * about_view;
  GList       * selected_songs, * selected_files; 
  /* We store a list of the directories which contain new songs (ie. lack
   * rating/color info */
  GList       * new_song_dirs;
  GHashTable  * new_song_dirs_hash;
  gint           tree_depth;               /* How deep does the tree go */

  /* Various Windows */
  GtkWidget *main_window;
  GtkWidget *notebook;
  GtkWidget * prefs_window;
  GtkWidget * message_window;


  /* Songs */
  GList      * songs;       /* List of song ptrs  */
  GList      * not_songs;   /* List of char *, UTF8 encoded */
  gboolean     songs_dirty;

  GHashTable * song_name_hash; 
  GHashTable * song_inode_dev_hash;
  GHashTable * not_song_hash;
  
  /* Supported filetypes */
  gboolean ogg_supported;
  gboolean flac_supported;


  /* Player calls */
  song* (*player_get_current_song)(void);
  gboolean (*player_is_running)(void);
  void (*player_play_files)(GList *list);
  gboolean (*player_start)(void);
};

/* From daemon.c */
void gjay_init_daemon(void);

#endif /* GJAY_H */
