/**
 * GJay, copyright (c) 2002 Chuck Groom. Sections of this code come
 * from spectromatic, copyright (C) 1997-2002 Daniel Franklin, and 
 * BpmDJ by Werner Van Belle.
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
#include <string.h>
#include <unistd.h>
#include <stdint.h>
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
#include <time.h>
#include <assert.h>
#include <math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_fft_real.h>
#include <gsl/gsl_fft_halfcomplex.h>
#include "gjay.h"
#include "analysis.h"

pthread_mutex_t analyze_mutex;
pthread_mutex_t analyze_data_mutex;

song * analyze_song = NULL;
int analyze_percent = 0;
analyze_mode analyze_state = ANALYZE_IDLE;
gdouble analyze_bpm;

// #define DEBUG

typedef enum {
    OGG = 0,
    WAV,
    MP3
} ftype;

#define BPM_BUF_SIZE 32*1024
#define SHARED_BUF_SIZE sizeof(short int) * BPM_BUF_SIZE

/* Shared struct for reading wav file data between two threads */
typedef struct {
    FILE * f;
    waveheaderstruct header;
    long int freq_seek, seek;
    char buffer[SHARED_BUF_SIZE];
    gboolean dont_wait; /* Set when one thread finishes */
} wav_file_shared;


/* BPM globals */
unsigned char *audio;
unsigned long audiosize;
unsigned long audiorate=2756;  /* Author states that 11025 is perfect
                                  measure, but we can tolerate more
                                  lossiness for the sake of speed */
unsigned long startbpm=120;
unsigned long stopbpm=160;
unsigned long startshift=0;
unsigned long stopshift=0;


/* Spectrum globals */
#define MAX_FREQ 22500
#define START_FREQ 100

typedef unsigned long ulongT;
typedef unsigned short ushortT;

static char * read_buffer;
static int read_buffer_size;
static long read_buffer_start;
static long read_buffer_end; /* same as file position */
static int window_size = 1024;
static int step_size = 1024;


void *     analyze_thread(void* arg);
FILE *     inflate_to_wav (gchar * path, ftype type);

int           run_analysis     ( wav_file_shared * wsfile,
                                 gdouble * freq_results,
                                 gdouble * volume_diff,
                                 gdouble * bpm_result );
unsigned long bpm_phasefit     ( long i );
int           freq_read_frames ( wav_file_shared * wsfile, 
                                 int start, 
                                 int length, 
                                 void *data );




