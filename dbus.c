/**
 * GJay, copyright (c) 2009 Craig Small
 *
 * dbus.c : Common D-Bus calls
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
#include <dbus/dbus-glib.h>
#include "gjay.h"
#include "dbus.h"

DBusGConnection *
gjay_dbus_connection(void)
{
  DBusGConnection *connection;
  GError *error=0;

  connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
  if (connection == NULL) {
    g_printerr("Failed to open connection to bus: %s\n",
        error->message);
    g_error_free(error);
    return NULL;
  }
  return connection;
}

gboolean
gjay_dbus_is_running(const char *appname)
{
  GError *error=0;
  DBusGProxy *dbus;
  gboolean running = FALSE;

  dbus = dbus_g_proxy_new_for_name(gjay->connection,
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
  fprintf(stderr, "Is %s running: %s\n",appname,(running ? "yes" : "no"));
  return running;
}

