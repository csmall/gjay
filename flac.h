/*
 * Gjay - Gtk+ DJ music playlist creator
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
#ifdef HAVE_FLAC_METADATA_H
#ifndef FLAC_H
#define FLAC_H

#include <stdio.h>

gboolean gjay_flac_dlopen(void);
gboolean
read_flac_file_type( gchar    * path,
                      gint     * length,
                      gchar   ** title,
                      gchar   ** artist,
                      gchar   ** album);

#endif /* FLAC_H */
#endif /* HAVE_FLAC_METADATA_H */
