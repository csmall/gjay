#ifndef __SONGS_H__
#define __SONGS_H__

/* We batch the freq spectrum into just a few bins */
#define NUM_FREQ_SAMPLES 30


#define MIN_RATING            0.0
#define MAX_RATING            5.0
#define DEFAULT_RATING        2.5
#define MIN_BPM               100.0
#define MAX_BPM               160.0

typedef struct _song song;

typedef enum {
    ARTIST_TITLE = 0,
    COLOR,
    BPM, 
    RATING,
    FREQ
} sort_by;
 
extern GList      * songs;          /* song *  */
extern GList      * rated;          /* song *  */
extern GList      * files_not_song; /* char *  */
extern GHashTable * song_name_hash;
extern GHashTable * rated_name_hash;
extern GHashTable * files_not_song_hash;


struct _song {
    /* Characteristics (don't change) */
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
    guint16 length;      /* Song length, in sec */
    
    /* Attributes (user-defined values that may change) */
    HB color;
    gdouble rating;

    /* Meta-info */
    gboolean no_data; /* Characteristics not set */
    gboolean no_rating; /* Rating attributes not set */
    gboolean no_color;  /* Color attributes not set */

    /* Marked by tree */
    gboolean marked; 

    /* Transient display pixbuf */
    GdkPixbuf * freq_pixbuf;
    GdkPixbuf * color_pixbuf;

    /* FIXME -- add */
    // guint32 checksum; 
};


#define SONG(list) ((song *) list->data)

song *      create_song            ( char * fname );
song *      create_song_from_file  ( gchar * fname );
void        delete_song            ( song * s );

int         append_daemon_file     ( char * fname, song * s );
void        append_attr_file       ( song * s );

void        read_data_file         ( void );
void        read_daemon_file       ( void );
void        read_attr_file         ( void );
void        write_attr_file        ( void );
void        write_data_file        ( void );

gboolean    add_from_daemon_file_at_seek ( int seek );



void        sort                 ( GList ** list );
HSV         average_color        ( GList * list );
gdouble     average_rating       ( GList * list );
GList *     songs_by_artist      ( GList * list, gchar * artist );
void        song_set_freq_pixbuf  ( song * s);
void        song_set_color_pixbuf ( song * s);
void        print_song  ( song * s );

/* Different song sort functions */
gint sort_bpm ( gconstpointer a, gconstpointer b);
gint sort_rating ( gconstpointer a, gconstpointer b);
gint sort_freq   ( gconstpointer a, gconstpointer b);
gint sort_artist_title ( gconstpointer a, gconstpointer b);
gint sort_color ( gconstpointer a, gconstpointer b);

#endif /* __SONGS_H__ */
