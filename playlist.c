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
#include "analysis.h"

static song * current;
static song * first;
static gdouble distance ( song * s1 );
/* Used for sorting song list */
static gint sort_dist ( gconstpointer a, gconstpointer b);


/* Generate a playlist no longer than the specified length */
GList * generate_playlist ( guint minutes ) {
    GList * working, * final, *temp;
    gint i, time;
    song * s;

    time = 0;
    if (!g_list_length(songs))
        return NULL;

    if (prefs.criteria_hue + 
        prefs.criteria_brightness +
        prefs.criteria_freq + 
        prefs.criteria_bpm == 0) {
        display_message("Some criterion must be non-zero");
        return NULL;
    }

    /* Create a working set, a copy of the songs list */
    working = g_list_copy(songs);
    final = NULL;

    if (prefs.rating_cutoff) {
        for (i = 0; i < g_list_length(working); i++) {
            current = SONG(g_list_nth(working, i));
            if (current->rating < prefs.rating) {
                i--;
                working = g_list_remove(working, current);
            }
        }
    }

    if (!working) {
        display_message("No songs to create playlist from");
        return NULL;
    }
    
    /* Pick the first song */
    if (prefs.start_random) {
        i = rand() % g_list_length(working);
    } else if (prefs.start_color) {
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
        i += prefs.variance - (rand() % (2*prefs.variance));
        if (i < 0)
            i = 0;
        else if (i >= g_list_length(working)) {
            i = g_list_length(working) - 1;
        }
    } else {
        for (temp = working, i = 0; temp; temp = g_list_next(temp), i++) {
            s = SONG(temp);
            if (s->checksum == prefs.song_cksum) 
                break;
        }
        if (!temp) {
            display_message("Sorry, that song isn't\nin the list");
            g_list_free(working);
            return NULL;
        }
    }
    first = SONG(g_list_nth(working, i));
    working = g_list_remove (working, first);
    final = g_list_append(final, first);
    current = first;

    /* Pick the rest of the songs */
    while (working && (time < minutes * 60)) {
        /* Pick song closest to current song, with some variance. */
        /* Sort list by distance to current song */
        working = g_list_sort (working, sort_dist);
        i = MIN(g_list_length(working), rand() % prefs.variance);
        current = SONG(g_list_nth(working, i));
        time += current->length;
        working = g_list_remove (working, current);
        final = g_list_append(final, current);
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
 */
static gdouble distance ( song * s1 ) {
    gdouble total_criteria, criteria;
    gdouble d, distance = 0;
    gint i;
    song * s2;
    
    total_criteria = 
        prefs.criteria_hue + 
        prefs.criteria_brightness +
        prefs.criteria_freq + 
        prefs.criteria_bpm;
    criteria = 0;

    s2 = (prefs.hold_hue ? first : current);
    if (!((s1->flags & COLOR_UNK) || (s2->flags & COLOR_UNK))) {
        criteria = prefs.criteria_hue;
        d = fabsl(s1->color.H - s2->color.H);
        if (d > M_PI) {
            if (s1->color.H > s2->color.H) 
                d = fabsl(s2->color.H + 2*M_PI - s1->color.H);
            else
                d = fabsl(s1->color.H + 2*M_PI - s2->color.H);
        }
        d *=  prefs.criteria_hue / (M_PI);
        distance += d;
    }

    s2 = (prefs.hold_brightness ? first : current);
    if (!((s1->flags & COLOR_UNK) || (s2->flags & COLOR_UNK))) {
        criteria +=  prefs.criteria_brightness;
        d = fabsl(s1->color.B - s2->color.B);
        d *= prefs.criteria_brightness;
        distance += d;
    }

    s2 = (prefs.hold_bpm ? first : current);
    if (!((s1->flags & BPM_UNK) || (s2->flags & BPM_UNK))) {
        criteria += prefs.criteria_bpm;
        if ((s1->flags & BPM_UNDEF) || (s2->flags & BPM_UNDEF)) {
            if ((s1->flags & BPM_UNDEF) && (s2->flags & BPM_UNDEF))
                d = 0;
            else 
                d = MAX_CRITERIA;
        } else {
            d = fabsl(s1->bpm - s2->bpm);
            d *= prefs.criteria_bpm / ((gdouble) (MAX_BPM - MIN_BPM));
        }
        distance += d;
    }

    s2 = (prefs.hold_freq ? first : current);
    if (!((s1->flags & FREQ_UNK) || (s2->flags & FREQ_UNK))) {
        criteria += prefs.criteria_freq;
        d = 0;
        for (i = 0; i < NUM_FREQ_SAMPLES; i++) {
            d += fabsl(s1->freq[i] - s2->freq[i]) / ideal_freq(i);
        }
        d *= prefs.criteria_freq / 25.0;
        distance += d;
    }

    if (criteria == 0) {
        /* There's no basis for comparing these songs. */
        return total_criteria;
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
