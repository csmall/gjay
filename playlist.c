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

#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "gjay.h"
#include "ui.h"
#include "analysis.h"
#include "playlist.h"

static song * current;
static song * first;
static void remove_repeats   ( song * s, GList * list);

/* How much does brightness factor into matching two songs? */
#define BRIGHTNESS_FACTOR .8

#define PATH_DIST_FACTOR 0.5

/* Limit size of working set to 1500 songs */
#define MAX_WORKING_SET 1500

/**
 * Generate a playlist (list of song *) no longer than the specified
 * time, in minutes 
 */
GList * generate_playlist ( guint minutes ) {
    GList * working, * final, * rand_list, * list;
    gint i, list_time, l, r, max_force_index, len;
    gdouble max_force, s_force;
    song * s;
    time_t t;

    list_time = 0;

    if (verbosity) 
        t = time(NULL);
    
    if (!g_list_length(gjay->songs))
        return NULL;

    /* Create a working set, a copy of the songs list */
    if (gjay->prefs->use_selected_songs) 
        working = g_list_copy(gjay->selected_songs); 
    else
        working = g_list_copy(gjay->songs); 

    final = NULL;
    for (list = g_list_first(working); list; ) {
        current = SONG(list);
        /* OK, hold your breath. We exclude songs which:
         * 1. Are not in the current file tree
         * 2. Are no longer present
         * 3. Are below the rating cutoff, if applied by the user
         * 4. Are not in the currently selected directory, if the user
         *    specified that s/he wanted to limit the playlist to the 
         *    current dir. */
        if ((!current->in_tree) || 
            (!current->access_ok) ||
            (gjay->prefs->use_ratings && gjay->prefs->rating_cutoff && 
             (current->rating < gjay->prefs->rating)) ||
            (gjay->prefs->use_selected_dir && 
             gjay->selected_files && 
             (strncmp((char *) gjay->selected_files->data, current->path,
                      strlen((char *) gjay->selected_files->data)) != 0))) {
            if (g_list_next(list))
                list = g_list_next(list);
            else 
                list = g_list_previous(list);
            working = g_list_remove(working, current);
        } else {
            list = g_list_next(list);
        }
    }
    
    if (!working) {
        display_message("No songs to create playlist from");
        return NULL;
    }
    
    /* Pick the first song */
    first = NULL;
    if (gjay->prefs->start_selected) {
        for (list = g_list_first(working); list && !first; 
             list = g_list_next(list)) {
            if (strncmp((char *) gjay->selected_files->data, SONG(list)->path,
                        strlen((char *) gjay->selected_files->data)) == 0)
                first = SONG(list);
        }
        if (!first) {
            gchar * latin1;
            latin1 = strdup_to_latin1((char *) gjay->selected_files->data);
            fprintf(stderr, "File '%s' not found in data file;\n"\
                    "perhaps it has not been analyzed. Using random starting song.\n",
                    latin1);
            g_free(latin1);
        }
    } 
    if (gjay->prefs->start_color) {
        song temp_song;
        bzero(&temp_song, sizeof(song));
        temp_song.no_data = TRUE;
        temp_song.no_rating = TRUE;
        temp_song.color = gjay->prefs->color;
        for (max_force = -1000, list = g_list_first(working); 
             list; 
             list = g_list_next(list)) {
            s = SONG(list);
            s_force = song_force(&temp_song, s);
            if (s_force > max_force) {
                max_force = s_force;
                first = s;
            }
        } 
    } 
    if (!first) {
        /* Pick random starting song */
        i = rand() % g_list_length(working); 
        first = SONG(g_list_nth(working, i));
    }

    final = g_list_append(final, first);    
    working = g_list_remove (working, first);
    current = first;
    
    /* If the song had any duplicates (symlinks) in the working
     * list, remove them from the working list, too */
    remove_repeats(current, working);

    /* Regretably, we must winnow the working set to something reasonable.
       If there were 10,000 songs, this would take ~20 seconds on a fast
       machine. */
    for (len = g_list_length(working); len > MAX_WORKING_SET; len--) {
        s = SONG(g_list_nth(working, rand() % len));
        working = g_list_remove(working, s);
    }
    

    /* Pick the rest of the songs */
    while (working && (list_time < minutes * 60)) {
        /* Divide working list into { random set, leftover }. Then 
         * pick the best song in random set. */
        rand_list = NULL;
        max_force = -10000;
        max_force_index = -1;
        l = g_list_length(working);
        r = MAX(1, (l * gjay->prefs->variance) / MAX_CRITERIA );
        /* Reduce copy of working to size of random list */
        len = g_list_length(working);
        while(r--) {
            s = SONG(g_list_nth(working, rand() % len));
            working = g_list_remove(working, s);
            rand_list = g_list_append(rand_list, s);
            len--;
            /* Find the closest song */
            if (gjay->prefs->wander) 
                s_force = song_force(s, current);
            else
                s_force = song_force(s, first);

            if (s_force > max_force) {
                max_force = s_force;
                max_force_index = g_list_length(rand_list) - 1;
            }
        }
        current = SONG(g_list_nth(rand_list, max_force_index));
        list_time += current->length;
        final = g_list_append(final, current);
        rand_list = g_list_remove(rand_list, current);
        working = g_list_concat(working, rand_list);
        
       /* If the song had any duplicates (symlinks) in the working
        * list, remove them from the working list, too */
        remove_repeats(current, working);
    }
    if (final && (list_time > minutes * 60)) {
        list_time -= SONG(g_list_last(final))->length;
        final = g_list_remove(final, SONG(g_list_last(final)));
    }
    
    g_list_free(working);

    if (verbosity) 
        printf("It took %d seconds to generate playlist\n",  
               (int) (time(NULL) - t));
    
    return final;
}


void save_playlist ( GList * list, gchar * fname ) {
    FILE * f;
    f = fopen(fname, "w");
    if (!f) {
        display_message("Sorry, cannot write playlist there.");
        return;
    }
    write_playlist(list, f, TRUE);
    fclose(f);
}


void write_playlist ( GList * list, FILE * f, gboolean m3u_format) {
    song * s;
    gchar * l1_artist, * l1_title, * l1_path, * l1_fname;
    
    if (m3u_format)
        fprintf(f, "#EXTM3U\n");
    for (; list; list = g_list_next(list)) {
        s = SONG(list);
        if (m3u_format) {
            if (s->title) {
                l1_artist = strdup_to_latin1(s->artist);
                l1_title = strdup_to_latin1(s->title);
                
                fprintf(f, "#EXTINF:%d,%s - %s\n",
                        s->length,
                        l1_artist,
                        l1_title);
                g_free(l1_artist);
                g_free(l1_title);
            } else {
                l1_fname = strdup_to_latin1(s->fname);
                fprintf(f, "#EXTINF:%d,%s\n",
                        s->length,
                        l1_fname);
                g_free(l1_fname);
            } 
        }
        l1_path = strdup_to_latin1(s->path);
        fprintf(f, "%s\n", l1_path);
        g_free(l1_path);
    }
}


static void remove_repeats ( song * s, GList * list) {
    song * repeat;
    for (repeat = s->repeat_prev; repeat; 
         repeat = repeat->repeat_prev) {
        list = g_list_remove(list, repeat);
    }
    for (repeat = s->repeat_next; repeat; 
         repeat = repeat->repeat_next) {
        list = g_list_remove(list, repeat);
    }
}
