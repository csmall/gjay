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
 *
 * Analysis.c -- manages the background threads and processes involved
 * in song analysis.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/poll.h>
#include <vorbis/vorbisfile.h>
#include <vorbis/codec.h>
#include <endian.h>
#include <math.h>
#include "gjay.h"
#include "analysis.h"

pthread_mutex_t analyze_mutex;
pthread_mutex_t analyze_data_mutex;
song * analyze_song = NULL;
int analyze_percent = 0;
analyze_mode analyze_state = ANALYZE_IDLE;
gboolean analyze_redraw_freq = FALSE;


typedef enum {
    OGG = 0,
    WAV,
    MP3
} ftype;


void *     analyze_thread(void* arg);
FILE *     inflate_to_wav (gchar * path, ftype type);
FILE *     inflate_to_raw (gchar * path, ftype type);

gboolean analyze(song * s) {
    pthread_t thread;
    if (!(s->flags & ( FREQ_UNK | BPM_UNK)))
        return FALSE;
    if (pthread_mutex_trylock(&analyze_mutex)) {
        return FALSE;
    }
    pthread_create(&thread, NULL, analyze_thread, (void *) s);
    return TRUE;
}

void * analyze_thread(void* arg) {
    int result, i;
    guint16 song_len;
    char buffer[BUFFER_SIZE];
    gchar * path;
    gdouble freq[NUM_FREQ_SAMPLES];
    gdouble song_bpm;
    waveheaderstruct header;
    OggVorbis_File vf;
    FILE * f;
    ftype type;
    
    pthread_mutex_lock(&analyze_data_mutex);
    analyze_song = (song *) arg;
    analyze_percent = 0;
    analyze_redraw_freq = FALSE;
    path = g_strdup(analyze_song->path);
    song_len = analyze_song->length;
    pthread_mutex_unlock(&analyze_data_mutex);

    /* First, determine file type */
    f = fopen(path, "r");
    if (!f) {
        /* Song ain't there. */
        pthread_mutex_lock(&analyze_data_mutex);
        analyze_song->flags |= SONG_ERROR;
        pthread_mutex_unlock(&analyze_data_mutex);
        pthread_mutex_unlock(&analyze_mutex);
        return NULL;
    }

    if(ov_open(f, &vf, NULL, 0) == 0) {
        type = OGG;
    } else {
        rewind(f);
        fread(&header, sizeof(waveheaderstruct), 1, f);
        wav_header_swab(&header);
        if ((strncmp(header.chunk_type, "WAVE", 4) == 0) &&
            (header.byte_p_spl / header.modus == 2)) {
            type = WAV;
        }  else {
            type = MP3;
        }
    }
    fclose(f);

    pthread_mutex_lock(&analyze_data_mutex);
    if (analyze_song && (analyze_song->flags & FREQ_UNK)) {
        analyze_percent = 0;
        analyze_state = ANALYZE_FREQ;
        pthread_mutex_unlock(&analyze_data_mutex);
        f = inflate_to_wav(path, type);
        result = spectrum(f, song_len, freq);

        pthread_mutex_lock(&analyze_data_mutex);
        if (result && analyze_song) {
            for (i = 0; i < NUM_FREQ_SAMPLES; i++) 
                analyze_song->freq[i] = freq[i];
            analyze_song->flags -= FREQ_UNK;
            analyze_redraw_freq = TRUE;
        } 
        pthread_mutex_unlock(&analyze_data_mutex);

        /* Finish reading rest of output */
        while ((result = fread(buffer, 1,  BUFFER_SIZE, f)))
            ;
        fclose(f);
    } else {
        pthread_mutex_unlock(&analyze_data_mutex);
    }

    pthread_mutex_lock(&analyze_data_mutex);
    if (analyze_song && (analyze_song->flags & BPM_UNK)) {
        analyze_percent = 0;
        analyze_state = ANALYZE_BPM;
        pthread_mutex_unlock(&analyze_data_mutex);

        f = inflate_to_wav(path, type);
        song_bpm = bpm(f, song_len);

        /* Finish reading rest of output */
        while ((result = fread(buffer, 1,  BUFFER_SIZE, f)))
            ;
        fclose(f);
        
        pthread_mutex_lock(&analyze_data_mutex);
        if (analyze_song) {
            analyze_song->bpm = song_bpm;
            if (isinf(analyze_song->bpm) || (analyze_song->bpm < MIN_BPM)) {
                analyze_song->flags |= BPM_UNDEF;
            }
            analyze_song->flags -= BPM_UNK;
        }
    }

    // analyze_data_mutex is locked
    analyze_percent = 0;
    analyze_state = ANALYZE_DONE;
    pthread_mutex_unlock(&analyze_data_mutex);
    pthread_mutex_unlock(&analyze_mutex);
    g_free(path);
    return NULL;
}


void * sys_thread(void* arg) {
    system((char *) arg);
    free((char *) arg);
    return NULL;
}


FILE * inflate_to_wav ( gchar * path,
                        ftype type) {
    char buffer[BUFFER_SIZE];
    char quoted_path[BUFFER_SIZE];
    FILE * f;

    quote_path(quoted_path, BUFFER_SIZE, path);
    switch (type) {
    case OGG:
        snprintf(buffer, BUFFER_SIZE, "ogg123 \'%s\' -d wav -f - 2> /dev/null",
                 quoted_path);
        break;
    case MP3:
        snprintf(buffer, BUFFER_SIZE, "mpg321 -b 10000 \'%s\' -w - 2> /dev/null",
                 quoted_path);
        break;
    case WAV:
        return fopen(path, "r");
    }
    if (!(f = popen(buffer, "r"))) {
        fprintf(stderr, "Unable to run %s\n", buffer);
        return NULL;
    } 
    return f;
}


/* Swap the byte order of wav header. Wavs are stored little-endian, so this
   is necessary when using on big-endian (e.g. PPC) machines */
void wav_header_swab(waveheaderstruct * header) {
    header->length = le32_to_cpu(header->length);
    header->length_chunk = le32_to_cpu(header->length_chunk);
    header->data_length = le32_to_cpu(header->data_length);
    header->sample_fq = le32_to_cpu(header->sample_fq);
    header->byte_p_sec = le32_to_cpu(header->byte_p_sec);
    header->format = le16_to_cpu(header->format);
    header->modus = le16_to_cpu(header->modus);
    header->byte_p_spl = le16_to_cpu(header->byte_p_spl);
    header->bit_p_spl = le16_to_cpu(header->bit_p_spl);
}



/* Escape ' char in path */
int quote_path(char *buf, size_t bufsiz, const char *path) {
    int in, out = 0;
    const char *quote = "\'\\\'\'"; /* a quoted quote character */
    for (in = 0; out < bufsiz && path[in] != '\0'; in++) {
        if (path[in] == '\'') {
            if (out + strlen(quote) > bufsiz) {
                break;
            } else {
                memcpy(&buf[out], quote, strlen(quote)); 
                out += strlen(quote);
            }
        } else {
            buf[out++] = path[in];
        }
    }
    if (out < bufsiz)
        buf[out++] = '\0';
    return out;
}
