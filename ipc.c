/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002 Chuck Groom
 * Copyright (C) 2010-2011 Craig Small 
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
#include "ipc.h"
#include "gjay.h"

char * daemon_pipe;
char * ui_pipe;
gchar * gjay_pipe_dir;

void send_ipc_text(int fd, ipc_type type, char * text) {
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


void send_ipc_int (int fd, ipc_type type, int val) {
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


void send_ipc (int fd, ipc_type type) {
    int len;

    if (fd == -1)
        return;

    len = sizeof(ipc_type);
    if (write(fd, &len,  sizeof(int)) <= 0)
     perror("send_ipc(): write length:"); 
    if (write(fd, &type, sizeof(ipc_type)) <= 0)
     perror("send_ipc(): write type:"); 
}
