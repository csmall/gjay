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

#ifndef __PLAYLIST__H__
#define __PLAYLIST__H__

GList *     generate_playlist ( guint len );
void        save_playlist     ( GList * list, 
                                gchar * fname );
void        write_playlist    ( GList * list, 
                                FILE  * f,
                                gboolean m3u_format);

#endif /* __PLAYLIST__H__ */
