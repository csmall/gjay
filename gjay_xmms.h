/**
 * GJay, copyright (c) 2002-3 Chuck Groom
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


#ifndef _GJAY_XMMS_H_
#define _GJAY_XMMS_H_

#include "gjay.h" 

void        join_or_start_xmms    ( void );
void        play_song             ( song * s );
void        play_songs            ( GList * slist );
void        play_files            ( GList * list);
song *      get_current_xmms_song ( void );
gboolean    xmms_is_running       ( void );

#endif /* _GJAY_XMMS_H_ */
