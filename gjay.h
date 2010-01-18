/**
 * GJay, copyright (c) 2002-4 Chuck Groom
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

#ifndef GJAY_H
#define GJAY_H

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
extern gint      verbosity;
extern gint      skip_verify;

/* App names */
extern char * ogg_decoder_app;
extern char * mp3_decoder_app;

/* Utilities */
void    read_line             ( FILE * f, 
                                char * buffer, 
                                int buffer_len);
#define strdup_to_utf8(str)   (strdup_convert(str, "UTF8", "LATIN1"))
#define strdup_to_latin1(str) (strdup_convert(str, "LATIN1", "UTF8"))
gchar * strdup_convert        ( const gchar * str, 
                                const gchar * enc_to, 
                                const gchar * enc_from );
float   strtof_gjay           ( const char * nptr,
                                char ** endptr);
gchar * parent_dir            ( const char * path );


struct _GjayApp {
  GjayPrefs *prefs;

  DBusGConnection *connection;
  DBusGProxy *audacious_proxy;

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

  gchar       * mp3_decoder_app;

  /* Songs */
  GList      * songs;       /* List of song ptrs  */
  GList      * not_songs;   /* List of char *, UTF8 encoded */
  gboolean     songs_dirty;

  GHashTable * song_name_hash; 
  GHashTable * song_inode_dev_hash;
  GHashTable * not_song_hash;
  
};

/* From daemon.c */
void gjay_init_daemon(void);

#endif /* GJAY_H */
