#ifndef __PREFS_H__
#define __PREFS_H__

#define DEFAULT_PLAYLIST_TIME 72
#define MIN_CRITERIA          0.1
#define MAX_CRITERIA          10.0
#define DEFAULT_CRITERIA      5.0

typedef struct {
    /* Root directory */
    gchar   * song_root_dir;
    gboolean  extension_filter; /* Do we only get *.{mp3,ogg,wav} files */

    /* Below is for playlist generation */
    gboolean use_selected_songs;
    gboolean use_selected_dir;
    gboolean start_selected;
    gboolean start_color;
    gboolean wander;
    gboolean rating_cutoff;
    
    /* Playlist len, in minutes */
    guint time;
    
    float variance; 
    float hue;
    float brightness;
    float bpm;
    float freq;
    float depth;
    float rating; 
    float path_weight;

    HB color;
} app_prefs;


extern app_prefs prefs;


void load_prefs ( void );
void save_prefs ( void );

#endif /* __PREFS_H__ */
