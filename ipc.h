 /**
 * ipc.h -- interprocess communication between UI, daemon processes
 */
#ifndef __IPC_H__
#define __IPC_H__

#include <stdio.h>
#include <glib.h>
#include <unistd.h>

typedef enum {
    /* UI process sends... */
    UNLINK_DAEMON_FILE, /* no arg */
    QUEUE_FILE,       /* str arg */
    QUIT_IF_ATTACHED, /* no arg */

    /* Daemon process sends... */ 
    STATUS_PERCENT,   /* int arg */
    STATUS_TEXT,      /* str arg */
    ADDED_FILE,       /* int arg -- seek */
    ANIMATE_START,    /* str arg */
    ANIMATE_STOP,     /* no arg */

    /* Both may send... */
    REQ_ACK,          /* no arg */
    ACK               /* no arg */
} ipc_type;


/* Pipes are named by the sending process */
#define DAEMON_PIPE "/tmp/gjay_daemon"
#define UI_PIPE     "/tmp/gjay_ui"

extern int daemon_pipe_fd;
extern int ui_pipe_fd;

gboolean ui_pipe_input (GIOChannel *source,
                        GIOCondition condition,
                        gpointer data);
gboolean daemon_pipe_input (GIOChannel *source,
                            GIOCondition condition,
                            gpointer data);

void send_ipc       (int fd, ipc_type type );
void send_ipc_text  (int fd, ipc_type type, char * text);
void send_ipc_int   (int fd, ipc_type type, int val);

#endif /* __IPC_H__ */


