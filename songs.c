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
#include <endian.h>
#include <errno.h>
#include "gjay.h"
#include "analysis.h"


gint    num_songs;

/* We track 3 types of file: files which are songs (analyzed), files
 * which have been rated but not analyzed, and files which are not
 * songs. We keep both a list and hash (on path)
 */
GList * songs;   
GList * rated;
GList * files_not_song;

GHashTable * song_name_hash;
GHashTable * rated_name_hash;
GHashTable * files_not_song_hash;

static void     write_data (FILE * f, 
                            char * fname, 
                            song * s);
static void     write_attr (FILE * f, 
                            song * s);
static gboolean read_data  (FILE * f);

/* Test whether a file is an ogg/mp3/wav; if so, create a song */
static song *  test_ogg_create ( char * fname );
static song *  test_mp3_create ( char * fname );
static song *  test_wav_create ( char * fname );


typedef enum {
    TOKEN_SONG = 0,
    TOKEN_NOT_SONG,
    TOKEN_END_OF_SONG,
    TOKEN_CHECKSUM,
    TOKEN_TITLE,
    TOKEN_ARTIST,
    TOKEN_ALBUM,
    TOKEN_BPM,
    TOKEN_FREQ,
    TOKEN_VOL_DIFF,
    TOKEN_LENGTH,
    TOKEN_COLOR,
    TOKEN_RATING,
    TOKEN_BPM_UNDEF,
    TOKEN_NO_COLOR,
    TOKEN_NO_RATING,
    TOKEN_NO_DATA,
    TOKEN_LAST
} song_tokens;


char * song_token[TOKEN_LAST] = {
    "SONG",
    "NOTSONG",
    "EOS",
    "CKSUM",
    "TITLE",
    "ARTIST",
    "ALBUM",
    "BPM",
    "FREQ",
    "VOL_DIFF",
    "LEN",
    "COLOR",
    "RATING",
    "BPM_UNDEF",
    "NO_COLOR",
    "NO_RATING",
    "NO_DATA"
};


/**
 * Read the main GJay data file and, if present, the daemon's analysis
 * data.
 */
#define NUM_DATA_FILES 2
void read_data_file ( void ) {
    char buffer[BUFFER_SIZE];
    char * files[NUM_DATA_FILES] = { GJAY_FILE_DATA, GJAY_DAEMON_DATA };
    FILE * f;
    gint k;

    for (k = 0; k < NUM_DATA_FILES; k++) {
        snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
                 GJAY_DIR, files[k]);
        f = fopen(buffer, "r");
        if (f) {
            while (!feof(f)) 
                read_data(f);
            fclose(f);
        }
    }
}


/**
 * The attr file is log-structured
 */
void read_attr_file ( void ) {
    char buffer[BUFFER_SIZE], token[128];
    FILE * f;
    song * s = NULL;
    gchar * fname;
    int k;
    float fl1, fl2;

    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_FILE_ATTR);
    f = fopen(buffer, "r");
    if (f) {
        while (!feof(f) && fscanf(f, "%s", token)) {
            for (k = 0; k < TOKEN_LAST; k++) {
                if (strcmp(token, song_token[k]) == 0)
                    break;
            }
            if (strlen(token) == 0)
                continue;
            switch (k) {
            case TOKEN_SONG:
                fscanf(f, " ");
                read_line(f, buffer, BUFFER_SIZE); 
                s = g_hash_table_lookup(song_name_hash, buffer);
                if (!s) {
                    if (g_hash_table_lookup(files_not_song_hash, buffer))
                        break;
                    s = g_hash_table_lookup(rated_name_hash, buffer);
                    if (!s) {
                        /* This file has been rated but not analyzed */
                        s = create_song_from_file(buffer);
                        if (!s) {
                            /* Not a song */
                            fname = g_strdup(buffer);
                            files_not_song = g_list_append(files_not_song, 
                                                           fname);
                            g_hash_table_insert (files_not_song_hash,
                                                 fname,
                                                 (int *) TRUE);
                            explore_update_file_pm(fname, PM_FILE_NOSONG);
                            break;
                        } else {
                            rated = g_list_append(rated, s);
                            g_hash_table_insert (rated_name_hash,
                                                 s->path,
                                                 s);
                            /* Not enough information to update the explore
                             * view window */
                        }
                    }
                    s->no_data = TRUE;
                }
                break;
            case TOKEN_END_OF_SONG:
                fscanf(f, "\n");
                s = NULL;
                break;
            case TOKEN_COLOR:
                s->no_color = FALSE;
                fscanf(f, " %f %f\n", &fl1, &fl2);
                if (s) {
                    s->color.H = fl1;
                    s->color.B = fl2;
                }
                break;
            case TOKEN_RATING:
                s->no_rating = FALSE;
                fscanf(f, " %f\n", &fl1);
                if (s)
                    s->rating = fl1;
                break;
            default:
                fprintf(stderr, "Unable to parse token\n");
                break;
            }
        }
        fclose(f);
    }
}


