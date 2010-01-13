
#ifndef GJAY_DBUS_H
#define GJAY_DBUS_H

#include <dbus/dbus-glib.h>

DBusGConnection * gjay_dbus_connection(void);
gboolean gjay_dbus_is_running(const char *appname);

#endif
