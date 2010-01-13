/**
 * GJay, copyright (c) 2009 Craig Small
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


#ifndef _GJAY_AUDACIOUS_H_
#define _GJAY_AUDACIOUS_H_

#include "gjay.h" 

void      play_song             (song * s );
void      play_songs            (GList * slist );
song *    get_current_audacious_song (void);
gboolean  audacious_is_running(void);

#endif /* _GJAY_AUDACIOUS_H_ */
