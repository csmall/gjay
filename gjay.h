/**
 * GJay, copyright (c) 2002 Chuck Groom
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
#include "rgbhsv.h"

/* We batch the freq spectrum into just a few bins */
#define NUM_FREQ_SAMPLES 30

/* Default directory for storing app info */
#define GJAY_VERSION "0.1"
#define GJAY_DIR     ".gjay"
#define GJAY_DATA    "gjay_data"
#define GJAY_PREFS   "gjay_prefs"

/* We use fixed-size buffers for labels and filenames */
#define BUFFER_SIZE  FILENAME_MAX

/* How frequently, in milliseconds, we check the analyize data */
#define ANALYZE_TIME 500

#define MIN_RATING            0.0
#define MAX_RATING            5.0
#define DEFAULT_RATING        2.5
#define MIN_BPM               100.0
#define MAX_BPM               160.0
#define MIN_VARIANCE          1
#define MAX_VARIANCE          10
#define MIN_PLAYLIST_TIME     10
#define MAX_PLAYLIST_TIME     10000
#define DEFAULT_PLAYLIST_TIME 72
#define MIN_CRITERIA          0.0
#define MAX_CRITERIA          10.0
#define DEFAULT_CRITERIA      5.0

/* song flags */
#define RATING_UNK (1 << 2)
#define BPM_UNK    (1 << 3)
#define BPM_UNDEF  (1 << 4)
#define FREQ_UNK   (1 << 5)
#define COLOR_UNK  (1 << 6)

/* Color wheel size */
#define CATEGORIZE_DIAMETER   200   
#define SELECT_RADIUS         3

/* Size of colorful pixmaps */
#define COLOR_SWATCH_W 27
#define COLOR_SWATCH_H 11
#define FREQ_PM_W      NUM_FREQ_SAMPLES + 2
#define FREQ_PM_H      COLOR_SWATCH_H


typedef struct _song song;

typedef enum {
    APP_SONGS = 0,
    APP_MAKE_PLAYLIST,
    APP_ABOUT,
    APP_LAST
} app_tabs;


typedef enum {
    SONG_TITLE = 0,
    SONG_ARTIST,
    SONG_FREQ,
    SONG_BPM,
    SONG_COLOR,
    SONG_RATING,
    SONG_LAST
} song_attr;

typedef enum {
    ARTIST_TITLE = 0,
    COLOR,
    BPM, 
    RATING,
    FREQ
} sort_by;

typedef struct {
    /* How to start playlist */
    gboolean start_random;
    gboolean start_color;
    gboolean start_song;

    HB color;

    /* Which song to start from, as identified by its checksum */
    guint32 song_cksum; 

    /* Whether to cut-off at a particular rating */
    gboolean rating_cutoff;
    gdouble rating; 

    /* How much randomness to introduce in selection */
    gint variance; 

    /* Playlist len, in minutes */
    guint time;

    /* Criteria holds */
    gboolean hold_hue;
    gboolean hold_brightness;
    gboolean hold_bpm;
    gboolean hold_freq;

    /* Criteria weight */
    gdouble criteria_hue;
    gdouble criteria_brightness;
    gdouble criteria_bpm;
    gdouble criteria_freq;

    /* How to sort */
    sort_by sort;

    /* Last directory to add files from */
    gchar * add_dir;

    /* The following doesn't have to do with the playlist, but instead 
       color labels */
    gint num_color_labels;
    RGB * label_colors;
    gchar ** label_color_titles;
} app_prefs;


struct _song {
    char * path;
    char * fname; /* Pointer within path */
    char * title;
    char * artist;
    char * album;
    guint32 checksum;   /* md5sum */
    guint16 length;     /* Song length, in sec */
    HB color;
    gdouble rating;
    gdouble bpm;
    gdouble freq[NUM_FREQ_SAMPLES];
    gint8   flags; 
};


extern gint num_songs;
extern GList * songs;        /* List of songs. Sorting may change. */
extern GList * dialog_list;  /* List of open dialogs. */

extern GtkWidget * app_window;
extern GtkWidget * song_list;
extern GtkWidget * playlist_list;
extern app_prefs prefs;
extern app_tabs current_tab;
extern gint xmms_session;

#define SONG(list) ((song *) list->data)

/* prefs.c */
void        load_prefs  ( void );
void        save_prefs  ( void );

/* songs.c */
void        load_songs  ( void );
void        save_songs  ( void );
song *      new_song_file ( gchar * fname );
void        sort        ( GList ** list );
gboolean    correct_sort ( GList * item);
void        delete_song ( song * s);
gint        song_add    ( GList ** song_list, song * s);
HSV         average_color ( GList * list );
gdouble     average_rating ( GList * list );
GList *     songs_by_artist ( GList * list, gchar * artist );
void        print_song  ( song * s );
gint        sort_artist_title ( gconstpointer a, gconstpointer b);
gint        sort_color ( gconstpointer a, gconstpointer b);
 

/* playlist.c */
GList *     generate_playlist ( guint len );
void        save_playlist ( GList * list, gchar * fname);

/* xmms.c */
void        init_xmms   ( void );
void        play_song   ( song * s );
void        play_songs  ( GList * slist );

/* ui.c */
GtkWidget * make_app_ui ( void );
RGB         song_get_rgb (song * s);
guint32     song_get_rgb_hex (song * s);
void        rebuild_song_in_list (song * s);
void        rebuild_new_song_in_list (song * s);
GtkWidget * build_song_list(void);
void        display_message ( gchar * msg );
void        dialog_unrealize (GtkWidget * widget,
                              gpointer user_data);

/* ui_color_wheel.c */
GtkWidget * make_color_wheel ( HB * hb );

/* ui_star_rating.c */
GtkWidget * make_star_rating ( GtkWidget * window, 
                               gdouble * rating );
/* ui_components.c */

GtkWidget * make_song_clist (void);
void        build_song_list_row ( GtkCList * clist, 
                                  guint row, 
                                  song * s);
GdkPixmap * create_color_pixmap ( guint32 rgb_hex );

/* ui_categorize.c */
GtkWidget * make_categorize_dialog ( GList * song_list,
                                     GFunc   done_func );

#endif /* GJAY_H */
