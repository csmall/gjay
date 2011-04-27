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
#ifndef UTIL_H
#define UTIL_H

#include <glib.h>

void * gjay_dlsym(void *handle, const char const *func_name);
#define strdup_to_utf8(str)   (strdup_convert(str, "UTF8", "LATIN1"))
#define strdup_to_latin1(str) (strdup_convert(str, "LATIN1", "UTF8"))

#ifdef ASSUME_LATIN1
gchar * strdup_convert        ( const gchar * str, 
                                const gchar * enc_to, 
                                const gchar * enc_from );
#else
#define strdup_convert(str, enc_to, enc_from)	(g_strdup(str))
#endif /* ASSUME_LATIN1 */

float   strtof_gjay           ( const char * nptr,
                                char ** endptr);

#endif /* UTIL_H */
