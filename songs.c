/**
 * GJay, copyright (c) 2002-2004 Chuck Groom
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
#include <endian.h>
#include <errno.h>
#include <math.h> 
#include <ctype.h>
#include "gjay.h"
#include "gjay_xmms.h"
#include "analysis.h"
#include "mp3.h"
#include "vorbis.h"


typedef enum {
    E_GJAY_DATA = 0,
    E_FILE,
    E_TITLE,
    E_ARTIST,
    E_ALBUM,
    E_INODE,
    E_DEV,
    E_LENGTH,
    E_RATING,
    E_COLOR,
    E_FREQ,
    E_BPM,    
    /* Attributes */
    E_PATH,
    E_NOT_SONG,
    E_REPEATS,
    E_VOL_DIFF,
    E_TYPE,
    E_VERSION,
    E_LAST
} element_type;


static const char * element_str[E_LAST] = {
    "gjay_data",
    "file",
    "title",
    "artist",
    "album",
    "inode",
    "dev",
    "length",
    "rating",
    "color",
    "freq",
    "bpm",
    "path",
    "not_song",
    "repeats",
    "volume_diff",
    "type",
    "version"
};


typedef struct {
    gboolean is_repeat;
    gboolean not_song;
    gboolean use_hb;
    gboolean new;
    gboolean has_dev;
    element_type element;
    song * s;
} song_parse_state;



GList      * songs;       /* List of song ptrs  */
GList      * not_songs;   /* List of char *, UTF8 encoded */
gboolean     songs_dirty;

GHashTable * song_name_hash; 
GHashTable * song_inode_dev_hash;
GHashTable * not_song_hash;


static void     write_song_data     ( FILE * f, song * s );
static void     write_not_song_data ( FILE * f, gchar * path );
static gboolean read_song_file_type ( char * path, 
                                      song_file_type type,
                                      gint   * length,
                                      gchar ** title,
                                      gchar ** artist,
                                      gchar ** album );
static gboolean read_data           ( FILE * f );
static void     data_start_element  ( GMarkupParseContext *context,
                                      const gchar         *element_name,
                                      const gchar        **attribute_names,
                                      const gchar        **attribute_values,
                                      gpointer             user_data,
                                      GError             **error );
static void     data_end_element    ( GMarkupParseContext *context,
                                      const gchar         *element_name,
                                      gpointer             user_data,
                                      GError             **error );
static void     data_text           ( GMarkupParseContext *context,
                                      const gchar         *text,
                                      gsize                text_len,  
                                      gpointer             user_data,
                                      GError             **error );
static int      get_element         ( gchar * element_name );
static void     song_copy_attrs     ( song * dest, 
                                      song * original );

