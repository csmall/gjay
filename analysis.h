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
 * Song analysis is done in a daemon process
 */


#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include <stdint.h>
#include "gjay.h"


/* Shamelessly excerpted from the Linux kernel */
#if __BYTE_ORDER == __BIG_ENDIAN
#define le32_to_cpu(x) __swab32((x))
#define le16_to_cpu(x) __swab16((x))
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define le32_to_cpu(x) ((uint32_t)(x))
#define le16_to_cpu(x) ((uint16_t)(x))
#endif

#define __swab16(x) \
({ \
         uint16_t __x = (x); \
	 ((uint16_t)( \
	 (((uint16_t)(__x) & (uint16_t)0x00ffU) << 8) | \
	 (((uint16_t)(__x) & (uint16_t)0xff00U) >> 8) )); \
})
#define __swab32(x) \
({ \
         uint32_t __x = (x); \
	         ((uint32_t)( \
			 (((uint32_t)(__x) & (uint32_t)0x000000ffUL) << 24) | \
             (((uint32_t)(__x) & (uint32_t)0x0000ff00UL) <<  8) | \
             (((uint32_t)(__x) & (uint32_t)0x00ff0000UL) >>  8) | \
             (((uint32_t)(__x) & (uint32_t)0xff000000UL) >> 24) )); \
})


/* WAV file header */
typedef struct {  
	char		main_chunk[4];	/* 'RIFF' */
	unsigned long	length;		/* length of file */
	char		chunk_type[4];	/* 'WAVE' */
	char		sub_chunk[4];	/* 'fmt' */
	unsigned long	length_chunk;	/* length sub_chunk, always 16 bytes */
	unsigned short	format;		/* always 1 = PCM-Code */
	unsigned short	modus;		/* 1 = Mono, 2 = Stereo */
	unsigned long	sample_fq;	/* Sample Freq */
	unsigned long	byte_p_sec;	/* Data per sec */
	unsigned short	byte_p_spl;	/* Bytes per sample */
	unsigned short	bit_p_spl;	/* bits per sample, 8, 12, 16 */
	char		data_chunk[4];	/* 'data' */
	unsigned long	data_length;	/* length of data */
} waveheaderstruct;


/* Mutex lock for analysis data */
extern song          * analyze_song;
extern int             analyze_percent;

/* Analysis.c */
void     analysis_daemon(void);
//gboolean analyze(song * s);
int      quote_path(char *buf, size_t bufsiz, const char *path);

/* Endian stuff */
void     wav_header_swab(waveheaderstruct * header); 
unsigned long  swab_ul(unsigned long l); 
unsigned short swab_us(unsigned short s);
 
#endif /* __ANALYSIS_H__ */



