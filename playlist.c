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
static gdouble distance      ( song * s1 );

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
    gint i, list_time, l, r, min_distance_index, len;
    gdouble min_distance, s_distance;
    song * s, * repeat;
    time_t t;

    list_time = 0;

    if (verbosity) 
        t = time(NULL);
    
    if (!g_list_length(songs))
        return NULL;

    /* Create a working set, a copy of the songs list */
    if (prefs.use_selected_songs) 
        working = g_list_copy(selected_songs); 
    else
        working = g_list_copy(songs); 

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
            (prefs.rating_cutoff && (current->rating < prefs.rating)) ||
            (prefs.use_selected_dir && 
             selected_files && 
             (strncmp((char *) selected_files->data, current->path,
                      strlen((char *) selected_files->data)) != 0))) {
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
    if (prefs.start_selected) {
        for (list = g_list_first(working); list && !first; 
             list = g_list_next(list)) {
            if (strncmp((char *) selected_files->data, SONG(list)->path,
                        strlen((char *) selected_files->data)) == 0)
                first = SONG(list);
        }
    } 
    if (prefs.start_color || (prefs.start_selected && !first)) {
        for (min_distance = 10000, list = g_list_first(working); list; 
             list = g_list_next(list)) {
            s = SONG(list);
            if (!s->no_color) {
                s_distance = MIN(fabs(prefs.color.H - s->color.H),
                                 (MIN(prefs.color.H, s->color.H)  + 2*M_PI) -
                                 MAX(prefs.color.H, s->color.H)) +
                    BRIGHTNESS_FACTOR * fabs(prefs.color.B - s->color.B);
                if ((s_distance < min_distance) || 
                    ((s_distance == min_distance) && (rand() % 2))) {
                    min_distance = s_distance;
                    first = SONG(list);
                } 
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
        min_distance = 10000;
        min_distance_index = -1;
        l = g_list_length(working);
        r = MAX(1, (l * prefs.variance) / MAX_CRITERIA );
        /* Reduce copy of working to size of random list */
        len = g_list_length(working);
        while(r--) {
            s = SONG(g_list_nth(working, rand() % len));
            working = g_list_remove(working, s);
            rand_list = g_list_append(rand_list, s);
            len--;
            /* Find the closest song */
            s_distance = distance(s);
            if (s_distance < min_distance) {
                min_distance = s_distance;
                min_distance_index = g_list_length(rand_list) - 1;
            }
        }
        current = SONG(g_list_nth(rand_list, min_distance_index));
        list_time += current->length;
        final = g_list_append(final, current);
        rand_list = g_list_remove(rand_list, current);
        working = g_list_concat(working, rand_list);
 
       /* If the song had any duplicates (symlinks) in the working
        * list, remove them from the working list, too */
        for (repeat = current->repeat_prev; repeat; 
             repeat = repeat->repeat_prev) 
            working = g_list_remove(working, repeat);
        for (repeat = current->repeat_next; repeat; 
             repeat = repeat->repeat_next) 
            working = g_list_remove(working, repeat);
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


/* Calculate "distance" between a song and "current" or "first", depending
 * on prefs. There are four factors:
 *  - Hue
 *  - Brightness
 *  - BPM
 *  - Freq
 *  - Sorting distance
 */
static gdouble distance ( song * s1 ) {
    gdouble total_criteria, criteria;
    gdouble d, distance = 0;
    gint i, path_dist;
    song * s2;
    
    total_criteria = 
        prefs.hue + 
        prefs.brightness +
        prefs.freq + 
        prefs.bpm +
        prefs.path_weight;
    criteria = 0;

    if (prefs.wander) 
        s2 = current;
    else
        s2 = first;

    if (!((s1->no_color) || (s2->no_color))) {
        criteria = prefs.hue;
        d = fabsl(s1->color.H - s2->color.H);
        if (d > M_PI) {
            if (s1->color.H > s2->color.H) 
                d = fabsl(s2->color.H + 2*M_PI - s1->color.H);
            else
                d = fabsl(s1->color.H + 2*M_PI - s2->color.H);
        }
        d *=  prefs.hue / (M_PI);
        distance += d;
    } else {
        /* Add half a hit against this combo */
        criteria += prefs.hue / 2.0;
        distance += prefs.hue / 2.0;
    }

    if (!(s1->no_color || s2->no_color)) {
        criteria +=  prefs.brightness;
        d = fabsl(s1->color.B - s2->color.B);
        d *= prefs.brightness;
        distance += d;
    } else {
        /* Add half a hit against this combo */
        criteria +=  prefs.brightness / 2;
        distance +=  prefs.brightness / 2;
    }


    criteria += prefs.bpm;
    if (s1->bpm_undef || s2->bpm_undef) {
        if (s1->bpm_undef && s2->bpm_undef)
            d = 0;
        else 
            d = MAX_CRITERIA;
    } else {
        d = fabsl(s1->bpm - s2->bpm);
        d *= prefs.bpm / ((gdouble) (MAX_BPM - MIN_BPM));
    }
    distance += d;
   
    criteria += prefs.freq;
    for (d = 0, i = 0; i < NUM_FREQ_SAMPLES; i++) {
        d += fabsl(s1->freq[i] - s2->freq[i]);
        if (i < NUM_FREQ_SAMPLES - 1) {
            d += fabsl(s1->freq[i] - s2->freq[i + 1])/2.0;  
            d += fabsl(s1->freq[i + 1] - s2->freq[i])/2.0;  
        }
        if (i > 0) {
            d += fabsl(s1->freq[i] - s2->freq[i - 1])/2.0;  
            d += fabsl(s1->freq[i - 1] - s2->freq[i])/2.0;  
        }
    }
    d /= 5.0;
    d *= 1 + (MAX(s1->volume_diff, s2->volume_diff) - MIN(s1->volume_diff, s2->volume_diff))/10.0;
    d *= prefs.freq;
    distance += d;
    
    if (tree_depth) {
        path_dist = explore_files_depth_distance(s1->path, s2->path);
        if (path_dist >= 0) {
            criteria += prefs.path_weight;
            distance += PATH_DIST_FACTOR * (
                (prefs.path_weight * ((float) path_dist)) / 
                ((float) tree_depth));
        }
    }
    
    return distance * (total_criteria / criteria);
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


