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
#include <string.h>
#include <vorbis/vorbisfile.h>
#include <vorbis/codec.h>
#include "gjay.h"
#include "analysis.h"

/* We compute our own CRC 32 cksums on the first bit of each file */
#define CRC_TABLE_SIZE 256
#define CRC_BYTES 50*1024
static guint32 * crc_table = NULL;

gint num_songs;
GList * songs;   
GList * songs_new;

song * new_song ( char * fname );
guint32 file_cksum ( char * fname );
song * test_ogg ( char * fname );
song * test_mp3 ( char * fname );
song * test_wav ( char * fname );
void get_line ( FILE * f, char * buffer, int buffer_len);
/* Different song sort functions */
gint sort_bpm ( gconstpointer a, gconstpointer b);
gint sort_rating ( gconstpointer a, gconstpointer b);
gint sort_freq ( gconstpointer a, gconstpointer b);
/* sort_artist_title and sort_color declared in gjay.h */

/* For CRC32 checksum */
void gen_crc_table(void);


/* Read song database */
void load_songs ( void ) {
    char buffer[BUFFER_SIZE];
    song * s;
    FILE * in;
    int i, k, t, len;
    unsigned int n;
    long unsigned int ln;

    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_DATA);
    num_songs = 0;
    in = fopen(buffer, "r");
    if (!in) {
        fprintf(stderr, "File not found: %s\n", buffer);
        return;
    }

    fscanf(in, "%s", buffer);
    if (strcmp(buffer, GJAY_VERSION)) {
        fprintf(stderr, "Unknown file format for version %s; this is version %s\n", buffer, GJAY_VERSION);
        return;
    }
        
    fscanf(in, "%d ", &num_songs);
    for (i = 0; i < num_songs; i++) {
        s = g_malloc(sizeof(song));
        fscanf(in, "%d ", &len);
        s->path = g_malloc(len + 1);
        s->path[len] = '\0';
        fread(s->path, len, 1, in);
        fscanf(in, "%d ", &len);
        s->fname = s->path + len;
        fscanf(in, "%d ", &len);
        s->title = g_malloc(len + 1);
        s->title[len] = '\0';
        fread(s->title, len, 1, in);
        fscanf(in, "%d ", &len);
        s->artist = g_malloc(len + 1);
        s->artist[len] = '\0';
        fread(s->artist, len, 1, in);
        fscanf(in, "%d ", &len);
        s->album = g_malloc(len + 1);
        s->album[len] = '\0';
        fread(s->album, len, 1, in);

        fscanf(in, "%f %f %lg %lg ",
               &s->color.H, &s->color.B, &s->rating, &s->bpm);
        for (k = 0; k < NUM_FREQ_SAMPLES; k++) {
            fscanf(in, "%lg ", &s->freq[k]);
        }
        fscanf(in, "%lu ", &ln);
        s->checksum = ln;
        fscanf(in, "%u ", &n);
        s->length = n;
        fscanf(in, "%d ", &t);
        s->flags = t;
        songs = g_list_append(songs, s);
    }
    fclose(in);
    sort(&songs);
}

