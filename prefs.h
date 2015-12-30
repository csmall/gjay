/**
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002 Chuck Groom
 * Copyright (C) 2010-2015 Craig Small 
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


#ifndef __PREFS_H__
#define __PREFS_H__

#include "colors.h"

typedef enum {
    PREF_DAEMON_QUIT,
    PREF_DAEMON_DETACH,
    PREF_DAEMON_ASK
} pref_daemon_action;

/* Music Player types */
#define PLAYER_NONE 0
#define PLAYER_AUDACIOUS 1
#define PLAYER_MPDCLIENT 2
#define PLAYER_LAST 3

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
    gboolean use_color;
    gboolean wander;
    gboolean rating_cutoff;
    int max_working_set;
    
    guint playlist_time; /* Playlist len, in minutes */
    
    float variance; 
    float hue;
    float brightness;
    float saturation;
    float bpm;
    float freq;
    float rating; 
    float path_weight;

    HSV start_color;

    /* Store how a color was loaded */
    gboolean use_hsv;

    /* which player will we use */
    gushort music_player;
} GjayPrefs;




GjayPrefs *load_prefs ( void );
void save_prefs ( GjayPrefs *prefs );

#endif /* __PREFS_H__ */
