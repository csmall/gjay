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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> 
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "gjay.h"

typedef enum {
    TOKEN_SONG_ROOT_DIR,
    TOKEN_EXTENSION_FILTER,
    TOKEN_USE_SEL_SONG,
    TOKEN_USE_SEL_DIR,
    TOKEN_START_SEL,
    TOKEN_START_COLOR,
    TOKEN_WANDER,
    TOKEN_RATING_CUTOFF,
    TOKEN_TIME,
    TOKEN_VARIANCE,
    TOKEN_HUE,
    TOKEN_BRIGHTNESS,
    TOKEN_BPM,
    TOKEN_FREQ,
    TOKEN_DEPTH,
    TOKEN_RATING,
    TOKEN_PATH_WEIGHT,
    TOKEN_COLOR, 
    TOKEN_DAEMON_ACTION,
    TOKEN_PREFS_LAST,
} pref_tokens;

char * pref_token[TOKEN_PREFS_LAST] = {
    "SONG_ROOT_DIR",
    "EXTENSION_FILTER",
    "USE_SEL_SONG",
    "USE_SEL_DIR",
    "START_SEL",
    "START_COLOR",
    "WANDER",
    "RATING_CUTOFF",
    "TIME",
    "VARIANCE",
    "HUE",
    "BRIGHTNESS",
    "BPM",
    "FREQ",
    "DEPTH",
    "RATING",
    "PATH_WEIGHT",
    "COLOR",
    "DAEMON_ACTION"
};



app_prefs prefs;


 void load_prefs ( void ) {
     char buffer[BUFFER_SIZE], token[128];
     FILE * f;
     gint k;

     /* Set default values */
     memset(&prefs, 0x00, sizeof(app_prefs));
     prefs.rating = DEFAULT_RATING;
     prefs.time = DEFAULT_PLAYLIST_TIME;
     prefs.variance =
         prefs.hue = 
         prefs.brightness =
         prefs.bpm =
         prefs.freq =
         prefs.depth =
         prefs.path_weight = DEFAULT_CRITERIA;
     prefs.extension_filter = TRUE;
     prefs.color.H = 0;
     prefs.color.B = 0.5;
     prefs.daemon_action = PREF_DAEMON_QUIT;
     snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
              GJAY_DIR, GJAY_PREFS);
     f = fopen(buffer, "r");
     if (f) {
         while (!feof(f) && fscanf(f, "%s", token)) {
             for (k = 0; k < TOKEN_PREFS_LAST; k++) {
                 if (strcmp(token, pref_token[k]) == 0)
                     break;
             }
             switch ((pref_tokens) k) {
             case TOKEN_SONG_ROOT_DIR:
                 if (prefs.song_root_dir)
                     g_free(prefs.song_root_dir);
                 fscanf(f, " ");
                 read_line(f, buffer, BUFFER_SIZE);
                 prefs.song_root_dir = g_strdup(buffer);
                 continue; // Because we read the newline
             case TOKEN_EXTENSION_FILTER:
                 prefs.extension_filter = TRUE;
                 break;
             case TOKEN_USE_SEL_SONG:
                 prefs.use_selected_songs = TRUE;
                 break;
             case TOKEN_USE_SEL_DIR:
                 prefs.use_selected_dir = TRUE;
                 break;
             case TOKEN_START_SEL:
                 prefs.start_selected = TRUE;
                 break;
             case TOKEN_START_COLOR:
                 prefs.start_color = TRUE;
                 break;
             case TOKEN_WANDER:
                 prefs.wander = TRUE;
                 break;
             case TOKEN_RATING_CUTOFF:
                 prefs.rating_cutoff = TRUE;
                 break;
             case TOKEN_TIME:
                 fscanf(f, " %ud", (unsigned int *) &prefs.time);
                 break;    
             case TOKEN_DAEMON_ACTION:
                 fscanf(f, " %d", &prefs.daemon_action);
                 break;    
             case TOKEN_VARIANCE:
                 fscanf(f, " %f", &prefs.variance);
                 break;
             case TOKEN_HUE:
                 fscanf(f, " %f", &prefs.hue);
                 break;
             case TOKEN_BRIGHTNESS:
                 fscanf(f, " %f", &prefs.brightness);
                 break;
             case TOKEN_BPM:
                 fscanf(f, " %f", &prefs.bpm);
                 break;
             case TOKEN_FREQ:
                 fscanf(f, " %f", &prefs.freq);
                 break;
             case TOKEN_DEPTH:
                 fscanf(f, " %f", &prefs.depth);
                 break;
             case TOKEN_RATING:
                 fscanf(f, " %f", &prefs.rating);
                 break;
             case TOKEN_PATH_WEIGHT:
                 fscanf(f, " %f", &prefs.path_weight);
                 break;
             case TOKEN_COLOR:
                 fscanf(f, " %f %f", &prefs.color.H, &prefs.color.B);
                 break;
             default:
                 fprintf(stderr, "Did not understand pref token %s\n", token);
             }
             fscanf(f, "\n");
          }
         fclose(f);
     } 
}


void save_prefs ( void ) {
    char buffer[BUFFER_SIZE];
    FILE * f;
    
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_PREFS);
    f = fopen(buffer, "w");
    if (f) {
        if (prefs.song_root_dir)
            fprintf(f, "%s %s\n", pref_token[TOKEN_SONG_ROOT_DIR],
                    prefs.song_root_dir);
        if (prefs.extension_filter)
            fprintf(f, "%s\n", pref_token[TOKEN_EXTENSION_FILTER]);        
        if (prefs.use_selected_songs)
            fprintf(f, "%s\n", pref_token[TOKEN_USE_SEL_SONG]);   
        if (prefs.use_selected_dir)
            fprintf(f, "%s\n", pref_token[TOKEN_USE_SEL_DIR]);   
        if (prefs.start_selected)
            fprintf(f, "%s\n", pref_token[TOKEN_START_SEL]);   
        if (prefs.start_color)
            fprintf(f, "%s\n", pref_token[TOKEN_START_COLOR]);   
        if (prefs.wander)
            fprintf(f, "%s\n", pref_token[TOKEN_WANDER]);   
        if (prefs.rating_cutoff)
            fprintf(f, "%s\n", pref_token[TOKEN_RATING_CUTOFF]);
            
        fprintf(f, "%s %f\n", pref_token[TOKEN_VARIANCE], prefs.variance);
        fprintf(f, "%s %f\n", pref_token[TOKEN_HUE], prefs.hue);
        fprintf(f, "%s %f\n", pref_token[TOKEN_BRIGHTNESS], prefs.brightness);
        fprintf(f, "%s %f\n", pref_token[TOKEN_BPM], prefs.bpm);
        fprintf(f, "%s %f\n", pref_token[TOKEN_FREQ], prefs.freq);
        fprintf(f, "%s %f\n", pref_token[TOKEN_DEPTH], prefs.depth);
        fprintf(f, "%s %f\n", pref_token[TOKEN_RATING], prefs.rating);
        fprintf(f, "%s %f\n", pref_token[TOKEN_PATH_WEIGHT], prefs.path_weight);

        fprintf(f, "%s %d\n", pref_token[TOKEN_TIME], prefs.time);
        fprintf(f, "%s %d\n", pref_token[TOKEN_DAEMON_ACTION], 
                prefs.daemon_action);

        fprintf(f, "%s %f %f\n", pref_token[TOKEN_COLOR], prefs.color.H,
                prefs.color.B);
        
        fclose(f);
    } else {
        fprintf(stderr, "Unable to write prefs %s\n", buffer);
    }
}