/* Create a new song with the given filename */
song * create_song ( void ) {
    song * s;
    
    s = g_malloc(sizeof(song));
    memset (s, 0x00, sizeof(song));
    s->no_color = TRUE;
    s->no_rating = TRUE;
    s->no_data = TRUE;
    s->access_ok = TRUE;
    s->rating = (MIN_RATING + MAX_RATING)/2;
    return s;
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


song * song_set_path ( song * s, 
                       char * path ) {
    int i;
    free(s->path);
    s->path = g_strdup(path);
    s->fname = s->path;
    for (i = strlen(s->path) - 1; i; i--) {
        if (s->path[i] == '/') {
            s->fname = s->path + i + 1;
            return s;
        }
    }
    return s;
}


/**
 * If the song does not have a pixbuf for its frequency, and it has been 
 * analyzed, create a pixbuf for its frequency. 
 */
void song_set_freq_pixbuf ( song * s) {
    guchar * data;
    int x, y, offset, rowstride;
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
        g = b = MIN(255, 255.0 * 1.8 * (float) sqrt((double) s->freq[x]));
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
    rgb = hsv_to_rgb(s->color);    
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


/**
 * The song "s" repeats the song "original". It should copy the same info, 
 * but be marked as a copy.
 */
void song_set_repeats ( song * s, song * original ) {
    char * path, * fname;
    song * ll;

    path = s->path;
    fname = s->fname;

    g_free(s->artist);
    g_free(s->title);
    g_free(s->album);
    
    memcpy(s, original, sizeof(song));
    if (original->title)
        s->title = g_strdup(original->title);
    if (original->album)
        s->album = g_strdup(original->album);
    if (original->artist)
        s->artist = g_strdup(original->artist);
    s->path = path;
    s->fname = fname;
    s->repeat_prev = NULL;
    s->repeat_next = NULL;
    s->freq_pixbuf = NULL;
    s->color_pixbuf = NULL;

    for (ll = original; ll->repeat_next; ll = ll->repeat_next)
        ;
    ll->repeat_next = s;
    s->repeat_prev = ll;
}


/**
 * Copy the song's attributes to all other songs which repeat it.
 */
void song_set_repeat_attrs( song * s) {
    song * repeat;
    for (repeat = s->repeat_prev; repeat; repeat = repeat->repeat_prev)
        song_copy_attrs(repeat, s);
    for (repeat = s->repeat_next; repeat; repeat = repeat->repeat_next)
        song_copy_attrs(repeat, s);
}


static void song_copy_attrs( song * dest, song * original ) {
    dest->bpm = original->bpm;
    dest->rating = original->rating;
    dest->color = original->color;
    memcpy(&dest->freq, &original->freq, sizeof(gdouble) * NUM_FREQ_SAMPLES);
    dest->no_data = original->no_data;
    dest->no_rating = original->no_rating;
    dest->no_color = original->no_color;
    dest->bpm_undef = original->bpm_undef;
    dest->volume_diff = original->volume_diff;
    dest->marked = original->marked;
}

/**
 * Collect information about a path. 
 * IN:     Path
 * RETURN: Set all other attributes if it is a song, otherwise set is_song
 *         to false
 *
 * Note that we expect the path to be UTF8
 */
void file_info ( gchar    * path,
                 gboolean * is_song,
                 guint32  * inode,
                 guint32  * dev,
                 gint     * length,
                 gchar   ** title,
                 gchar   ** artist,
                 gchar   ** album, 
                 song_file_type * type ) {
    gchar * latin1_path;
    struct stat buf;
    
    *is_song = FALSE;
    *inode = 0;
    *dev = 0;
    *length = 0;
    *artist = NULL;
    *title = NULL;
    *album = NULL;

    if (verbosity > 1) {
        printf("Scanning %s...\n", path);
    }

    latin1_path = strdup_to_latin1(path);
    if (stat(latin1_path, &buf)) {
        g_free(latin1_path);
        return;
    } 
    
    *dev = buf.st_dev;
    *inode = buf.st_ino;

    for (*type = OGG; *type < SONG_FILE_TYPE_LAST; (*type)++) {
        if (read_song_file_type(latin1_path, *type, 
                                length, title, artist, album)) {
            *is_song = TRUE;
            g_free(latin1_path);
            return;
        }
    }
    g_free(latin1_path);
}


void write_data_file(void) {
    char buffer[BUFFER_SIZE], buffer_temp[BUFFER_SIZE];
    FILE * f;
    GList * llist, * w_songs = NULL;
    song * s;

    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_FILE_DATA);
    snprintf(buffer_temp, BUFFER_SIZE, "%s_temp", buffer);

    /* Cull songs which are no longer there */
    for (llist = g_list_first(songs); llist; llist = g_list_next(llist)) {
        s = SONG(llist);
        if (!s->access_ok) {
            if (s->repeat_prev)
                s->repeat_prev->repeat_next = s->repeat_next;
            if (s->repeat_next)
                s->repeat_next->repeat_prev = s->repeat_prev;
        } else {
            w_songs = g_list_append(w_songs, s);
        }
    }
    
    f = fopen(buffer_temp, "w");
    if (f) {
        fprintf(f, "<gjay_data version=\"%s\">\n", GJAY_VERSION);
        for (llist = g_list_first(w_songs); llist; llist = g_list_next(llist))
            write_song_data(f, SONG(llist));
        for (llist = g_list_first(not_songs); 
             llist; llist = g_list_next(llist))
            write_not_song_data(f, (char *) llist->data);
        fprintf(f, "</gjay_data>\n");
        fclose(f);
        rename(buffer_temp, buffer);
        songs_dirty = FALSE;
    } else {
        fprintf(stderr, "Unable to write song data %s\n", buffer_temp);
    }

    g_list_free(w_songs);
}


/**
 * Append song info to the daemon data file.
 * 
 * Return the file seek position of the start of the song/file, or -1 if
 * error
 */
int append_daemon_file (song * s) {
    char buffer[BUFFER_SIZE];
    FILE * f;
    int file_seek; /* Get the seek position before the write */
    
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_DAEMON_DATA);
    f = fopen(buffer, "a");
    if (f) {
        file_seek = ftell(f);
        write_song_data(f, s);
        fclose(f);
        return file_seek;
    } else {
        fprintf(stderr, "Error: unable to write %s.\nAnalysis for %s was skipped!\n", buffer, s->path);
    }
    return -1;
}


