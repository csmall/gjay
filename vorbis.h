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
#ifdef HAVE_VORBIS_VORBISFILE_H
#ifndef VORBIS_H
#define VORBIS_H

gboolean gjay_vorbis_dlopen(void);
gboolean
read_ogg_file_type( gchar    * path,
                      gint     * length,
                      gchar   ** title,
                      gchar   ** artist,
                      gchar   ** album);

#endif /* VORBIS_H */
#endif /* HAVE_VORBIS_VORBISFILE_H */
