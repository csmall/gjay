/*
 * Gjay - Gtk+ DJ music playlist creator
 * dbus.c : Common D-Bus calls
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WITH_DBUSGLIB


#include <dbus/dbus-glib.h>
#include "gjay.h"
#include "dbus.h"
#include "i18n.h"

DBusGConnection *
gjay_dbus_connection(void)
{
  DBusGConnection *connection;
  GError *error=0;

  connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
  if (connection == NULL) {
    g_printerr(_("Failed to open connection to dbus bus: %s\n"),
        error->message);
    g_error_free(error);
    return NULL;
  }
  return connection;
}

gboolean
gjay_dbus_is_running(const char *appname, DBusGConnection *connection)
{
  GError *error=0;
  DBusGProxy *dbus;
  gboolean running = FALSE;

  dbus = dbus_g_proxy_new_for_name(connection,
      DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);
  if (!dbus)
    return FALSE;
  if (!dbus_g_proxy_call_with_timeout(dbus, "NameHasOwner", 5, &error,
        G_TYPE_STRING, appname,
        G_TYPE_INVALID,
        G_TYPE_BOOLEAN, &running,
        G_TYPE_INVALID)) {
    return FALSE;
  }
  return running;
}

#endif /* WITH_DBUSGLIB */