/**
 * Write a song to the data file in XML. Note that strings are UTF8-encoded.
 *
 * <file path="path" not_song="t/f" repeats="original_path">
 *   <title>str</title>
 *   <artist>str</artist>
 *   <album>str</album>
 *   <inode>int</inode>
 *   <dev>int</dev>
 *   <length>int</length>
 *   <rating>float</rating>
 *   <color type="hsv">float float float</color>  
 *   <freq volume_diff="float">float float...</freq>
 *   <bpm>float</bpm>
 * </file>
 */
static void write_song_data (FILE * f, song * s) {
    gchar * escape; /* Escape XML elements from text */
    int k;

    assert(s);

    escape = g_markup_escape_text(s->path, strlen(s->path));
    fprintf(f, "<file path=\"%s\"", escape);
    g_free(escape);

    if (s->repeat_prev) {
        escape = g_markup_escape_text(s->repeat_prev->path, 
                                      strlen(s->repeat_prev->path));
        fprintf(f, " repeats=\"%s\"", escape);
        g_free(escape);
    }
    fprintf(f, ">\n");

    if (!s->repeat_prev) {
        if(s->artist) {
            escape = g_markup_escape_text(s->artist, strlen(s->artist));
            fprintf(f, "\t<artist>%s</artist>\n", escape);
            g_free(escape);
        }
        if(s->album) {
            escape = g_markup_escape_text(s->album, strlen(s->album));
            fprintf(f, "\t<album>%s</album>\n", escape);
            g_free(escape);
        }
        if(s->title) {
            escape = g_markup_escape_text(s->title, strlen(s->title));
            fprintf(f, "\t<title>%s</title>\n", escape);
            g_free(escape);
        }
        fprintf(f, "\t<inode>%lu</inode>\n", (long unsigned int) s->inode);
        fprintf(f, "\t<dev>%lu</dev>\n", (long unsigned int) s->dev);
        fprintf(f, "\t<length>%d</length>\n", s->length);
        if(!s->no_data) {
            if (s->bpm_undef)
                fprintf(f, "\t<bpm>undef</bpm>\n");
            else 
                fprintf(f, "\t<bpm>%f</bpm>\n", s->bpm);
            
            fprintf(f, "\t<freq volume_diff=\"%f\">", s->volume_diff);

            for (k = 0; k < NUM_FREQ_SAMPLES; k++) 
                fprintf(f, "%f ", s->freq[k]);
            fprintf(f, "</freq>\n");
        }
        if (!s->no_rating)
            fprintf(f, "\t<rating>%f</rating>\n", s->rating);
        if (!s->no_color)
            fprintf(f, "\t<color type=\"hsv\">%f %f %f</color>\n", 
                    s->color.H,
                    s->color.S,
                    s->color.V);
    }
    fprintf(f, "</file>\n");
}