/* Save song database */
void save_songs ( void ) {
    char buffer[BUFFER_SIZE];
    struct stat stat_buf;
    FILE * out;
    GList * current;
    song * s;
    int i;

    snprintf(buffer, BUFFER_SIZE, "%s/%s", getenv("HOME"), GJAY_DIR);
    if (stat(buffer, &stat_buf) < 0) {
        if (mkdir (GJAY_DIR, 
                   S_IXUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | 
                   S_IROTH | S_IXOTH) < 0) {
            fprintf (stderr, "Could not create %s\n", buffer);
            return;
        }
    }
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_DATA);
    out = fopen(buffer, "w");
    if (!out) {
        fprintf (stderr, "Could not write to %s\n", buffer);
        return;
    }
    
    fprintf(out, "%s ", GJAY_VERSION);
    fprintf(out, "%d ", num_songs);
    for (current = songs; current; current = g_list_next(current)) {
        s = SONG(current);
        fprintf(out, "%d %s %d %d %s %d %s %d %s %f %f %f %f ",
                strlen(s->path), s->path,
                s->fname - s->path, /* The filename is an offset
                                       within the path */
                strlen(s->title), s->title,
                strlen(s->artist), s->artist,
                strlen(s->album), s->album,
                s->color.H, s->color.B, s->rating, s->bpm);
        for (i = 0; i < NUM_FREQ_SAMPLES; i++) {
            fprintf(out, "%f ", s->freq[i]);
        } 
        fprintf(out, "%lu ", (long unsigned int) s->checksum);
        fprintf(out, "%u ", (unsigned int) s->length);
        fprintf(out, "%d ", s->flags);
        fprintf(out, "\n");
    }
    fclose(out);
}


/* Create a new song from a file. Return NULL if file invalid, not an
   mp3, ogg, or wav, or if the song is already in the list. */
song * new_song_file ( gchar * fname ) {
    song * s;
    GList * current;
    guint32 checksum;
    char buffer[BUFFER_SIZE];
    
    if (access(fname, R_OK)) 
        return NULL;
    checksum = file_cksum(fname);
    
    for (current = songs; current; current = g_list_next(current)) {
        if(SONG(current)->checksum == checksum) {
            snprintf(buffer, BUFFER_SIZE, "%s\nalready in list",
                     SONG(current)->fname);
            display_message(buffer);
            return NULL;
        }
    }
    

    s = test_ogg(fname);
    if (!s) 
        s = test_wav(fname);
    if (!s) 
        s = test_mp3(fname);
    if (s) {
        s->checksum = checksum;
        if (!s->title)
            s->title = g_strdup ("?");
        if (!s->artist)
            s->artist = g_strdup ("?");
        if (!s->album)
            s->album = g_strdup ("?");
    } else {
        display_message("Sorry, unable to open.\nMake sure file is mp3, ogg, or wav");
    }
    return s;
}


void print_song ( song * s) {
    int i;
    printf("Path: '%s'\n", s->path);
    printf("Fname: '%s'\n", s->fname);
    printf("Title: '%s'\n", s->title);
    printf("Artist: '%s'\n", s->artist);
    printf("Album: '%s'\n", s->album);
    printf("Length: %.2d:%.2d\n", s->length/60, s->length%60);
    printf("Checksum: %lu\n", (long unsigned int) s->checksum);
    printf("Hue: %f\n", s->color.H);
    printf("Brightness: %f\n", s->color.B);
    printf("Rating: %f\n", s->rating);
    printf("BPM: %f\n", s->bpm);
    printf("Flags: %x\n", s->flags);
    printf("Freq: ");
    for (i = 0; i < NUM_FREQ_SAMPLES; i++) {
        printf("%.3f ", s->freq[i]);
    }
    printf("\n");
}


void delete_song (song * s) {
    g_free(s->path);
    g_free(s->title);
    g_free(s->artist);
    g_free(s->album);
    g_free(s);
}

song * new_song ( char * fname ) {
    int i;
    song * s = g_malloc(sizeof(song));
    memset (s, 0x00, sizeof(song));
    s->flags = COLOR_UNK | BPM_UNK | FREQ_UNK;
    /* FIXME: get average of other songs by this artist */
    s->rating = (MIN_RATING + MAX_RATING)/2;
    s->path = g_strdup(fname);
    s->fname = s->path;
    for (i = strlen(s->path) - 1; i; i--) {
        if (s->path[i] == '/') {
            s->fname = s->path + i + 1;
            return s;
        }
    }
    return s;
}


