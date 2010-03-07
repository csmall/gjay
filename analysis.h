/**
 * GJay, copyright (c) 2002 Chuck Groom
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
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
	unsigned char		main_chunk[4];	/* 'RIFF' */
	uint32_t    	  length;		/* length of file */
	unsigned char		chunk_type[4];	/* 'WAVE' */
	unsigned char		sub_chunk[4];	/* 'fmt' */
	uint32_t    	length_chunk;	/* length sub_chunk, always 16 bytes */
	uint16_t	format;		/* always 1 = PCM-Code */
	uint16_t	modus;		/* 1 = Mono, 2 = Stereo */
	uint32_t sample_fq;	/* Sample Freq */
	uint32_t	byte_p_sec;	/* Data per sec */
	uint16_t	byte_p_spl;	/* Bytes per sample */
	uint16_t	bit_p_spl;	/* bits per sample, 8, 12, 16 */
	unsigned char		data_chunk[4];	/* 'data' */
	uint32_t	data_length;	/* length of data */
} waveheaderstruct;


/* Mutex lock for analysis data */
extern int             analyze_percent;


/* Run the analysis of one song */
void run_as_daemon(void);
void     run_as_analyze_detached  ( const char * analyze_detached_fname );

/* Endian stuff */
void     wav_header_swab(waveheaderstruct * header); 
unsigned long  swab_ul(unsigned long l); 
unsigned short swab_us(unsigned short s);
 
#endif /* __ANALYSIS_H__ */