static void write_not_song_data (FILE * f, gchar * path) {
    gchar * escape; /* Escape XML elements from text */
    assert(path);
    escape = g_markup_escape_text(path, strlen(path));
    fprintf(f, "<file path=\"%s\" not_song=\"t\"></file>\n", escape);
    g_free(escape); 
}


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
        snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", 
                 getenv("HOME"), GJAY_DIR, files[k]);
        if (verbosity) {
            printf("Reading from file %s\n", buffer);
        }
        f = fopen(buffer, "r");
        if (f) {
            read_data(f);
            fclose(f);
        }
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
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", 
             getenv("HOME"), GJAY_DIR, GJAY_DAEMON_DATA);
    f = fopen(buffer, "r");
    if (f) {
        fseek(f, seek, SEEK_SET);
        result = read_data(f);
        if (result) {
            songs_dirty = TRUE;
        }
        fclose(f);
    }
    return result;
}


/**
 * Read file data from the file f to its end 
 */
gboolean read_data (FILE * f ) {
    GMarkupParseContext * parse_context;
    GMarkupParser parser;
    gboolean result = TRUE;
    GError * error;
    char buffer[BUFFER_SIZE];
    gssize text_len;
    song_parse_state state;

    memset(&state, 0x00, sizeof(song_parse_state));
    
    parser.start_element = data_start_element;
    parser.end_element = data_end_element;
    parser.text = data_text;
    
    parse_context = g_markup_parse_context_new(&parser, 0, &state, NULL);
    if (parse_context) {
        while (result && !feof(f)) {
            text_len = fread(buffer, 1, BUFFER_SIZE, f);
            result = g_markup_parse_context_parse ( parse_context,
                                                    buffer,
                                                    text_len,
                                                    &error);
            error = NULL;
        }
        g_markup_parse_context_free(parse_context);
    }
    return result;
}
  
 
/* Called for open tags <foo bar="baz"> */
void data_start_element  (GMarkupParseContext *context,
                          const gchar         *element_name,
                          const gchar        **attribute_names,
                          const gchar        **attribute_values,
                          gpointer             user_data,
                          GError             **error) {
    song_parse_state * state = (song_parse_state *) user_data;
    gchar * path, * repeat_path = NULL;
    song * original;
    element_type element;
    int k;
    
    element = get_element((char *) element_name);
    switch(element) {
    case E_FILE:
        memset(state, 0x00, sizeof(song_parse_state));
        for (k = 0; attribute_names[k]; k++) {
            switch(get_element((char *) attribute_names[k])) {
            case E_PATH:
                path = (gchar *) attribute_values[k];
                break;
            case E_REPEATS:
                repeat_path = (gchar *) attribute_values[k];
                break;   
            case E_NOT_SONG:
                if (*attribute_values[k] == 't') 
                    state->not_song = TRUE;
                break;
            }
        }
        assert(path);
        
        if (state->not_song) {
            if (!g_hash_table_lookup(not_song_hash, path)) {
                state->new = TRUE;
                /* Only keep track of files which still exist */
                if (!access(path, R_OK)) {
                    path = g_strdup(path);
                    not_songs = g_list_append(not_songs, path);
                    g_hash_table_insert ( not_song_hash,
                                          path, 
                                          (gpointer) TRUE);
                } 
            }
            return;
        }
        state->s = g_hash_table_lookup(song_name_hash, path);
        if (!state->s) {
            state->new = TRUE;
            state->s = create_song();
            song_set_path(state->s, path);
        }
        if (repeat_path && (strlen(repeat_path) > 0)) {
            state->is_repeat = TRUE;
            original = g_hash_table_lookup(song_name_hash, repeat_path);
            assert(original);
            song_set_repeats(state->s, original);
        }
        state->s->marked = TRUE; /* Mark all modified or added songs */
        break;
    case E_RATING:
        state->s->no_rating = FALSE;
        break;
    case E_COLOR:
        state->use_hb = TRUE;
        for (k = 0; attribute_names[k]; k++) {
            switch(get_element((char *) attribute_names[k])) {
            case E_TYPE:
                if (strcasecmp(attribute_values[k], "hsv") == 0) {
                    state->use_hb = FALSE;
                }
                break;
            }
        }
        state->s->no_color = FALSE;
        break;
    case E_FREQ:
        for (k = 0; attribute_names[k]; k++) {
            if (get_element((gchar *) attribute_names[k]) == E_VOL_DIFF) { 
                state->s->volume_diff = strtof_gjay(attribute_values[0], NULL);
            }
        }
        /* Fall into next case */
    case E_BPM:
        state->s->no_data = FALSE;
        break;
    default:
        break;
    }
    state->element = element;
}


