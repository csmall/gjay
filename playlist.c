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
#include "gjay.h"
#include "ui.h"
#include "analysis.h"

static song * current;
static song * first;
static gdouble distance      ( song * s1 );
/* Used for sorting song list */
static gint    sort_dist     ( gconstpointer a, gconstpointer b);
static gint    gaussian_rand ( gdouble deviation );


/**
 * Generate a playlist (list of song *) no longer than the specified
 * time, in minutes 
 */
GList * generate_playlist ( guint minutes ) {
    GList * working, * final, * rand_list, * ll;
    gint i, time, l, r, min_distance_index;
    gdouble min_distance, s_distance;
    song * s;

    time = 0;
    if (!g_list_length(songs))
        return NULL;

    /* Create a working set, a copy of the songs list */
    working = g_list_copy(songs); 
    /* FIXME: Add support for limiting songs to current selection */
    final = NULL;

    for (i = 0; i < g_list_length(working); i++) {
        current = SONG(g_list_nth(working, i));
        if ((!current->marked) || 
            ((prefs.rating_cutoff) && (current->rating < prefs.rating))) {
            i--;
            working = g_list_remove(working, current);
        } 
    }
    
    if (!working) {
        display_message("No songs to create playlist from");
        return NULL;
    }
    
    

    /* Pick the first song */
    /* FIXME: add support for selected song */
    if (prefs.start_color) {
        s = g_malloc(sizeof(song));
        memset(s, 0x00, sizeof(song));
        s->color.H = prefs.color.H;
        s->color.B = prefs.color.B;
        working = g_list_append(working, s);
        working = g_list_sort(working, sort_color);
        i = g_list_index(working, s);
        working = g_list_remove(working, s);
        g_free(s);
        if (rand()%2)
            i--;
        i += (int) prefs.variance - (rand() % (2* ((int) prefs.variance)));
        if (i < 0)
            i = 0;
        else if (i >= g_list_length(working)) {
            i = g_list_length(working) - 1;
        }
    } else {
        i = rand() % g_list_length(working); 
    }

    first = SONG(g_list_nth(working, i));
    working = g_list_remove (working, first);
    final = g_list_append(final, first);

    current = first;

    /* Pick the rest of the songs */
    while (working && (time < minutes * 60)) {
        /* Divide working list into { random set, leftover }. Then 
         * pick the best song in random set. */
        rand_list = NULL;
        min_distance = 10000;
        min_distance_index = -1;
        l = g_list_length(working);
        r = MAX(1, (l * prefs.variance) / MAX_CRITERIA );
        /* Reduce copy of working to size of random list */
        while(r--) {
            s = SONG(g_list_nth(working, rand() % g_list_length(working)));
            working = g_list_remove(working, s);
            rand_list = g_list_append(rand_list, s);
            /* Find the closest song */
            s_distance = distance(s);
            if (s_distance < min_distance) {
                min_distance = s_distance;
                min_distance_index = g_list_length(rand_list) - 1;
            }
        }
        current = SONG(g_list_nth(rand_list, min_distance_index));
        time += current->length;
        final = g_list_append(final, current);
        rand_list = g_list_remove(rand_list, current);
        working = g_list_concat(working, rand_list);
    }
    if (final && (time > minutes * 60)) {
        time -= SONG(g_list_last(final))->length;
        final = g_list_remove(final, SONG(g_list_last(final)));
    }
    
    g_list_free(working);
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
            distance += (prefs.path_weight * ((float) path_dist)) / 
                ((float) tree_depth);
        }
    }
    
    return distance * (total_criteria / criteria);
}



/* Return -1 if (a) is closer to current than (b), 0 if same dist, 1
   if (b) is closer than (a). */
static gint sort_dist ( gconstpointer a, gconstpointer b) {
    double d_a, d_b;
    d_a = distance((song *) a);
    d_b = distance((song *) b);
    if (d_a < d_b)
        return - 1;
    if (d_a > d_b)
        return 1;
    return 0;
}


void save_playlist ( GList * list, gchar * fname ) {
    FILE * f;
    song * s;
    gboolean title_unk;

    f = fopen(fname, "w");
    if (!f) {
        display_message("Sorry, cannot write playlist there.");
        return;
    }
    
    fprintf(f, "#EXTM3U\n");
    for (; list; list = g_list_next(list)) {
        s = SONG(list);
        title_unk = (strcmp(s->title, "?") == 0);
        if (title_unk) {
            fprintf(f, "#EXTINF:%d,%s\n",
                    s->length,
                    s->fname);
        } else {
            fprintf(f, "#EXTINF:%d,%s - %s\n",
                    s->length,
                    s->artist, 
                    s->title);
        } 
        fprintf(f, "%s\n", s->path);
    }
    fclose(f);
}


static gint gaussian_rand( gdouble deviation ) {
    float x1, x2, w, y1;
    
    do {
        x1 = 2.0 * (rand() / (gdouble) RAND_MAX) - 1.0;
        x2 = 2.0 * (rand() / (gdouble) RAND_MAX) - 1.0;
        w = x1 * x1 + x2 * x2;
    } while ( w >= 1.0 );
    
    w = sqrt( (-2.0 * log( w ) ) / w );
    y1 = fabs(x1 * w);
    return (int) (y1 * deviation + 1);
}
