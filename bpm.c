/**
 * GJay, copyright (c) 2002 Chuck Groom. Core algorithm in bmp.c (the
 * 'bpm' function) taken from BpmDJ by Werner Van Belle. My changes
 * are strictly limited to making the algorithm work data from a
 * non-seekable file.
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
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <glib.h>
#include "analysis.h"

unsigned char *audio;
unsigned long audiosize;
unsigned long audiorate=11025;  /* perfect measure, 5512 is too lossy,
                                   22050 takes too much time */
unsigned long startbpm=120;
unsigned long stopbpm=160;

unsigned long startshift=0;
unsigned long stopshift=0;


unsigned long phasefit(long i)
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


void * bpm (void * arg) {
    signed short buffer[BPM_BUF_SIZE];
    long count,pos,i,redux,startpos;
    wav_file_shared *wsfile;
    
    wsfile = (wav_file_shared *) arg;

    audiosize = wsfile->header.data_length;
    audiosize/=(4*(44100/audiorate));
    audio=malloc(audiosize+1);
    assert(audio);
    pos=0;
    startpos = pos;
    while(pos<audiosize)
    {
	count = fread_wav_shared(buffer, BPM_BUF_SIZE, wsfile, FALSE);
	for (i=0;i<count/2;i+=2*(44100/audiorate))
        {
            signed long int left, right,mean;
            left=abs(buffer[i]);
            right=abs(buffer[i+1]);
            mean=(left+right)/2;
            redux=abs(mean)/128;
            if (pos+i/(2*(44100/audiorate))>=audiosize) break;
            assert(pos+i/(2*(44100/audiorate))<audiosize);
            audio[pos+i/(2*(44100/audiorate))]=(unsigned char)redux;
        }
	pos+=count/(4*(44100/audiorate));
    }
    stopshift=audiorate*60*4/startbpm;
    startshift=audiorate*60*4/stopbpm;
    {
	unsigned long foutat[stopshift-startshift];
	unsigned long fout, minimumfout=0, maximumfout,minimumfoutat,left,right;
	memset(&foutat,0,sizeof(foutat));
	for(i=startshift;i<stopshift;i+=50)
        {
            fout=phasefit(i);
            foutat[i-startshift]=fout;
            if (minimumfout==0) maximumfout=minimumfout=fout;
            if (fout<minimumfout) 
            {
                minimumfout=fout;
                minimumfoutat=i;
            }
            if (fout>maximumfout) maximumfout=fout;
            pthread_mutex_lock(&analyze_data_mutex);
            if(analyze_state == ANALYZE_FINISH) 
                analyze_percent = (50*(i - startshift)) / (stopshift - startshift);
            pthread_mutex_unlock(&analyze_data_mutex);
        }
        left=minimumfoutat-100;
	right=minimumfoutat+100;
	if (left<startshift) left=startshift;
	if (right>stopshift) right=stopshift;
	for(i=left;i<right;i++) {
            fout=phasefit(i);
            foutat[i-startshift]=fout;
	     if (minimumfout==0) maximumfout=minimumfout=fout;
	     if (fout<minimumfout) 
             {
                 minimumfout=fout;
                 minimumfoutat=i;
             }
	     if (fout>maximumfout) maximumfout=fout;
            
             pthread_mutex_lock(&analyze_data_mutex);
             if(analyze_state == ANALYZE_FINISH) 
                 analyze_percent = 50 + ((50*(i - left)) / (right - left));
             pthread_mutex_unlock(&analyze_data_mutex);
        }

        for(i=startshift;i<stopshift;i++) {
            fout=foutat[i-startshift];
            if (fout)
            {
                fout-=minimumfout;
            }
        }
        pthread_mutex_lock(&wsfile->d_w);
        wsfile->dont_wait = TRUE;
        pthread_mutex_trylock(&wsfile->end);
        pthread_mutex_unlock(&wsfile->end);
        pthread_mutex_unlock(&wsfile->d_w);

        analyze_bpm = 4.0*(double)audiorate*60.0/(double)minimumfoutat;
        return NULL;
    }
    analyze_bpm = 0;
    return NULL;
}