/**
 * Read a song/file info from the file at the seek position, add to the
 * songs list. Return TRUE if the songs list was updated.
 */
gboolean add_from_daemon_file_at_seek (int seek) {
    char buffer[BUFFER_SIZE];
    gboolean result;
    FILE * f;

    result = FALSE;
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_DAEMON_DATA);
    f = fopen(buffer, "r");
    if (f) {
        fseek(f, seek, SEEK_SET);
        result = read_data(f);
        fclose(f);
    }
    return result;
}


gboolean read_data (FILE * f) {
    char buffer[BUFFER_SIZE], token[128];
    int k, checksum;
    song * s = NULL;
    gchar * fname;

    while (fscanf(f, "%s", token)) {
        for (k = 0; k < TOKEN_LAST; k++) {
            if (strcmp(token, song_token[k]) == 0)
                break;
        }
        switch (k) {
        case TOKEN_SONG:
            fscanf(f, " ");
            read_line(f, buffer, BUFFER_SIZE);
            /**
             * If this song has already been rated, remove from 
             * rating list 
             */
            s = g_hash_table_lookup(rated_name_hash, buffer);
            if (s) {
                rated = g_list_remove(rated, s);
                g_hash_table_remove(rated_name_hash, s->path);
            } else {
                s = create_song(buffer);
            }
            break;
        case TOKEN_NOT_SONG:
            fscanf(f, " ");
            read_line(f, buffer, BUFFER_SIZE);
            if (g_hash_table_lookup(files_not_song_hash, buffer) == NULL) {
                fname = g_strdup(buffer);
                files_not_song = g_list_append(files_not_song, fname);
                g_hash_table_insert (files_not_song_hash,
                                     fname,
                                     (int *) TRUE);
                explore_update_file_pm(fname, PM_FILE_NOSONG);
            }
            return FALSE;
        case TOKEN_END_OF_SONG:
            fscanf(f, "\n");
            if (g_hash_table_lookup(song_name_hash, s->path) == NULL) {
                num_songs++;
                songs = g_list_append(songs, s);
                g_hash_table_insert (song_name_hash,
                                     s->path,
                                     s);
                if (explore_update_file_pm(s->path, PM_FILE_SONG))
                    s->marked = TRUE;
            } else {
                printf("TOSSING!\n");
                /* Already in list */
                delete_song(s);
                return FALSE;
            }
            return TRUE;
        case TOKEN_CHECKSUM:
            /* Fixme */
            fscanf(f, " %d\n", &checksum);
            break;
        case TOKEN_TITLE:
            fscanf(f, " ");
            read_line(f, buffer, BUFFER_SIZE);
            s->title = g_strdup(buffer);
            break;
        case TOKEN_ARTIST:
            fscanf(f, " ");
            read_line(f, buffer, BUFFER_SIZE);
            s->artist = g_strdup(buffer);
            break;
        case TOKEN_ALBUM:
            fscanf(f, " ");
            read_line(f, buffer, BUFFER_SIZE);
            s->album = g_strdup(buffer);
            break;
        case TOKEN_BPM:
            fscanf(f, " %lf\n", &s->bpm);
            break;
        case TOKEN_BPM_UNDEF:
            fscanf(f, "\n");
            s->bpm_undef = TRUE;
            break;
        case TOKEN_FREQ:
            s->no_data = FALSE;
            for (k = 0; k < NUM_FREQ_SAMPLES; k++) {
                fscanf(f, " %lf", &s->freq[k]);
            }
            fscanf(f, "\n");
            break;
        case TOKEN_VOL_DIFF:
            fscanf(f, " %lf\n", &s->volume_diff);
            break;
        case TOKEN_LENGTH:
            fscanf(f, " %ud\n", (unsigned int *) &s->length);
            break;
        default:
            fprintf(stderr, "Unknown token %s\n", token);
            if (s)
                delete_song(s);
            return FALSE;
        }
    }
    return FALSE;
}


