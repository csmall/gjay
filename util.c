/*
 * Gjay - Gtk+ DJ music playlist creator
 * util.c - common utilities
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

#include <ctype.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "util.h"
#include "i18n.h"

void *
gjay_dlsym(void *handle, const char const *func_name)
{
  char *error;
  void *sym;

  sym = dlsym(handle, func_name);

  if ((error = dlerror()) != NULL) {
    g_error("Cannot dynamically find libFLAC function %s (%s)",
        func_name, error);
    return NULL;
  }
  return sym;
}

/**
 * Duplicate a string from one encoding to another
 */
gchar * strdup_convert ( const gchar * str, 
                         const gchar * enc_to, 
                         const gchar * enc_from ) {
    gchar * conv;
    gsize b_read, b_written;
    conv = g_convert (str,
                      -1, 
                      enc_to,
                      enc_from,
                      &b_read,
                      &b_written,
                      NULL);
    if (!conv) {
        printf(_("Unable to convert from %s charset; perhaps encoded differently?"), enc_from);
        return g_strdup(str);
    }
    return conv;
}

/**
 * Implement strtof, except make it locale agnostic w/r/t whether a
 * decimal is "," or "."
 */
float strtof_gjay ( const char *nptr, char **endptr) {
    char * end;
    float base = 10.0;
    float divisor;
    float result = 0;

    result = strtol(nptr, &end, 10);
    if ((*end == '.') || (*end == ',')) {
        end++;
        divisor = base;
        while (isdigit(*end)) {
            result += (*end - '0') / divisor;
            divisor *= base;
            end++;
        }
    }
    if (endptr) 
        *endptr = end;
    return result;
}

