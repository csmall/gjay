/**
 * GJay, copyright (c) 2002 Chuck Groom. Core algorithm in bmp.c (the
 * 'bpm' function) taken from BpmDJ by Werner Van Belle. My changes
 * are strictly limited to making the algorithm work with a pipe and not
 * a seekable file.
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
unsigned long audiorate=11025;  // perfect measure, 5512 is too lossy, 22050 takes too much time
unsigned long startbpm=120;
unsigned long stopbpm=160;

unsigned long startshift=0;
unsigned long stopshift=0;
long     int  bufsiz=32*1024;
/* There are three distinct loops used in BPM analysis. The first takes
 * about 32% of the time, the second 23%, the last 45% */
int bpm_loop_percents[3] = { 32, 23, 45 };

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


double bpm (FILE * wav_file, 
            guint16 song_len ) {
    char d[500];
    signed short buffer[bufsiz];
    long count,pos,i,redux,startpos;
    int segment_percent;
    waveheaderstruct header;
    
    assert(wav_file);
    
    fread (&header, 1, sizeof(waveheaderstruct), wav_file);
    wav_header_swab(&header);
    audiosize = (song_len - 1) * header.byte_p_sec;
    audiosize/=(4*(44100/audiorate));
    audio=malloc(audiosize+1);
    assert(audio);
    pos=0;
    startpos = pos;
    while(pos<audiosize)
    {
	count=fread(buffer,1,bufsiz,wav_file);
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
   
        segment_percent = ((pos - startpos)*100)/(audiosize - startpos);
        pthread_mutex_lock(&analyze_data_mutex);
        analyze_percent = (bpm_loop_percents[0] * segment_percent) / 100;
        pthread_mutex_unlock(&analyze_data_mutex);
    }
    sprintf(d," ");
//   index_addcomment(d);
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
//	     printf(d,"# %d: %ld (%g BPM)\n",i,fout,
//		    4.0*(double)audiorate*60.0/(double)i);
            if (minimumfout==0) maximumfout=minimumfout=fout;
            if (fout<minimumfout) 
            {
                minimumfout=fout;
                minimumfoutat=i;
            }
            if (fout>maximumfout) maximumfout=fout;
            
            segment_percent = 100 * (i - startshift) / 
                (stopshift - startshift);
            pthread_mutex_lock(&analyze_data_mutex);
            analyze_percent = bpm_loop_percents[0] + 
                (bpm_loop_percents[1] * segment_percent) / 100;
            pthread_mutex_unlock(&analyze_data_mutex);
        }
        left=minimumfoutat-100;
	right=minimumfoutat+100;
	if (left<startshift) left=startshift;
	if (right>stopshift) right=stopshift;
	for(i=left;i<right;i++) {
            fout=phasefit(i);
            foutat[i-startshift]=fout;
//	     printf("# %d: %ld (%g BPM)\n",i,fout,
//		    4.0*(double)audiorate*60.0/(double)i);
	     if (minimumfout==0) maximumfout=minimumfout=fout;
	     if (fout<minimumfout) 
             {
                 minimumfout=fout;
                 minimumfoutat=i;
             }
	     if (fout>maximumfout) maximumfout=fout;
             segment_percent = 100 * (i - left) /
                 (right - left);
             pthread_mutex_lock(&analyze_data_mutex);
             analyze_percent = bpm_loop_percents[0] +
                 bpm_loop_percents[1] +
                 (bpm_loop_percents[2] * segment_percent) / 100;
             pthread_mutex_unlock(&analyze_data_mutex);
        }
	//	printf("# %d: %ld - %ld\n",minimumfoutat,minimumfout,maximumfout);
        for(i=startshift;i<stopshift;i++) {
            fout=foutat[i-startshift];
            if (fout)
            {
                fout-=minimumfout;
                // fout=(fout*100)/(maximumfout-minimumfout);
                sprintf(d,"%g  %ld",4.0*(double)audiorate*60.0/(double)i,fout);
                // index_addcomment(d);
            }
        }
	sprintf(d,"%g",4.0*(double)audiorate*60.0/(double)minimumfoutat);
        return 4.0*(double)audiorate*60.0/(double)minimumfoutat;
    }
    return 0;
}
