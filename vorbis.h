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
 *
 * GJay offers ogg support as a soft dependancy on libvorbis and
 * libvorbisfile
 *
 * For the record, I love the xiph people. Ogg rocks. 
 */
#include <stdio.h>


typedef struct vorbis_comment{
  char **user_comments;
  int   *comment_lengths;
  int    comments;
  char  *vendor;
} vorbis_comment;


typedef int (*ov_open)   (FILE *f, void * vf, char *initial, long ibytes);
typedef vorbis_comment * (*ov_comment)  (void * vf, int link);
typedef double           (*ov_time_total) (void * vf, int i);
typedef int              (*ov_clear) (void * vf);

extern  ov_open       gj_ov_open;
extern  ov_comment    gj_ov_comment;
extern  ov_time_total gj_ov_time_total;
extern  ov_clear      gj_ov_clear;

int     gjay_vorbis_dlopen(void);
int     gjay_vorbis_available(void);
char *  gjay_vorbis_error(void);
