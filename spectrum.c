/**
 * GJay, copyright (c) 2002 Chuck Groom. Most of this file -- all the
 * algorithm bits -- come from spectromatic, copyright (C) 1997-2002
 * Daniel Franklin.
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
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include <endian.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_fft_real.h>
#include <gsl/gsl_fft_halfcomplex.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include "analysis.h"

typedef unsigned long ulongT;
typedef unsigned short ushortT;

int read_frames   (wav_file_shared * wsfile, int start, 
                   int length, void *data);

static char * read_buffer;
static int read_buffer_size;
static long read_buffer_start;
static long read_buffer_end; /* same as file position */
static int window_size = 1024;
static int step_size = 256;

#define MAX_FREQ 22500
#define START_FREQ 100


int spectrum (wav_file_shared * wsfile,
              gdouble * results) {
    int16_t *data;
    double *ch1 = NULL, *ch2 = NULL, *mags = NULL;
    int i, j, k, bin;
    double ch1max = 0, ch2max = 0;
    double *total_mags;
    double sum, g_factor, freq, g_freq;
    
    read_buffer_size = window_size*4;
    read_buffer = malloc(read_buffer_size);
    read_buffer_start = 0;
    read_buffer_end = read_buffer_size;
    fread_wav_shared(read_buffer, read_buffer_size, wsfile, TRUE);

    if (wsfile->header.modus != 1 && wsfile->header.modus != 2) {
        fprintf (stderr, "Error: not a wav file...\n");
        return FALSE;
    }
    
    if (wsfile->header.byte_p_spl / wsfile->header.modus != 2) {
        fprintf (stderr, "Error: not 16-bit...\n");
        return FALSE;
    }

    data = (int16_t*) malloc (window_size * wsfile->header.byte_p_spl);
    mags = (double*) malloc (window_size / 2 * sizeof (double));
    total_mags = (double*) malloc (window_size / 2 * sizeof (double));
    memset (total_mags, 0x00, window_size / 2 * sizeof (double));
    memset (results, 0x00, NUM_FREQ_SAMPLES * sizeof(double));
    ch1 = (double*) malloc (window_size * sizeof (double));
    
    if (wsfile->header.modus == 2)
        ch2 = (double*) malloc (window_size * sizeof (double));
    
    for (i = -window_size; i < window_size + (int)(wsfile->header.data_length / wsfile->header.byte_p_spl); i += step_size) {
        read_frames (wsfile, i, window_size, (char *)data);
        
        if (wsfile->header.modus == 1) {
            for (j = 0; j < window_size; j++)
                ch1 [j] = (int16_t)le16_to_cpu(data[j]);
            
        } else {
            /*for (j = 0; j < window_size; j++) {
                ch1 [j] = data [j << 2] + (data [(j << 2) + 1] << 8);
                ch2 [j] = data [(j << 2) + 2] + (data [(j << 2) + 3] << 8);
            }
            */
            for (j = 0, k = 0; k < window_size; k++, j+=2) {
                ch1[k] = (double)((int16_t)le16_to_cpu(data[j]));
                ch2[k] = (double)((int16_t)le16_to_cpu(data[j+1]));
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
            for (k = 0; k < window_size / 2; k++) 
                total_mags[k] += mags[k];
        }
    }
    
    sum = 0;
    for (k = 0; k < window_size / 2; k++) 
        sum += total_mags[k];
    
    for (k = 0; k < window_size / 2; k++) 
        total_mags[k] = total_mags[k]/sum;

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
        results[bin] += total_mags[k];
    }
    
    free (data);
    free (mags);
    free (total_mags);
    free (read_buffer);

    pthread_mutex_lock(&wsfile->d_w);
    wsfile->dont_wait = TRUE;
    pthread_mutex_trylock(&wsfile->end);
    pthread_mutex_unlock(&wsfile->end);
    pthread_mutex_unlock(&wsfile->d_w);
    return TRUE;
}


int read_frames (wav_file_shared * wsfile,
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
                fread_wav_shared(read_buffer + read_buffer_size - shift, 
                                 shift,
                                 wsfile,
                                 TRUE);
                read_buffer_start += shift;
                read_buffer_end += shift;
            }
            memcpy(data + offset * wsfile->header.byte_p_spl,
                   read_buffer + seek - read_buffer_start,
                   wsfile->header.byte_p_spl * reallength);
        }
	return 1;
}


