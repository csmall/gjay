/**
 * GJay, copyright (c) 2002-2003 Chuck Groom
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
#include "gjay.h"

#ifndef __SONGS_H__
#define __SONGS_H__


typedef enum {
    OGG = 0, 
    MP3,
    WAV,
    SONG_FILE_TYPE_LAST
} song_file_type;


typedef struct _song song;

typedef enum {
    ARTIST_TITLE = 0,
    COLOR,
    BPM, 
    RATING,
    FREQ
} sort_by;
 
extern GList      * songs;             /*  song *   */
extern GList      * not_songs;         /*  char *   */
extern gboolean     songs_dirty;       /* Songs list does not match
                                          disk copy */
extern GHashTable * song_name_hash;
extern GHashTable * song_inode_hash;
extern GHashTable * not_song_hash;


struct _song {
    /* Characteristics (don't change). Strings are UTF8 encoded. */
    char * path;
    char * fname; /* Pointer within path */
    char * title;
    char * artist;
    char * album;
    gdouble bpm;
    gboolean bpm_undef;
    gdouble freq[NUM_FREQ_SAMPLES];
    gdouble volume_diff; /* % difference in volume between loudest and
                            average frame */
    gint    length;      /* Song length, in sec */
    
    /* Attributes (user-defined values that may change) */
    HB color;
    gdouble rating;

    /* Meta-info */
    gboolean no_data;   /* Characteristics not set */
    gboolean no_rating; /* Rating attributes not set */
    gboolean no_color;  /* Color attributes not set */

    /* Transient flags, not saved with song data */
    gboolean in_tree;
    gboolean marked; 

    /* Transient display pixbuf */
    GdkPixbuf * freq_pixbuf;
    GdkPixbuf * color_pixbuf;

    /* How to tell if two songs with different paths are the same
       song (symlinked, most likely) */
    guint32 inode; 
    song * repeat_prev, * repeat_next;    

    /* Does the song exist? */
    gboolean access_ok;
};


#define SONG(list) ((song *) list->data)

song *      create_song            ( void );
void        delete_song            ( song * s );
song *      song_set_path          ( song * s, 
                                     char * path );
void        song_set_freq_pixbuf   ( song * s);
void        song_set_color_pixbuf  ( song * s);
void        song_set_repeats       ( song * s, 
                                     song * original );
void        song_set_repeat_attrs  ( song * s);
void        file_info              ( gchar          * path,
                                     gboolean       * is_song,
                                     guint32        * inode,
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
gdouble     song_force             ( song * a, song  * b );
gdouble     song_attraction        ( song * a, song  * b );
gdouble     song_mass              ( song * s );


void        write_data_file        ( void );
int         write_dirty_song_timeout ( gpointer data );
int         append_daemon_file     ( song * s );

void        read_data_file         ( void );
void        read_daemon_file       ( void );
gboolean    add_from_daemon_file_at_seek ( int seek );


#endif /* __SONGS_H__ */