/* Called for close tags </foo> */
void data_end_element (GMarkupParseContext *context,
                       const gchar         *element_name,
                       gpointer             user_data,
                       GError             **error) {
    gchar * latin1_path;
    song_parse_state * state = (song_parse_state *) user_data;
    if (get_element((char *) element_name) == E_FILE) {
        if (state->new && state->s) {
            /* Check to see if the song is still there */
            latin1_path = strdup_to_latin1(state->s->path);
            state->s->access_ok = !access(latin1_path, R_OK);
            if (!state->s->access_ok) {
                songs_dirty = TRUE;
            }
            g_free(latin1_path);

            /* Add song to song list and hash table */
            songs = g_list_append(songs, state->s);
            g_hash_table_insert (song_name_hash,
                                 state->s->path, 
                                 state->s);           
            hash_inode_dev(state->s, state->has_dev);
            g_hash_table_insert (song_inode_dev_hash,
                                 &state->s->inode_dev_hash,
                                 state->s);
        }
        /* If there is a song and it itself is not copy of another
         * song, check to see if it is the original upon which copies
         * are based and set their attributes */
        if (!(state->not_song || state->is_repeat))  
            song_set_repeat_attrs(state->s);
    }
    state->element = E_LAST;
}


 
void data_text ( GMarkupParseContext *context,
                 const gchar         *text,
                 gsize                text_len,  
                 gpointer             user_data,
                 GError             **error) {
    song_parse_state * state = (song_parse_state *) user_data;
    gchar buffer[BUFFER_SIZE];
    gchar * buffer_str;
    int n;

    memset(buffer, 0x00, BUFFER_SIZE);
    memcpy(buffer, text, text_len);
    
    switch(state->element) {
    case E_TITLE:
        if (state->new) {
            g_free(state->s->title);
            state->s->title = g_strdup(buffer);
        }
        break;
    case E_ARTIST:
        if (state->new) {
            g_free(state->s->artist);
            state->s->artist = g_strdup(buffer);
        }
        break;
    case E_ALBUM:
        if (state->new) {
            g_free(state->s->album);
            state->s->album = g_strdup(buffer);
        }
        break;
    case E_INODE:
        state->s->inode = atol(buffer);
        break;
    case E_DEV:
        state->s->dev = atol(buffer);
        state->has_dev = TRUE;
        break;
    case E_LENGTH:
        state->s->length = atoi(buffer);
        break;
    case E_RATING:
        state->s->rating = strtof_gjay(buffer, NULL);
        break;
    case E_BPM:
        if (strcmp(buffer, "undef") == 0) {
            state->s->bpm_undef = TRUE;
        } else {
            state->s->bpm = strtof_gjay(buffer, NULL);
        }
        break;
    case E_COLOR:
        if (state->use_hb) {
            HB hb;
            hb.H = strtof_gjay(buffer, &buffer_str);
            hb.B = strtof_gjay(buffer_str, NULL);
            state->s->color = hb_to_hsv(hb);
        } else {
            state->s->color.H = strtof_gjay(buffer, &buffer_str);
            state->s->color.S = strtof_gjay(buffer_str, &buffer_str);
            state->s->color.V = strtof_gjay(buffer_str, NULL);
        }
        break;
    case E_FREQ:
        buffer_str = buffer;
        for (n = 0; n < NUM_FREQ_SAMPLES; n++) {
            state->s->freq[n] = strtof_gjay(buffer_str, &buffer_str);
        }
        break;
    default:
        break;
    }
}