void write_data_file(void) {
    char buffer[BUFFER_SIZE], buffer_temp[BUFFER_SIZE];
    FILE * f;
    GList * llist;

    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_FILE_DATA);
    snprintf(buffer_temp, BUFFER_SIZE, "%s_temp", buffer);
    
    f = fopen(buffer_temp, "w");
    if (f) {
        for (llist = g_list_first(songs); llist; llist = g_list_next(llist))
            write_data(f, NULL, (song *) llist->data);
        for (llist = g_list_first(files_not_song); 
             llist; llist = g_list_next(llist))
            write_data(f, (char *) llist->data, NULL);
        fclose(f);
        rename(buffer_temp, buffer);
    }
}


void write_attr_file(void) {
    char buffer[BUFFER_SIZE], buffer_temp[BUFFER_SIZE];
    FILE * f;
    GList * llist;

    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_FILE_ATTR);
    snprintf(buffer_temp, BUFFER_SIZE, "%s_temp", buffer);
    
    f = fopen(buffer_temp, "w");
    if (f) {
        for (llist = g_list_first(songs); llist; llist = g_list_next(llist)) 
            write_attr(f, (song *) llist->data);
        for (llist = g_list_first(rated); llist; llist = g_list_next(llist))
            write_attr(f, (song *) llist->data);
        fclose(f);
        rename(buffer_temp, buffer);
    }
}
     

/**
 * Write to the data file. If song s is NULL, assume the file in
 * question is not a song.
 */
static void write_data (FILE * f, 
                        char * fname, 
                        song * s) {
    int k;
    if (s) {
        fprintf(f, "%s %s\n", song_token[TOKEN_SONG], s->path);
/*       fprintf(f, "%s %d\n", song_token[TOKEN_CHECKSUM], s->checksum); */
        if (s->title)
            fprintf(f, "%s %s\n", song_token[TOKEN_TITLE], s->title);
        if (s->artist)
            fprintf(f, "%s %s\n", song_token[TOKEN_ARTIST], s->artist);
        if (s->album)
            fprintf(f, "%s %s\n", song_token[TOKEN_ALBUM], s->album);
        fprintf(f, "%s %d\n", song_token[TOKEN_LENGTH], s->length);
        fprintf(f, "%s %f\n", song_token[TOKEN_BPM], s->bpm);
        if (s->bpm_undef)
            fprintf(f, "%s\n", song_token[TOKEN_BPM_UNDEF]);
        fprintf(f, "%s %f\n", song_token[TOKEN_VOL_DIFF], s->volume_diff);
        
        fprintf(f, "%s ", song_token[TOKEN_FREQ]);
        for (k = 0; k < NUM_FREQ_SAMPLES; k++) {
            fprintf(f, "%f", s->freq[k]);
            if (k == NUM_FREQ_SAMPLES - 1)
                fprintf(f, "\n");
            else 
                fprintf(f, " ");
        }
        fprintf(f, "%s\n", song_token[TOKEN_END_OF_SONG]);
    } else {
        fprintf(f, "%s %s\n", song_token[TOKEN_NOT_SONG], fname);
    }
}


/**
 * Write to the attr file.
 */
static void write_attr (FILE * f, 
                        song * s) {
    if (s->no_color && s->no_rating)
        return;
    fprintf(f, "%s %s\n", song_token[TOKEN_SONG], s->path);
    if (!s->no_color) {
        fprintf(f, "%s %f %f\n", 
                song_token[TOKEN_COLOR], 
                s->color.H, s->color.B);
    }
    if (!s->no_rating) {
        fprintf(f, "%s %f\n", song_token[TOKEN_RATING], s->rating);
    }
    fprintf(f, "%s\n", song_token[TOKEN_END_OF_SONG]);
}



/**
 * Append to the daemon data file. If song s is NULL, assume this file
 * is not a song. 
 * 
 * Return the file seek position of the start of the song/file, or -1 if
 * error
 */
int append_daemon_file (char * fname, song * s) {
    char buffer[BUFFER_SIZE];
    FILE * f;
    int file_seek; /* Get the seek position before the write */
    
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_DAEMON_DATA);
    f = fopen(buffer, "a");
    if (f) {
        file_seek = ftell(f);
        write_data(f, fname, s);
        fclose(f);
        return file_seek;
    } else {
        fprintf(stderr, "Error: unable to write %s.\nAnalysis for %s was skipped!\n", buffer, s->path);
    }
    return -1;
}


/**
 * Append to the attr file 
 */
void append_attr_file (song * s) {
    char buffer[BUFFER_SIZE];
    FILE * f;
    
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_FILE_ATTR);
    f = fopen(buffer, "a");
    if (f) {
        write_attr(f, s);
        fclose(f);
    } else {
        fprintf(stderr, "Error: unable to write %s.\nAnalysis for %s was skipped!\n", buffer, s->path);
    }
}



/**
 * Create a new song from a file. Return NULL if file invalid, not an
 * mp3, ogg, or wav, or if the song is already in the list. 
 */
