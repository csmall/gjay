/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002 Chuck Groom
 * Copyright (C) 2010 Craig Small 
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
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
