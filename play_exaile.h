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

#ifndef _GJAY_EXAILE_H_
#define _GJAY_EXAILE_H_

#include "gjay.h" 

#define EXAILE_DBUS_SERVICE "org.exaile.DBusInterface"
#define EXAILE_DBUS_PATH "/DBusInterfaceObject"
#define EXAILE_DBUS_INTERFACE "org.exaile.DBusInterface"

void      exaile_init(void);
song*     exaile_get_current_song(void);
gboolean  exaile_is_running(void);
void      exaile_play_files(GList *list);
gboolean  exaile_start(void);

#endif /* _GJAY_EXAILE_H_ */