static gboolean read_song_file_type ( char         * path, 
                                      song_file_type type,
                                      gint        * length,
                                      gchar       ** title,
                                      gchar       ** artist,
                                      gchar       ** album) {
    FILE * f; 
    int i, result;
    char * vf; // OggVorbis_Files
    vorbis_comment * vc;
    struct stat buf;
    waveheaderstruct header;
    mp3info mp3;
    
    assert(artist && album && title);
    *artist = NULL;
    *album = NULL;
    *title = NULL;
    
    switch (type) {
    case OGG:
        if (!gjay_vorbis_available()) 
            return FALSE;
        f = fopen(path, "r");
        if (!f) 
            return FALSE;
        vf = malloc(2048); /* Much larger than OggVorbis_File struct on
                              64 bit system */
        if(gj_ov_open(f, vf, NULL, 0) == 0) {
            vc = gj_ov_comment(vf, -1);
            *length = gj_ov_time_total(vf, -1);
            for (i = 0; i < vc->comments; i++) {
                if (strncasecmp(vc->user_comments[i], "title=", 
                                strlen("title=")) == 0) {
                    *title = strdup_to_utf8(
                        vc->user_comments[i] + strlen("title="));
                } else if (strncasecmp(vc->user_comments[i], "artist=", 
                                       strlen("artist=")) == 0) {
                    *artist = strdup_to_utf8(
                        vc->user_comments[i] + strlen("artist="));
                } else if (strncasecmp(vc->user_comments[i], "album=", 
                                       strlen("album=")) == 0) {
                    *album = strdup_to_utf8(
                        vc->user_comments[i] + strlen("album="));
                }
            }
            gj_ov_clear(vf);
            free(vf);
            return TRUE;
        }
        fclose(f);
        break;
    case MP3:
        bzero(&mp3, sizeof(mp3info));
        mp3.filename = path;
        mp3.file = fopen(path, "r");
        // returns 0 on success
        result = get_mp3_info(&mp3, SCAN_QUICK, 0);
        if (result == 0) {
            *length = mp3.seconds;
            if (mp3.id3_isvalid) {
                *artist = strdup_to_utf8(mp3.id3.artist);
                *album = strdup_to_utf8(mp3.id3.album);
                *title = strdup_to_utf8(mp3.id3.title);
            } else {
                /* Use alternate methods of looking for ID3 tags 
                 * This is needed to support, among other things, 
                 * mp3s generated by iTunes */
                get_id3_tags(mp3.file, title, artist, album);
            }
        }
        fclose(mp3.file);
        if (mp3.header_isvalid) 
            return TRUE;
        else
            return FALSE;
        break;
    case WAV:
        f = fopen(path, "r");
        if (!f) 
            return FALSE;
        fread(&header, sizeof(waveheaderstruct), 1, f);
        wav_header_swab(&header);
        fclose(f);
        if ((strncmp(header.chunk_type, "WAVE", 4) == 0) &&
            (header.byte_p_spl / header.modus == 2)) {
            stat(path, &buf);
            *length = (buf.st_size - sizeof(waveheaderstruct)) / 176758;
            return TRUE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}


static int get_element ( gchar * element_name ) {
    int k;
    for (k = 0; k < E_LAST; k++) {
        if (strcasecmp(element_str[k], element_name) == 0)
            return k;
    }
    return k;
}


int write_dirty_song_timeout ( gpointer data ) {
    if (songs_dirty) 
        write_data_file();
    return TRUE;
}


gdouble song_force ( song * a, song  * b ) {
    gdouble ma, mb, attr, sign = 1;
    ma = song_mass(a);
    mb = song_mass(b);
    attr = song_attraction(a, b) * 10;
    if (attr < 0)
        sign = -1;
    return (ma * mb * attr * attr * sign);
}


/* Attraction is a value -1...1 for the affinity between A and B,
   with criteria weighed by prefs */
gdouble song_attraction ( song * a, song  * b ) {
    gdouble a_hue, a_saturation, a_brightness, a_freq, a_bpm, a_path;
    gdouble d, ba, bb, v_diff, a_max, attraction = 0;
    gint i;

    a_max = 
        (prefs.hue + 
         prefs.brightness +
         prefs.saturation +
         prefs.freq + 
         prefs.bpm +
         prefs.path_weight);
    
    a_hue = prefs.hue / a_max;
    a_brightness = prefs.brightness / a_max;
    a_saturation = prefs.saturation / a_max;
    a_freq = prefs.freq / a_max;
    a_bpm = prefs.bpm / a_max;
    a_path = prefs.path_weight / a_max;

    if (!(a->no_color || b->no_color)) {
        /* Hue is 0...6 */
        d = fabsl(a->color.H - b->color.H) / 6.0;
        if (d > 0.5) {
            d = 1 - d;
        }
        /* d is 0...1, where 0 is more similiar */
        d = 1.0 - d*2.0;
        /* d is now -1...1, where 1 is more similiar */
        attraction += d * a_hue;

        d = 1.0 - fabsl(a->color.S - b->color.S) * 2.0;
        /* d is -1 ... 1, where 1 is more similiar*/
        attraction += d * a_saturation;

        d = 1.0 - fabsl(a->color.V - b->color.V) * 2.0;
        /* d is -1 ... 1, where 1 is more similiar*/
        attraction += d * a_brightness;
    }

    if (!(a->bpm_undef || b->bpm_undef)) {
        ba = MIN(MAX(MIN_BPM, a->bpm), MAX_BPM) - MIN_BPM;
        bb = MIN(MAX(MIN_BPM, b->bpm), MAX_BPM) - MIN_BPM;
        d = fabsl(ba - bb) / ((gdouble) (MAX_BPM - MIN_BPM));
        /* d is 0...1, 0 is most similar */
        d = 1.0 - d * 2.0;
        /* d is -1 ... 1 */
        attraction += d * a_bpm;
    }

    if (!(a->no_data || b->no_data)) {
        for (d = 0, i = 0; i < NUM_FREQ_SAMPLES; i++) {
            d += fabsl(a->freq[i] - b->freq[i]);
            if (i < NUM_FREQ_SAMPLES - 1) {
                d += fabsl(a->freq[i] - b->freq[i + 1]) / 2.0;
                d += fabsl(a->freq[i + 1] - b->freq[i]) / 2.0;
            }
            if (i > 0) {
                d += fabsl(a->freq[i] - b->freq[i - 1]) / 2.0;  
                d += fabsl(a->freq[i - 1] - b->freq[i]) / 2.0;
            }
        }
        /* d is 0...~20.0. Most similar is 0, medium similar are about 2 */
        d = 1.0 - (d / 2.5);
        d = MIN(MAX(d, -1.0), 1.0);
        /* d is now -1...1, 1 is most similar*/
        
        /* We adjust the freq val by the volume diff. The closer to 0, the 
           more similar the two songs are. Values over 1 are dissimilar. */
        v_diff = (MAX(a->volume_diff, b->volume_diff) - 
                  MIN(a->volume_diff, b->volume_diff));
        v_diff = MAX(-1.0, 1.0 - v_diff);
        d = 0.75 * d + 0.25 * v_diff;
        attraction += d * a_freq;
    }

    if (tree_depth && a->path && b->path) {
        d = explore_files_depth_distance(a->path, b->path);
        if (d >= 0) {
            d = 1.0 - 2.0 * (d / tree_depth);
            /* d = -1 ... 1, where 1 is more similiar */
            attraction += d * a_path;
        }
    }
    return attraction;
}



/* Every song has a mass 0...1.0 */
gdouble song_mass ( song * s ) {
    gdouble max_mass = 0, song_mass = 0;
    max_mass = 
        prefs.hue + 
        prefs.brightness +
        prefs.freq + 
        prefs.bpm +
        prefs.path_weight;

    assert(max_mass != 0);

    song_mass += prefs.path_weight;
    if (!s->no_data) {
        song_mass += prefs.freq;
    }
    if (!s->no_color) {
        song_mass += prefs.hue;
        song_mass += prefs.brightness;
        song_mass += prefs.saturation;
    }
    if (!s->bpm_undef) {
        song_mass += prefs.bpm;
    }
    
    return (song_mass / max_mass);
}


void hash_inode_dev( song * s, gboolean has_dev) {
    if ((has_dev == FALSE) && (skip_verify == 0)) {
        struct stat buf;
        if (stat(s->path, &buf)) {
            s->dev = buf.st_dev;
        }
    }
    s->inode_dev_hash = s->inode ^ ( (s->dev << 16) | (s->dev >> 16) );
}