void get_line ( FILE * f, char * buffer, int buffer_len) {
    int i;
    for (i = 0, buffer[0] = '\0'; 
         (!feof(f)) && (i < (buffer_len - 1));
         i++) {
        buffer[i] = fgetc(f);
        if (buffer[i] == '\n') {
            break;
        }
    }
    buffer[i] = '\0';
    return;
}


song * test_mp3 ( char * fname ) {
     FILE * f;
     char buffer[BUFFER_SIZE];
     song * s = NULL;
     
     snprintf(buffer, BUFFER_SIZE, 
              "mp3info -p \"frames:%%u\\n%%a\\n%%t\\n%%l\\n%%S\" %s", fname);
     if (!(f = popen(buffer, "r"))) {
         fprintf(stderr, "Unable to run %s\n", buffer);
         return NULL;
     } 
     while (!feof(f) && !s) {
         get_line(f, buffer, BUFFER_SIZE);
         if (strncmp(buffer, "frames:", strlen("frames:")) == 0) {
             if (strlen(buffer) > strlen("frames:")) {
                 s = new_song(fname);
             }
         }
     }
     if (!feof(f) && s) {
         get_line(f, buffer, BUFFER_SIZE);
         if (strlen(buffer)) {
             s->artist = g_strdup(buffer);
         }
         get_line(f, buffer, BUFFER_SIZE);
         if (strlen(buffer)) {
             s->title = g_strdup(buffer);
         }
         get_line(f, buffer, BUFFER_SIZE);
         if (strlen(buffer)) {
             s->album = g_strdup(buffer);
         }
         get_line(f, buffer, BUFFER_SIZE);
         if (strlen(buffer)) {
             s->length = atoi(buffer);
         }
     }
     pclose(f);
     return s;
}


song * test_ogg ( char * fname ) { 
    FILE * f;
    song * s = NULL;
    int i;
    OggVorbis_File vf;
    vorbis_comment * vc;

    f = fopen(fname, "r");
    if (!f) {
        return NULL;
    }
    if(ov_open(f, &vf, NULL, 0) == 0) {
        s = new_song(fname);
        vc = ov_comment(&vf, -1);
        s->length = ov_time_total(&vf, -1);
                
        for (i = 0; i < vc->comments; i++) {
            if (strncmp(vc->user_comments[i], "title=", strlen("title=")) == 0) {
                s->title = g_strdup(vc->user_comments[i] + strlen("title="));
            } else if (strncmp(vc->user_comments[i], "artist=", strlen("artist=")) == 0) {
                s->artist = g_strdup(vc->user_comments[i] + strlen("artist="));
            } else if (strncmp(vc->user_comments[i], "album=", strlen("album=")) == 0) {
                s->album = g_strdup(vc->user_comments[i] + strlen("album="));
            }
        }
//        ov_clear(&vf);
    }
    fclose(f);
    return s;
}


song * test_wav ( char * fname ) {
    FILE * f;
    song * s = NULL;
    struct stat buf;
    waveheaderstruct header;

    f = fopen(fname, "r");
    if (!f)
        return NULL;
    fread(&header, sizeof(waveheaderstruct), 1, f);
    fclose(f);
    if ((strncmp(header.chunk_type, "WAVE", 4) == 0) &&
        (header.byte_p_spl / header.modus == 2)) {
        s = new_song(fname);
        stat(fname, & buf);
        s->length = (buf.st_size - sizeof(waveheaderstruct)) / 176758;
    }
    return s;
}


void gen_crc_table(void) {
    guint32 crc, poly;
    int	i, j;
    crc_table = g_malloc(sizeof(guint32) * CRC_TABLE_SIZE);

    poly = 0xEDB88320L;
    for (i = 0; i < CRC_TABLE_SIZE; i++)
        {
        crc = i;
        for (j = 8; j > 0; j--)
            {
            if (crc & 1)
                crc = (crc >> 1) ^ poly;
            else
                crc >>= 1;
            }
        crc_table[i] = crc;
        }
}


