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
pthread_mutex_t wav_file_shared_mutex;
song * analyze_song = NULL;
int analyze_percent = 0;
analyze_mode analyze_state = ANALYZE_IDLE;
gdouble analyze_bpm;


typedef enum {
    OGG = 0,
    WAV,
    MP3
} ftype;

void *     analyze_thread(void* arg);
FILE *     inflate_to_wav (gchar * path, ftype type);

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
    OggVorbis_File vf;
    waveheaderstruct header;
    wav_file_shared wsfile;
    int result, i;
    guint16 song_len;
    char buffer[BUFFER_SIZE];
    gchar * path;
    gdouble freq[NUM_FREQ_SAMPLES];
    FILE * f;
    ftype type;
    pthread_t thread;
    
    pthread_mutex_lock(&analyze_data_mutex);
    analyze_song = (song *) arg;
    analyze_percent = 0;
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
    if (!analyze_song)
        return NULL;
    f = inflate_to_wav(path, type);
    memset(&wsfile, 0x00, sizeof(wav_file_shared));
    wsfile.f = f;
    fread(&wsfile.header, sizeof(waveheaderstruct), 1, f);
    wav_header_swab(&wsfile.header);
    wsfile.header.data_length = (analyze_song->length - 1) * wsfile.header.byte_p_sec;
    pthread_mutex_init(&wsfile.end, NULL);
    pthread_mutex_init(&wsfile.d_w, NULL);
    fread(&wsfile.buffer, 1, WAV_BUF_SIZE, f);
    analyze_percent = 0;
    analyze_state = ANALYZE;
    pthread_mutex_unlock(&analyze_data_mutex);
    
    /* Run the analysis */
    pthread_create(&thread, NULL, bpm, (void *) &wsfile);
    result = spectrum(&wsfile, freq);

    /* If the spectrum finishs before the BPM, set the status bar to
     * say that we're finishing up */
    pthread_mutex_lock(&analyze_data_mutex);
    analyze_percent = 0;
    analyze_state = ANALYZE_FINISH;
    pthread_mutex_unlock(&analyze_data_mutex);
    
    pthread_join(thread, NULL);
    
    pthread_mutex_lock(&analyze_data_mutex);
    if (result && analyze_song) {
        for (i = 0; i < NUM_FREQ_SAMPLES; i++) 
            analyze_song->freq[i] = freq[i];
        analyze_song->flags -= FREQ_UNK;
        analyze_song->flags -= BPM_UNK;
        analyze_song->bpm = analyze_bpm;
        if (isinf(analyze_song->bpm) || (analyze_song->bpm < MIN_BPM)) {
            analyze_song->flags |= BPM_UNDEF;
        }
    } 
    pthread_mutex_unlock(&analyze_data_mutex);

    /* Finish reading rest of output */
    while ((result = fread(buffer, 1,  BUFFER_SIZE, f)))
        ;
    fclose(f);

    pthread_mutex_lock(&analyze_data_mutex);
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



/* Both the frequency and BPM algorithms read the same decoded WAV.
   They are run in separate threads, so we use a mutex lock so they
   share the same input buffer. This is also where we update the
   percent done. */
int fread_wav_shared (void *ptr,  
                      size_t size, 
                      wav_file_shared * wsfile, 
                      gboolean is_freq) {
    long seek;
    int result;

    if (is_freq) {
        seek = wsfile->freq_seek;
    } else {
        seek = wsfile->bpm_seek;
    }

    assert(seek + size <= wsfile->buffer_seek + WAV_BUF_SIZE);
    memcpy(ptr, wsfile->buffer + seek - wsfile->buffer_seek, size);
    
    if (is_freq) {
        wsfile->freq_seek += size;
    } else {
        wsfile->bpm_seek += size;
    }
    
    if (seek + size == wsfile->buffer_seek + WAV_BUF_SIZE) {
        pthread_mutex_lock(&wsfile->d_w);
        if (wsfile->dont_wait || pthread_mutex_trylock(&wsfile->end)) {
            pthread_mutex_unlock(&wsfile->d_w);
            /* If the mutex was already locked, that means both
               threads are ready for more data in buffer. */
            result = fread(wsfile->buffer, 1, WAV_BUF_SIZE, wsfile->f);
            wsfile->buffer_seek += result;

            pthread_mutex_lock(&analyze_data_mutex);
            analyze_percent = (wsfile->buffer_seek * 100) / 
                wsfile->header.data_length;
            pthread_mutex_unlock(&analyze_data_mutex);

            pthread_mutex_unlock(&wsfile->end);
        } else {
            pthread_mutex_unlock(&wsfile->d_w);
            /* The end mutex is unlocked, which means the other thread
             * is still reading off the buffer. */
            pthread_mutex_lock(&wsfile->end); /* Block; it's already locked */
            pthread_mutex_unlock(&wsfile->end);
        } 
    } 
    return size;
}

