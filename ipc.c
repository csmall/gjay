/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002 Chuck Groom
 * Copyright (C) 2010-2012 Craig Small 
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
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "gjay.h"
#include "ipc.h"
#include "i18n.h"

#define DAEMON_PIPE_FILE "daemon"
#define UI_PIPE_FILE     "ui"

/* Function Definitons */
static int gjay_fifo(const char *dirname, const char *filename);


gboolean
create_gjay_ipc(GjayIPC **ipc)
{
  const gchar *username;

  if ( (username = g_get_user_name()) == NULL)
	return FALSE;

  if ( (*ipc = g_malloc0(sizeof(GjayIPC))) == NULL)
	return FALSE;
  (*ipc)->pipe_dir = g_strdup_printf("%s/gjay-%s",
	  g_get_tmp_dir(), username);
  if (g_file_test((*ipc)->pipe_dir, G_FILE_TEST_IS_DIR) == FALSE) {
	if (mkdir((*ipc)->pipe_dir, 0700) != 0) {
	  g_warning(_("Couldn't create pipe directory '%s'.\n"), (*ipc)->pipe_dir);
	  g_free(*ipc);
	  return FALSE;
	}
  }
  if ( ((*ipc)->ui_fifo = gjay_fifo((*ipc)->pipe_dir, UI_PIPE_FILE)) < 0) {
	g_free(*ipc);
	return FALSE;
  }

  if ( ((*ipc)->ui_fifo = gjay_fifo((*ipc)->pipe_dir, DAEMON_PIPE_FILE)) < 0) {
	g_free(*ipc);
	return FALSE;
  }
  return TRUE;
}

void destroy_gjay_ipc(GjayIPC *ipc) {
  char *pathname;
  close(ipc->daemon_fifo);
  close(ipc->ui_fifo);

  pathname = g_strdup_printf("%s/%s", ipc->pipe_dir, UI_PIPE_FILE);
  if (g_file_test(pathname, G_FILE_TEST_IS_REGULAR) == TRUE)
	unlink(pathname);
  g_free(pathname);

  pathname = g_strdup_printf("%s/%s", ipc->pipe_dir, DAEMON_PIPE_FILE);
  if (g_file_test(pathname, G_FILE_TEST_IS_REGULAR) == TRUE)
  	unlink(pathname);
  g_free(pathname);

  rmdir(ipc->pipe_dir);
  g_free(ipc->pipe_dir);
  g_free(ipc);
}

void send_ipc_text(const int fd, const ipc_type type, const char * text) {
    int len, slen;
    if (fd == -1)
        return;

    slen = strlen(text);
    len = sizeof(ipc_type) + slen;
    if (write(fd, &len,  sizeof(int)) <= 0)
     perror("send_ipc_text(): write length:"); 
    if (write(fd, &type, sizeof(ipc_type)) <= 0)
     perror("send_ipc_text(): write type:"); 
    if (write(fd, text,  slen) <= 0)
     perror("send_ipc_text(): write text:"); 
}


void send_ipc_int (const int fd, const ipc_type type, const int val) {
    int len;
    if (fd == -1)
        return;
    
    len = sizeof(ipc_type) + sizeof(int);
    if (write(fd, &len,  sizeof(int)) <= 0)
     perror("send_ipc_int(): write length:"); 
    if (write(fd, &type, sizeof(ipc_type)) <= 0)
     perror("send_ipc_int(): write type:"); 
    if (write(fd, &val,  sizeof(int)) <= 0)
     perror("send_ipc_int(): write value:"); 
}


void send_ipc (const int fd, const ipc_type type) {
    int len;

    if (fd == -1)
        return;

    len = sizeof(ipc_type);
    if (write(fd, &len,  sizeof(int)) <= 0)
     perror("send_ipc(): write length:"); 
    if (write(fd, &type, sizeof(ipc_type)) <= 0)
     perror("send_ipc(): write type:"); 
}

static int gjay_fifo(const char *dirname, const char *filename) {
  gchar *pathname;
  int fd;

  pathname = g_strdup_printf("%s/%s", dirname, filename);
  if (g_file_test(pathname, G_FILE_TEST_EXISTS) == FALSE) {
	if (mknod(pathname, S_IFIFO | 0777, 0) != 0) {
	  g_warning(_("Couldn't create the pipe '%s'.\n"), pathname);
	  g_free(pathname);
	  return -1;
	}
  }

  if ( (fd = open(pathname, O_RDWR)) < 0) {
	g_warning(_("Couldn't open the pipe '%s'.\n"), pathname);
	g_free(pathname);
	return -1;
  }
  g_free(pathname);
  return fd;
}

