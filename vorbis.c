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
 * Thin wrapper for libvorbis and libvorbisfile, which are dlopen'ed. 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <strings.h>
#include <dlfcn.h>
#include <gjay.h>
#include <vorbis/vorbisfile.h>
#include "vorbis.h"

/* the functions */
int (*gjov_fopen)(char *path, OggVorbis_File *vf);
vorbis_comment *(*gjov_comment)(OggVorbis_File *vf, int link);
double (*gjov_time_total)(OggVorbis_File *vf, int i);
int (*gjov_clear)(OggVorbis_File *vf);

gboolean
gjay_vorbis_dlopen(void) {
  void * lib;
  
  lib = dlopen("libvorbis.so.0", RTLD_GLOBAL | RTLD_LAZY);
  if (!lib) 
    return FALSE;

  lib = dlopen("libvorbisfile.so.3", RTLD_GLOBAL | RTLD_LAZY);
  if (!lib)
    return FALSE;

  if ( (gjov_fopen = 
        gjay_dlsym(lib, "ov_fopen")) == NULL)
    return FALSE;
  if ( (gjov_comment = 
        gjay_dlsym(lib, "ov_comment")) == NULL)
    return FALSE;
  if ( (gjov_time_total = 
        gjay_dlsym(lib, "ov_time_total")) == NULL)
    return FALSE;
  if ( (gjov_clear = 
        gjay_dlsym(lib, "ov_clear")) == NULL)
    return FALSE;
  return TRUE;
}

gboolean
read_ogg_file_type(   gchar    * path,
                      gint     * length,
                      gchar   ** title,
                      gchar   ** artist,
                      gchar   ** album)
{
  OggVorbis_File *vf;
  vorbis_comment *vc;
  int i;

  assert(gjov_fopen);

  vf = g_malloc0(sizeof(OggVorbis_File));

  if ((*gjov_fopen)(path, vf) != 0)
  {
    g_free(vf);
    return FALSE;
  }
  vc = (*gjov_comment)(vf, -1);
  *length = (*gjov_time_total)(vf, -1);
  for (i = 0; i < vc->comments; i++)
  {
    if (strncasecmp(vc->user_comments[i], "title=", 6) == 0)
    {
      *title = strdup_to_utf8(vc->user_comments[i] + 6);
    } else if (strncasecmp(vc->user_comments[i], "artist=", 7) ==0)
    {
      *artist = strdup_to_utf8(vc->user_comments[i] + 7);
    } else if (strncasecmp(vc->user_comments[i], "album=", 6) ==0)
    {
      *album = strdup_to_utf8(vc->user_comments[i] + 6);
    }
  }
  (*gjov_clear)(vf);
  g_free(vf);

  return TRUE;
}