song * create_song_from_file ( gchar * fname ) {
    song * s = NULL;
    
    if (access(fname, R_OK)) 
        return NULL;

    /* FIXME: add file checksum */

    s = test_ogg_create(fname);
    if (!s) 
        s = test_wav_create(fname);
    if (!s) 
        s = test_mp3_create(fname);
    if (s) {
        if (!s->title)
            s->title = g_strdup ("?");
        if (!s->artist)
            s->artist = g_strdup ("?");
        if (!s->album)
            s->album = g_strdup ("?");
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
    /*   printf("Checksum: %lu\n", (long unsigned int) s->checksum); */
    printf("Hue: %f\n", s->color.H);
    printf("Brightness: %f\n", s->color.B);
    printf("Rating: %f\n", s->rating);
    printf("BPM: %f\n", s->bpm);
    printf("No data: %d\n", s->no_data);
    if (s->no_color) 
        printf("No color attr\n");
    if (s->no_rating) 
        printf("No rating attr\n");
    printf("Freq: ");
    for (i = 0; i < NUM_FREQ_SAMPLES; i++) {
        printf("%.3f ", s->freq[i]);
    }
    printf("\n");
}


void delete_song (song * s) {
    if(s->freq_pixbuf)
        gdk_pixbuf_unref(s->freq_pixbuf);
    if(s->color_pixbuf)
        gdk_pixbuf_unref(s->color_pixbuf);
    g_free(s->path);
    g_free(s->title);
    g_free(s->artist);
    g_free(s->album);
    g_free(s);
}


/* Create a new song with the given filename */
song * create_song ( char * fname ) {
    int i;
    song * s;
    
    s = g_malloc(sizeof(song));
    memset (s, 0x00, sizeof(song));
    s->no_color = TRUE;
    s->no_rating = TRUE;
    s->no_data = TRUE;
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


static song * test_mp3_create ( char * fname ) {
     FILE * f;
     char buffer[BUFFER_SIZE];
     char quoted_fname[BUFFER_SIZE];
     song * s = NULL;

     quote_path(quoted_fname, BUFFER_SIZE, fname);
     snprintf(buffer, BUFFER_SIZE, 
              "mp3info -p \"frames:%%u\\n%%a\\n%%t\\n%%l\\n%%S\" \'%s\'",
              quoted_fname);
     if (!(f = popen(buffer, "r"))) {
         fprintf(stderr, "Unable to run %s\n", buffer);
         return NULL;
     } 
     while (!feof(f) && !s) {
         read_line(f, buffer, BUFFER_SIZE);
         if (strncmp(buffer, "frames:", strlen("frames:")) == 0) {
             if (strlen(buffer) > strlen("frames:")) {
                 s = create_song(fname);
             }
         }
     }
     if (!feof(f) && s) {
         read_line(f, buffer, BUFFER_SIZE);
         if (strlen(buffer)) {
             s->artist = g_strdup(buffer);
         }
         read_line(f, buffer, BUFFER_SIZE);
         if (strlen(buffer)) {
             s->title = g_strdup(buffer);
         }
         read_line(f, buffer, BUFFER_SIZE);
         if (strlen(buffer)) {
             s->album = g_strdup(buffer);
         }
         read_line(f, buffer, BUFFER_SIZE);
         if (strlen(buffer)) {
             s->length = atoi(buffer);
         }
     }
     pclose(f);
     return s;
}


static song * test_ogg_create ( char * fname ) { 
    FILE * f;
    song * s = NULL;
    int i;
    OggVorbis_File vf;
    vorbis_comment * vc;

    f = fopen(fname, "r");
    if (!f) 
        return NULL;
    
    if(ov_open(f, &vf, NULL, 0) == 0) {
        s = create_song(fname);
        vc = ov_comment(&vf, -1);
        s->length = ov_time_total(&vf, -1);
                
        for (i = 0; i < vc->comments; i++) {
            if (strncasecmp(vc->user_comments[i], "title=", strlen("title=")) == 0) {
                s->title = g_strdup(vc->user_comments[i] + strlen("title="));
            } else if (strncasecmp(vc->user_comments[i], "artist=", strlen("artist=")) == 0) {
                s->artist = g_strdup(vc->user_comments[i] + strlen("artist="));
            } else if (strncasecmp(vc->user_comments[i], "album=", strlen("album=")) == 0) {
                s->album = g_strdup(vc->user_comments[i] + strlen("album="));
            }
        }
        ov_clear(&vf);
    } else {
        fclose(f);
    }
    return s;
}


static song * test_wav_create ( char * fname ) {
    FILE * f;
    song * s = NULL;
    struct stat buf;
    waveheaderstruct header;

    f = fopen(fname, "r");
    if (!f)
        return NULL;
    fread(&header, sizeof(waveheaderstruct), 1, f);
    wav_header_swab(&header);
    fclose(f);
    if ((strncmp(header.chunk_type, "WAVE", 4) == 0) &&
        (header.byte_p_spl / header.modus == 2)) {
        s = create_song(fname);
        stat(fname, & buf);
        s->length = (buf.st_size - sizeof(waveheaderstruct)) / 176758;
    } 
    return s;
}


void sort ( GList ** list ) {
    GCompareFunc func;

    /* FIXME */
#if 0
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
#endif
    func = sort_artist_title;
    *list = g_list_sort(*list, func);
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

    if (s_a->bpm_undef) {
        if (s_b->bpm_undef)
            return sort_artist_title(a, b);
        return -1;
    } 
    if (s_b->bpm_undef)
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
    
    
    if (s_a->no_color) {
        if (s_b->no_color) 
            return sort_artist_title(a, b);
        return -1;
    }
    if (s_b->no_color) 
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
    gdouble a_sum, b_sum, a_val, b_val;
    gint i;
    
    for (i = 0, a_sum = 0, b_sum = 0; i < NUM_FREQ_SAMPLES; i++) {
        a_val = s_a->freq[i];
        b_val = s_b->freq[i];
        if (a_val > b_val)
            a_sum += a_val;
        else if (a_val < b_val)
            b_sum += b_val;
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
        rgb = hsv_to_rgb(hb_to_hsv(SONG(list)->color));
        r += rgb.R;
        g += rgb.G;
        b += rgb.B;
    }
    rgb.R = r / num_songs;
    rgb.G = g / num_songs;
    rgb.B = b / num_songs;
    return(rgb_to_hsv(rgb));
}


/**
 * If the song does not have a pixbuf for its frequency, and it has been 
 * analyzed, create a pixbuf for its frequency. 
 */
void song_set_freq_pixbuf ( song * s) {
    guchar * data;
    int x, y, sample, offset, strength, rowstride;
    guchar r, g, b;

    assert(s);
    if (s->freq_pixbuf)
        return;
    if (s->no_data)
        return;
    
    s->freq_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                    FALSE,
                                    8,
                                    FREQ_IMAGE_WIDTH,
                                    FREQ_IMAGE_HEIGHT);
    rowstride = gdk_pixbuf_get_rowstride(s->freq_pixbuf);
    data = gdk_pixbuf_get_pixels (s->freq_pixbuf);
    
    for (x = 0; x < NUM_FREQ_SAMPLES; x++) {
        g = b = MIN(255, 255.0 * 1.8 * sqrt((double) s->freq[x]));
        r = MIN(255, 255.0 * 2.0 * s->freq[x]);
        for (y = 0; y <  FREQ_IMAGE_HEIGHT; y++) {
            offset = rowstride * y + 3*x;
            data[offset] = r;
            data[offset + 1] = g;
            data[offset + 2] = b;
        }
    }
}


/**
 * If the song does not have a pixbuf for its frequency, and it has been 
 * analyzed, create a pixbuf for its frequency. 
 */
void song_set_color_pixbuf ( song * s) {
    guchar * data;
    int x, y, offset, rowstride;
    guchar r, g, b;
    RGB rgb;

    assert(s);
    if (s->color_pixbuf)
        return;
    if (s->no_color)
        return;
    
    s->color_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                     FALSE,
                                     8,
                                     COLOR_IMAGE_WIDTH,
                                     COLOR_IMAGE_HEIGHT);
    rowstride = gdk_pixbuf_get_rowstride(s->color_pixbuf);
    data = gdk_pixbuf_get_pixels (s->color_pixbuf);
    rgb = hsv_to_rgb(hb_to_hsv(s->color));    
    r = rgb.R * 255;
    g = rgb.G * 255;
    b = rgb.B * 255;
    for (x = 0; x < COLOR_IMAGE_WIDTH; x++) {
        for (y = 0; y <  COLOR_IMAGE_HEIGHT; y++) {
            offset = rowstride * y + 3*x;
            data[offset] = r;
            data[offset + 1] = g;
            data[offset + 2] = b;
        }
    }
}


/* FIXME -- use file checksum for file uniqueness */
#if 0
#define CRC_TABLE_SIZE 256
#define CRC_BYTES 50*1024
static guint32 * crc_table = NULL;

/* Utilities */
static void gen_crc_table(void); /* For CRC32 checksum */
static guint32 file_cksum ( char * fname );


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
#endif
