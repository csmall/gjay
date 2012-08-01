/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002-2004 Chuck Groom
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

#ifndef __SONGS_H__
#define __SONGS_H__

#include "gjay.h"
#include "prefs.h"

typedef enum {
    OGG = 0, 
    MP3,
    WAV,
    FLAC,
    SONG_FILE_TYPE_LAST
} song_file_type;


typedef struct _song GjaySong;

typedef enum {
    ARTIST_TITLE = 0,
    COLOR,
    BPM, 
    RATING,
    FREQ
} sort_by;
 
typedef struct _GjaySongLists {
  GList			* songs;
  GList			* not_songs;
  GHashTable	* name_hash;
  GHashTable	* inode_dev_hash;
  GHashTable	* not_hash;

  gboolean		dirty;
} GjaySongLists;

struct _song {
    /* Characteristics (don't change). Strings are UTF8 encoded. */
    char   * path;
    char   * fname; /* Pointer within path */
    char   * title;
    char   * artist;
    char   * album;
    gdouble  bpm;
    gboolean bpm_undef;
    gdouble  freq[NUM_FREQ_SAMPLES];
    gdouble  volume_diff; /* % difference in volume between loudest and
                             average frame */
    gint     length;      /* Song length, in sec */
    
    /* Attributes (user-defined values that may change) */
    HSV      color;
    gdouble  rating;

    /* Meta-info */
    gboolean no_data;   /* Characteristics not set */
    gboolean no_rating; /* Rating attributes not set */
    gboolean no_color;  /* Color attributes not set */

    /* Transient flags, not saved with song data */
    gboolean in_tree;
    gboolean marked; 

#ifdef WITH_GUI
    /* Transient display pixbuf */
    GdkPixbuf * freq_pixbuf;
    GdkPixbuf * color_pixbuf;
#endif /* WITH_GUI */

    /* How to tell if two songs with different paths are the same
       song (symlinked, most likely) */
    guint32 inode; 
    guint32 dev; 
    guint32 inode_dev_hash;
    
    GjaySong * repeat_prev, * repeat_next;    

    /* Does the song exist? */
    gboolean access_ok;
};

#define SONG(list) ((GjaySong *) list->data)

GjaySong *      create_song            ( void );
void        delete_song            ( GjaySong * s );
GjaySong *      song_set_path          ( GjaySong * s, 
                                     char * path );
#ifdef WITH_GUI
void        song_set_freq_pixbuf   ( GjaySong * s);
void        song_set_color_pixbuf  ( GjaySong * s);
#endif /* WITH_GUI */
void        song_set_repeats       ( GjaySong * s, 
                                     GjaySong * original );
void        song_set_repeat_attrs  ( GjaySong * s);
void        file_info              ( const guint verbosity,
									 const gboolean ogg_supported,
									 const gboolean flac_supported,
	                                 gchar          * path,
                                     gboolean       * is_song,
                                     guint32        * inode,
                                     guint32        * dev,
                                     gint           * length,
                                     gchar         ** title,
                                     gchar         ** artist,
                                     gchar         ** album,
                                     song_file_type * type );
/* There are two factors for the related-ness of two songs;
 * how similiar their parameters are, and how important these
 * similarities are we. We model this like particle
 * attraction/repulsion (f = m1 * m2 * d^2), where "mass" means the 
 * importance of each song's attributes and "d" is the attraction
 * or repulsion (note that the attraction of a -> b = b-> a)
 */
gdouble song_force ( const GjayPrefs *prefs, GjaySong * a, GjaySong  * b, const gint tree_depth );


void        write_data_file        ( GjayApp *gjay );
int         write_dirty_song_timeout ( gpointer data );
int         append_daemon_file     ( GjaySong * s );
void        write_song_data        ( FILE * f, GjaySong * s );


void        read_data_file         ( GjayApp *gjay );
gboolean    add_from_daemon_file_at_seek ( GjayApp *gjay, const gint seek );
void        hash_inode_dev         ( GjaySong * s,
                                     gboolean has_dev );


#endif /* __SONGS_H__ */