gboolean analyze(song * s) {
    pthread_t thread;
    if (s->flags & ANALYZED)
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
    gdouble freq[NUM_FREQ_SAMPLES], volume_diff, analyze_bpm;
    FILE * f;
    ftype type;
#ifdef DEBUG
    time_t t;
#endif

    pthread_mutex_lock(&analyze_data_mutex);
    analyze_song = (song *) arg;
    analyze_percent = 0;
    analyze_state = ANALYZE;
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
    if (!analyze_song) {
        pthread_mutex_unlock(&analyze_data_mutex);
        pthread_mutex_unlock(&analyze_mutex);
        return NULL;
    }
    f = inflate_to_wav(path, type);
    memset(&wsfile, 0x00, sizeof(wav_file_shared));
    wsfile.f = f;
    fread(&wsfile.header, sizeof(waveheaderstruct), 1, f);
    wav_header_swab(&wsfile.header);
    wsfile.header.data_length = (analyze_song->length - 1) * wsfile.header.byte_p_sec;
#ifdef DEBUG
    printf("Analyzing %s\n", analyze_song->fname);
    t = time(NULL);
#endif
    pthread_mutex_unlock(&analyze_data_mutex);

    result = run_analysis(&wsfile, freq, &volume_diff, &analyze_bpm); 

#ifdef DEBUG
    printf("Analysis took %ld seconds\n", time(NULL) - t);
#endif

    pthread_mutex_lock(&analyze_data_mutex);
    analyze_percent = 0;
    analyze_state = ANALYZE_FINISH;
    if (result && analyze_song) {
        for (i = 0; i < NUM_FREQ_SAMPLES; i++) 
            analyze_song->freq[i] = freq[i];
        analyze_song->flags |= ANALYZED;
        analyze_song->bpm = analyze_bpm;
        if (isinf(analyze_song->bpm) || (analyze_song->bpm < MIN_BPM)) {
            analyze_song->flags |= BPM_UNDEF;
        }
        analyze_song->volume_diff = volume_diff;
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


/**
 * This is the big, bad analysis algorithm. To make things REALLY complicated,
 * I'm interspersing two algorithsm -- BPM and frequency analysis -- such that
 * we only have to do one decode pass. Whee!
 *
 * BPM algorithm taken from BpmDJ by Werner Van Belle.
 *
 * Frequency algorithm taken from spectromatic by Daniel Franklin.
 *
 * Any ugliness is the fault of my munging. 
 */
int run_analysis  (wav_file_shared * wsfile,
                   gdouble * freq_results,
                   gdouble * volume_diff,
                   gdouble * bpm_result ) {
    int16_t *freq_data;
    double *ch1 = NULL, *ch2 = NULL, *mags = NULL;
    long i, j, k, bin;
    double ch1max = 0, ch2max = 0;
    double *total_mags;
    double sum, frame_sum, max_frame_sum, g_factor, freq, g_freq;
    signed short buffer[BPM_BUF_SIZE];
    long count, pos, h, redux, startpos, num_frames;

    if (wsfile->header.modus != 1 && wsfile->header.modus != 2) {
        fprintf (stderr, "Error: not a wav file...\n");
        return FALSE;
    }
    
    if (wsfile->header.byte_p_spl / wsfile->header.modus != 2) {
        fprintf (stderr, "Error: not 16-bit...\n");
        return FALSE;
    }

    /* Spectrum set-up */    
    read_buffer_size = window_size*4;
    read_buffer = malloc(read_buffer_size);
    read_buffer_start = 0;
    read_buffer_end = read_buffer_size;
    freq_data = (int16_t*) malloc (window_size * wsfile->header.byte_p_spl);
    mags = (double*) malloc (window_size / 2 * sizeof (double));
    total_mags = (double*) malloc (window_size / 2 * sizeof (double));
    memset (total_mags, 0x00, window_size / 2 * sizeof (double));
    memset (freq_results, 0x00, NUM_FREQ_SAMPLES * sizeof(double));
    ch1 = (double*) malloc (window_size * sizeof (double));
    if (wsfile->header.modus == 2)
        ch2 = (double*) malloc (window_size * sizeof (double));
    num_frames = 0;
    max_frame_sum = 0;
    sum = 0;

    /* BPM set-up */
    audiosize = wsfile->header.data_length;
    audiosize/=(4*(44100/audiorate));
    audio=malloc(audiosize+1);
    assert(audio);
    pos=0;
    startpos = pos;


    /* Read the first chunk of the file into the shared buffer */
    fread(wsfile->buffer, 1, SHARED_BUF_SIZE, wsfile->f);
    wsfile->seek = SHARED_BUF_SIZE;
    
    /* Copy the first bit of this into the freq. analysis buffer */
    memcpy(read_buffer, wsfile->buffer, read_buffer_size);
    wsfile->freq_seek = read_buffer_size;
    
    /* In this main loop, we read the entire file. Most of this loop 
     * represents the spectrum algorithm; the marked tight loop is the bpm
     * algorithm. */
    for (i = -window_size; i < window_size + (int)(wsfile->header.data_length / wsfile->header.byte_p_spl); i += step_size) {

        freq_read_frames (wsfile, i, window_size, (char *)freq_data);

        if (wsfile->freq_seek == SHARED_BUF_SIZE) {
            /* At end of buffer. Read some more data. */
            count = fread(wsfile->buffer, 1, SHARED_BUF_SIZE, wsfile->f);
            memcpy(buffer, wsfile->buffer, SHARED_BUF_SIZE);
            wsfile->freq_seek = 0;
            wsfile->seek += count;
            
            /* Update status bar */
            pthread_mutex_lock(&analyze_data_mutex);
            analyze_percent = MIN(100, (wsfile->seek * 100 ) / 
                                  wsfile->header.data_length);
            pthread_mutex_unlock(&analyze_data_mutex);
            
            /* BPM loop */
            for (h=0;h<count/2;h+=2*(44100/audiorate))
            {
                signed long int left, right,mean;
                left=abs(buffer[h]);
                right=abs(buffer[h+1]);
                mean=(left+right)/2;
                redux=abs(mean)/128;
                if (pos+h/(2*(44100/audiorate))>=audiosize) break;
                assert(pos+h/(2*(44100/audiorate))<audiosize);
                audio[pos+h/(2*(44100/audiorate))]=(unsigned char)redux;
            }
            pos+=count/(4*(44100/audiorate));
        }
        
        /* The rest of this loop is spectrum analysis */
        if (wsfile->header.modus == 1) {
            for (j = 0; j < window_size; j++)
                ch1 [j] = (int16_t)le16_to_cpu(freq_data[j]);
            
        } else {
            for (j = 0, k = 0; k < window_size; k++, j+=2) {
                ch1[k] = (double)((int16_t)le16_to_cpu(freq_data[j]));
                ch2[k] = (double)((int16_t)le16_to_cpu(freq_data[j+1]));
            } 
            gsl_fft_real_radix2_transform (ch2, 1, window_size);
        }
        gsl_fft_real_radix2_transform (ch1, 1, window_size);
        
        mags [0] = fabs (ch1 [0]);
        
        for (j = 0; j < window_size / 2; j++) {
            mags [j] = sqrt (ch1 [j] * ch1 [j] + ch1 [window_size - j] * ch1 [window_size - j]);
            
            if (mags [j] > ch1max)
                ch1max = mags [j];
        }
        
        /* Add magnitudes */
        for (k = 0; k < window_size / 2; k++) 
            total_mags[k] += mags[k];
        
        if (wsfile->header.modus == 2) {
            mags [0] = fabs (ch2 [0]);
            
            for (j = 0; j < window_size / 2; j++) {
                mags [j] = sqrt (ch2 [j] * ch2 [j] + ch2 [window_size - j] * ch2 [window_size - j]);
                
                if (mags [j] > ch2max)
                    ch2max = mags [j];
            }

            /* Add magnitudes */            
            for (frame_sum = 0, k = 0; k < window_size / 2; k++) {
                total_mags[k] += mags[k];
                frame_sum += mags[k];
            }
            max_frame_sum = MAX(frame_sum, max_frame_sum);
            sum += frame_sum;
            num_frames++;
        }
    }
    
    pthread_mutex_lock(&analyze_data_mutex);
    analyze_state = ANALYZE_FINISH;
    analyze_percent = 0;
    pthread_mutex_unlock(&analyze_data_mutex);
    *volume_diff =  max_frame_sum / (sum / (gdouble) num_frames);

#ifdef DEBUG
    printf ("Diff between max and min vols %f\n", 
            *volume_diff);
#endif 

    for (k = 0; k < window_size / 2; k++) 
        total_mags[k] = total_mags[k] / sum;
    
    g_freq = START_FREQ;
    g_factor = exp( log(MAX_FREQ/START_FREQ) / NUM_FREQ_SAMPLES);
    bin = 0;
    for (k = 0; k < window_size / 2; k++) {
        /* Determine which frequency band this sample falls into */
        freq = (k * MAX_FREQ ) / (window_size / 2);
        if (freq > g_freq) {
            bin = MIN(bin + 1, NUM_FREQ_SAMPLES - 1);
            g_freq *= g_factor;
        }
        freq_results[bin] += total_mags[k];
    }

#ifdef DEBUG
    for (k = 0; k < NUM_FREQ_SAMPLES; k++) 
        printf("%f ", freq_results[k]);
    printf("\n");
#endif        

    /* Complete BPM analysis */
    stopshift=audiorate*60*4/startbpm;
    startshift=audiorate*60*4/stopbpm;
    {
	unsigned long foutat[stopshift-startshift];
	unsigned long fout, minimumfout=0, maximumfout,minimumfoutat,left,right;
        memset(&foutat,0,sizeof(foutat));
	for(h=startshift;h<stopshift;h+=50)
        {
            fout=bpm_phasefit(h);
            foutat[h-startshift]=fout;
            if (minimumfout==0) maximumfout=minimumfout=fout;
            if (fout<minimumfout) 
            {
                minimumfout=fout;
                minimumfoutat=h;
            }
            if (fout>maximumfout) maximumfout=fout;
            pthread_mutex_lock(&analyze_data_mutex);
            analyze_percent = (50*(h - startshift)) / (stopshift - startshift);
            pthread_mutex_unlock(&analyze_data_mutex);
        }
        left=minimumfoutat-100;
	right=minimumfoutat+100;
	if ( left < startshift ) 
            left = startshift;
	if ( right > stopshift ) 
            right = stopshift;
	for(h=left; h<right; h++) {
            fout=bpm_phasefit(h);
            foutat[h-startshift]=fout;
            if (minimumfout==0) maximumfout=minimumfout=fout;
            if (fout<minimumfout) 
            {
                minimumfout=fout;
                minimumfoutat=h;
            }
            if (fout>maximumfout) maximumfout=fout;
            
            pthread_mutex_lock(&analyze_data_mutex);
            analyze_percent = 50 + ((50*(h - left)) / (right - left));
            pthread_mutex_unlock(&analyze_data_mutex);
        }
        
        for(h=startshift;h<stopshift;h++) {
            fout=foutat[h-startshift];
            if (fout)
            {
                fout-=minimumfout;
            }
        }
        *bpm_result = 4.0*(double)audiorate*60.0/(double)minimumfoutat;
#ifdef DEBUG
        printf("BPM: %f\n", *bpm_result);
#endif
    }
    free (audio);
    free (freq_data);
    free (mags);
    free (total_mags);
    free (read_buffer);
    return TRUE;
}



int freq_read_frames (wav_file_shared * wsfile,
                      int start, 
                      int length, 
                      void *data)
{
    int realstart = start;
    int reallength = length;
    int offset = 0;
    int seek, len;
    
    if (start + length < 0 || start > (int)(wsfile->header.data_length / wsfile->header.byte_p_spl)) {
        memset (data, 0, length * wsfile->header.byte_p_spl);
        return 0;
    }
    
    if (start < 0) {
        offset = -start;
        memset (data, 0, offset * wsfile->header.byte_p_spl);
        realstart = 0;
        reallength += start;
    }
    
    if (start + length > (int)(wsfile->header.data_length / wsfile->header.byte_p_spl)) {
        reallength -= start + length - (wsfile->header.data_length / wsfile->header.byte_p_spl);
        memset (data, 0, reallength * wsfile->header.byte_p_spl);
    }
    
    seek = ((realstart + offset) * wsfile->header.byte_p_spl);
    len = wsfile->header.byte_p_spl * reallength;
    if (seek + len <= read_buffer_size) {
        memcpy(data + offset * wsfile->header.byte_p_spl,
               read_buffer + seek,
               wsfile->header.byte_p_spl * reallength);
    } else {
        if (seek + len > read_buffer_end) {
            char * new_buffer = malloc(read_buffer_size);
            int shift = seek + len - read_buffer_end;
            memcpy(new_buffer, 
                   read_buffer + shift,
                   read_buffer_size - shift);
            free(read_buffer);
            read_buffer = new_buffer;

            memcpy (read_buffer + read_buffer_size - shift,
                    wsfile->buffer + wsfile->freq_seek,
                    shift);
            wsfile->freq_seek += shift;

            read_buffer_start += shift;
            read_buffer_end += shift;
        }
        memcpy(data + offset * wsfile->header.byte_p_spl,
               read_buffer + seek - read_buffer_start,
               wsfile->header.byte_p_spl * reallength);
    }
    return 1;
}


unsigned long bpm_phasefit(long i)
{
   long c,d;
   unsigned long mismatch=0;
   unsigned long prev=mismatch;
   for(c=i;c<audiosize;c++)
     {
	d=abs((long)audio[c]-(long)audio[c-i]);
	prev=mismatch;
	mismatch+=d;
	assert(mismatch>=prev);
     }
   return mismatch;
}