guint32 file_cksum ( char * fname ) { 
    FILE * f;
    guint32 crc;
    int count;
    int ch;
    
    if (!crc_table)
        gen_crc_table();

    if (!(f = fopen(fname, "r"))) {
         fprintf(stderr, "Unable to open file %s\n", fname);
         return 0;
    } 
    
    for (crc = 0xFFFFFFFF, count = 0; 
         (count < CRC_BYTES) && ((ch = getc(f)) != EOF);
         count++) {
        crc = ((crc>>8) & 0x00FFFFFF) ^ crc_table[ (crc^ch) & 0xFF ];
    }
    return crc;
}



/* Add the song to the list according to the current sort order */
gint song_add ( GList ** song_list, song * s) {
    GList * current;
    int i;
    GCompareFunc func;

    switch (prefs.sort) {
    case ARTIST_TITLE:
        func = sort_artist_title;
        break;
    case COLOR:
        func = sort_color;
        break;
    case BPM:
        func = sort_bpm;
        break;
    case RATING:
        func = sort_rating;
        break;
    case FREQ:
        func = sort_freq;
    }
    assert(s);
    *song_list = g_list_insert_sorted ( *song_list, s, func );
    for (current = *song_list, i = 0; 
         current;
         i++, current = g_list_next(current)) {
        if (s == SONG(current))
            return i;
    }
    assert(FALSE);
    return 0;
}

void sort ( GList ** list ) {
    GCompareFunc func;

    switch (prefs.sort) {
    case ARTIST_TITLE:
        func = sort_artist_title;
        break;
    case COLOR:
        func = sort_color;
        break;
    case BPM:
        func = sort_bpm;
        break;
    case RATING:
        func = sort_rating;
        break;
    case FREQ:
        func = sort_freq;
    }
    *list = g_list_sort(*list, func);
}

gboolean correct_sort ( GList * item) {
    GList * prev, * next;
    GCompareFunc func;

    switch (prefs.sort) {
    case ARTIST_TITLE:
        func = sort_artist_title;
        break;
    case COLOR:
        func = sort_color;
        break;
    case BPM:
        func = sort_bpm;
        break;
    case RATING:
        func = sort_rating;
        break;
    case FREQ:
        func = sort_freq;
    }

    prev = g_list_previous(item);
    next = g_list_next(item);
    
    if (prev && (func(SONG(prev), SONG(item)) > 0))
            return FALSE;
    if (next && (func(SONG(item), SONG(next)) > 0))
            return FALSE;
    return TRUE;
}


gint sort_artist_title ( gconstpointer a, gconstpointer b) {
    gint result;
    gchar * a_artist, * b_artist, * a_title, * b_title;
    song * s_a = (song *) a;
    song * s_b = (song *) b;
    
    a_artist = s_a->artist;
    b_artist = s_b->artist;

    if (strncasecmp(a_artist, "the ", 4) == 0)
        a_artist += 4;
    if (strncasecmp(b_artist, "the ", 4) == 0)
        b_artist += 4;
    result = strcasecmp(a_artist, b_artist);
    if (result == 0) {
        a_title = strcmp(s_a->title, "?") ? s_a->title: s_a->fname;
        b_title = strcmp(s_b->title, "?") ? s_b->title: s_b->fname;
        if (strncasecmp(a_title, "the ", 4) == 0)
            a_title += 4;
        else if (strncasecmp(a_title, "a ", 2) == 0)
            a_title += 2;
        if (strncasecmp(b_title, "the ", 4) == 0)
            b_title += 4;
        else if (strncasecmp(b_title, "a ", 2) == 0)
            b_title += 2;

        result = strcasecmp(a_title, b_title);
    }
    return result;
}

gint sort_rating ( gconstpointer a, gconstpointer b) {
    song * s_a = (song *) a;
    song * s_b = (song *) b;
    if (s_a->rating < s_b->rating)
        return 1;
    if (s_a->rating == s_b->rating)
        return sort_artist_title(a, b);
    return -1;
}


