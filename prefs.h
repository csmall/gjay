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


#ifndef __PREFS_H__
#define __PREFS_H__

typedef enum {
    PREF_DAEMON_QUIT,
    PREF_DAEMON_DETACH,
    PREF_DAEMON_ASK
} pref_daemon_action;

typedef struct {
    /* Root directory (latin1) */
    gchar   * song_root_dir;
    gboolean  extension_filter; /* Do we only get *.{mp3,ogg,wav} files */

    /* What to do with daemon on quit */
    gint daemon_action;
    gboolean detach; /* Override set in UI session; not read 
                        from prefs file */

    /* Enable tooltips? */
    gboolean hide_tips;

    /* Use and show song rating? */
    gboolean use_ratings;

    /* Below is for playlist generation */
    gboolean use_selected_songs;
    gboolean use_selected_dir;
    gboolean start_selected;
    gboolean start_color;
    gboolean wander;
    gboolean rating_cutoff;
    int max_working_set;
    
    /* Playlist len, in minutes */
    guint time;
    
    float variance; 
    float hue;
    float brightness;
    float saturation;
    float bpm;
    float freq;
    float rating; 
    float path_weight;

    HSV color;

    /* Store how a color was loaded */
    gboolean use_hsv;
} GjayPrefs;




GjayPrefs *load_prefs ( void );
void save_prefs ( void );

#endif /* __PREFS_H__ */
