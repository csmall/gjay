/*
 * GJay, copyright (c) 2002-3 Chuck Groom <cgroom@users.sourceforge.net>
 *
 * MP3 header information take from mp3info package's mp3tech.h, which is
 * copyright (C) 2000-2001  Cedric Tefft <cedric@earthling.net>, which is
 * in turn based in part on:
 *    MP3Info 0.5 by Ricardo Cerqueira <rmc@rccn.net>
 *    MP3Stat 0.9 by Ed Sweetman <safemode@voicenet.com> and 
 *     	             Johannes Overmann <overmann@iname.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>

enum VBR_REPORT { VBR_VARIABLE, VBR_AVERAGE, VBR_MEDIAN };
enum SCANTYPE { SCAN_NONE, SCAN_QUICK, SCAN_FULL };

typedef struct {
	unsigned long	sync;
	unsigned int	version;
	unsigned int	layer;
	unsigned int	crc;
	unsigned int	bitrate;
	unsigned int	freq;
	unsigned int	padding;
	unsigned int	extension;
	unsigned int	mode;
	unsigned int	mode_extension;
	unsigned int	copyright;
	unsigned int	original;
	unsigned int	emphasis;
} mp3header;

typedef struct {
	char title[31];
	char artist[31];
	char album[31];
	char year[5];
	char comment[31];
	unsigned char track[1];
	unsigned char genre[1];
} id3tag;

typedef struct {
	char *filename;
	FILE *file;
	off_t datasize;
	int header_isvalid;
	mp3header header;
	int id3_isvalid;
	id3tag id3;
	int vbr;
	float vbr_average;
	int seconds;
	int frames;
	int badframes;
} mp3info;

int get_mp3_info( mp3info *mp3,
                  int scantype, 
                  int fullscan_vbr);
int get_id3_tags( FILE         * fp, 
                  gchar       ** title,
                  gchar       ** artist,
                  gchar       ** album);
