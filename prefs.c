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

app_prefs prefs;

static void default_prefs (void );
static gchar * read_string (FILE * in);

void load_prefs ( void ) {
    char buffer[BUFFER_SIZE];
    FILE * in;
    gint i;

    memset(&prefs, 0x00, sizeof(app_prefs));

    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_PREFS);
    in = fopen(buffer, "r");
    if (!in) {
        default_prefs();
        return;
    }
    fscanf(in, "%s\n", buffer);
    if (strcmp(buffer, GJAY_VERSION)) {
        /* Prefs didn't change from 0.1 */
        if (strcmp(buffer, "0.1")) {
            fprintf(stderr, "Unknown file format for version %s; this is version %s\n", buffer, GJAY_VERSION);
            default_prefs();
            fclose(in);
            return;
        }
    }   
    fread(&prefs, sizeof(app_prefs), 1, in);

    prefs.add_dir = read_string(in);
    if (prefs.num_color_labels) {
        prefs.label_colors = g_malloc(sizeof(HSV) * prefs.num_color_labels);
        prefs.label_color_titles = g_malloc(sizeof(gchar *) * prefs.num_color_labels);
        for (i = 0; i < prefs.num_color_labels; i++) {
            prefs.label_color_titles[i] = read_string(in);
        }
        for (i = 0; i < prefs.num_color_labels; i++) {
            fread(&prefs.label_colors[i], sizeof(HSV), 1, in);
        }    
    }

    fclose(in);
}


void save_prefs ( void ) {
    char buffer[BUFFER_SIZE];
    struct stat stat_buf;
    FILE * out;
    int i;

    snprintf(buffer, BUFFER_SIZE, "%s/%s", getenv("HOME"), GJAY_DIR);
    if (stat(buffer, &stat_buf) < 0) {
        if (mkdir (buffer, 
                   S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP |
                   S_IROTH | S_IXOTH) < 0) {
            fprintf (stderr, "Could not create %s\n", buffer);
            perror(NULL);
            return;
        }
        fprintf(stderr, "Created directory %s\n", buffer);
    }
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_PREFS);
    out = fopen(buffer, "w");
    if (!out) {
        fprintf (stderr, "Could not write to %s\n", buffer);
        return;
    }

    fprintf(out, "%s\n", GJAY_VERSION);

    fwrite(&prefs, sizeof(app_prefs), 1, out);

    if (prefs.add_dir) {
        fprintf(out, "%d %s\n", strlen(prefs.add_dir), prefs.add_dir);
    } else {
        fprintf(out, "0\n");
    }

    for (i = 0; i < prefs.num_color_labels; i++) {
        if (prefs.label_color_titles[i]) {
            fprintf(out, "%d %s\n", strlen(prefs.label_color_titles[i]), 
                    prefs.label_color_titles[i]);
        } else {
            fprintf(out, "0\n");
        }
    }
    for (i = 0; i < prefs.num_color_labels; i++) {
        fwrite(&prefs.label_colors[i], sizeof(HSV), 1, out);
    }
    fclose(out);
}

static void default_prefs (void ) {
    prefs.add_dir = NULL;
    prefs.start_random = TRUE;
    prefs.rating_cutoff = FALSE;
    prefs.sort = ARTIST_TITLE;
    prefs.rating = (MAX_RATING + MIN_RATING)/2.0;
    prefs.time = DEFAULT_PLAYLIST_TIME;
    prefs.criteria_bpm = prefs.criteria_freq = prefs.criteria_hue =
        prefs.criteria_brightness = DEFAULT_CRITERIA;
}

/* Read a string from a file formatted as "integer string\n". If integer is
   0, return NULL; otherwise, return g_malloc'd string */
static gchar * read_string (FILE * in) {
    gint len;
    gchar * str = NULL;
    fscanf(in, "%d", &len);
    if (len > 0) {
        str = g_malloc(len);
        fscanf(in, " %s", str);
    }
    fscanf(in, "\n");
    return str;
}
