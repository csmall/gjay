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
#include <math.h>
#include "gjay.h"
#include "analysis.h"

pthread_mutex_t analyze_mutex;
pthread_mutex_t analyze_data_mutex;
song * analyze_song = NULL;
int analyze_percent = 0;
analyze_mode analyze_state = ANALYZE_IDLE;

typedef enum {
    OGG = 0,
    WAV,
    MP3
} ftype;


void *     analyze_thread(void* arg);
FILE *     inflate_to_wav (song * s, ftype type);
FILE *     inflate_to_raw (song * s, ftype type);
  
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
    int result;
    long fsize, fsize_est;
    song * s = (song *) arg;
    char * block;
    int block_size = 1024;
    waveheaderstruct header;
    OggVorbis_File vf;
    FILE * f;
    ftype type;
    
    pthread_mutex_lock(&analyze_data_mutex);
    analyze_song = s;
    analyze_percent = 0;
    pthread_mutex_unlock(&analyze_data_mutex);

    /* First, determine file type */
    f = fopen(s->path, "r");
    if (!f)
        return NULL;
    if(ov_open(f, &vf, NULL, 0) == 0) {
        type = OGG;
    } else {
        rewind(f);
        fread(&header, sizeof(waveheaderstruct), 1, f);
        if ((strncmp(header.chunk_type, "WAVE", 4) == 0) &&
            (header.byte_p_spl / header.modus == 2)) {
            type = WAV;
            fsize = header.length;
        }  else {
            type = MP3;
        }
    }
    fclose(f);

    /* If the file is an ogg or MP3, determine file length in the 
       most gruesome manner possible */
    if (type == OGG || type == MP3) {
        fsize_est = (s->length ? s->length : 60*8) * 177373;    
        pthread_mutex_lock(&analyze_data_mutex);
        analyze_state = ANALYZE_LEN;
        pthread_mutex_unlock(&analyze_data_mutex);
        
        if (!(f = inflate_to_raw(s, type)))
            return NULL;
        block = g_malloc(block_size);
        fsize = 0;
        while ((result = fread(block, 1,  block_size, f))) {
            fsize += result;
            pthread_mutex_lock(&analyze_data_mutex);
            analyze_percent = MAX(0, MIN(100, 100*(((double) fsize) / fsize_est )));
            pthread_mutex_unlock(&analyze_data_mutex);
        }
        fclose(f);
        g_free(block);
    }


    if (analyze_song->flags & FREQ_UNK) {
        pthread_mutex_lock(&analyze_data_mutex);
        analyze_percent = 0;
        analyze_state = ANALYZE_FREQ;
        pthread_mutex_unlock(&analyze_data_mutex);
        f = inflate_to_wav(s, type);
        spectrum(f, fsize, analyze_song->freq);
        fclose(f);
        analyze_song->flags -= FREQ_UNK;
    } 

    if (analyze_song->flags & BPM_UNK) {
        pthread_mutex_lock(&analyze_data_mutex);
        analyze_percent = 0;
        analyze_state = ANALYZE_BPM;
        pthread_mutex_unlock(&analyze_data_mutex);
        f = inflate_to_raw(s, type);
        analyze_song->bpm = bpm(f, fsize);
        fclose(f);
        if (isinf(analyze_song->bpm) || (analyze_song->bpm < MIN_BPM)) {
            analyze_song->flags |= BPM_UNDEF;
        }
        analyze_song->flags -= BPM_UNK;
    }
    
    pthread_mutex_lock(&analyze_data_mutex);
    analyze_percent = 0;
    analyze_state = ANALYZE_DONE;
    pthread_mutex_unlock(&analyze_data_mutex);
    pthread_mutex_unlock(&analyze_mutex);
    return NULL;
}


void * sys_thread(void* arg) {
    system((char *) arg);
    free((char *) arg);
    return NULL;
}


FILE * inflate_to_raw ( song * s, 
                        ftype type) {
    char buffer[BUFFER_SIZE];
    FILE * f;
    waveheaderstruct header;

    switch (type) {
    case OGG:
        snprintf(buffer, BUFFER_SIZE, "ogg123 %s -d raw -f - 2> /dev/null",
                 s->path);
        break;
    case MP3:
        snprintf(buffer, BUFFER_SIZE, "mpg321 -b 10000 -s %s 2> /dev/null ",
                 s->path);
        break;
    case WAV: 
        /* Read past the header */
        f = fopen(s->path, "r");
        if (f) {
            fread(&header, sizeof(waveheaderstruct), 1, f);
        }
        return f;
        break;
    }
    if (!(f = popen(buffer, "r"))) {
        fprintf(stderr, "Unable to run %s\n", buffer);
        return NULL;
    } 
    return f;
}


FILE * inflate_to_wav ( song * s, 
                        ftype type) {
    char buffer[BUFFER_SIZE];
    FILE * f;
    
    switch (type) {
    case OGG:
        snprintf(buffer, BUFFER_SIZE, "ogg123 %s -d wav -f - 2> /dev/null",
                 s->path);
        break;
    case MP3:
        snprintf(buffer, BUFFER_SIZE, "mpg321 -b 10000 %s -w - 2> /dev/null",
                 s->path);
        break;
    case WAV:
        return fopen(s->path, "r");
        break;
    }
    if (!(f = popen(buffer, "r"))) {
        fprintf(stderr, "Unable to run %s\n", buffer);
        return NULL;
    } 
    return f;
}