gint sort_bpm ( gconstpointer a, gconstpointer b) {
    song * s_a = (song *) a;
    song * s_b = (song *) b;

    if (s_a->flags & BPM_UNDEF) {
        if (s_b->flags & BPM_UNDEF) 
            return sort_artist_title(a, b);
        return 1;
    }
    if (s_b->flags & BPM_UNDEF)
        return -1;
    if (s_a->flags & BPM_UNK) {
        if (s_b->flags & BPM_UNK) 
            return sort_artist_title(a, b);
        return -1;
    }
    if (s_b->flags & BPM_UNK)
        return 1;
    
    if (s_a->bpm < s_b->bpm)
        return -1;
    if (s_a->bpm == s_b->bpm)
        return sort_artist_title(a, b);
    return 1;
}


#define BRIGHTNESS_SORT .8

gint sort_color ( gconstpointer a, gconstpointer b) {
    song * s_a = (song *) a;
    song * s_b = (song *) b;
    
    
    if (s_a->flags & COLOR_UNK) {
        if (s_b->flags & COLOR_UNK) 
            return sort_artist_title(a, b);
        return -1;
    }
    if (s_b->flags & COLOR_UNK)
        return 1;
    if ((s_a->color.H + BRIGHTNESS_SORT * s_a->color.B) <
        (s_b->color.H + BRIGHTNESS_SORT * s_b->color.B)) {
        return -1;
    }
    if ((s_a->color.H + BRIGHTNESS_SORT * s_a->color.B) ==
        (s_b->color.H + BRIGHTNESS_SORT * s_b->color.B)) {
        return 0;
    }
    return 1;
}



gint sort_freq ( gconstpointer a, gconstpointer b) {
    song * s_a = (song *) a;
    song * s_b = (song *) b;
    gdouble a_sum, b_sum, ideal, a_val, b_val;
    gint i;
    
    if (s_a->flags & FREQ_UNK) {
        if (s_b->flags & FREQ_UNK) 
            return sort_artist_title(a, b);
        return -1;
    }
    if (s_b->flags & FREQ_UNK)
        return 1;

    for (i = 0, a_sum = 0, b_sum = 0; i < NUM_FREQ_SAMPLES; i++) {
        ideal = ideal_freq(i);
        a_val = (s_a->freq[i] - ideal)/ideal;
        b_val = (s_b->freq[i] - ideal)/ideal;
        if (a_val > 0)
            a_sum += a_val;
        else
            b_sum -= a_val;
        if (b_val > 0)
            b_sum += b_val;
        else
            a_sum -= b_val;
    }

    if (a_sum < b_sum) 
        return -1;
    if (a_sum > b_sum)
        return 1;
    return 0;
}




HSV average_color ( GList * list ) {
    RGB rgb;
    gdouble r, g, b;
    gdouble num_songs = g_list_length(list);
    r = g = b = 0;
    for (;list; list = g_list_next(list)) {
        rgb = song_get_rgb(SONG(list));
        r += rgb.R;
        g += rgb.G;
        b += rgb.B;
    }
    rgb.R = r / num_songs;
    rgb.G = g / num_songs;
    rgb.B = b / num_songs;
    return(rgb_to_hsv(rgb));
}

gdouble average_rating ( GList * list ) {
    gdouble result = 0;
    gdouble num_songs = g_list_length(list);
    for (;list; list = g_list_next(list)) {
        result += SONG(list)->rating;
    }
    return result / num_songs;
}


GList * songs_by_artist ( GList * list, gchar * artist) {
    GList * new;
    for (new = NULL; list; list = g_list_next(list)) {
        if (strcasecmp(SONG(list)->artist, artist) == 0) {
            new = g_list_append(new, SONG(list));
        }
    }
    return new;
}

