 /**
 * ipc.h -- interprocess communication between UI, daemon processes
 */
#ifndef __IPC_H__
#define __IPC_H__

#include <stdio.h>
#include <glib.h>
#include <unistd.h>

typedef struct _GjayIPC {
  gchar 		*pipe_dir;
  int			ui_fifo;
  int			daemon_fifo;
} GjayIPC;

typedef enum {
    /* UI process sends... */
    UNLINK_DAEMON_FILE, /* no arg */
    CLEAR_ANALYSIS_QUEUE, /* no arg */
    QUEUE_FILE,       /* str arg */
    QUIT_IF_ATTACHED, /* no arg */
    ATTACH,           /* no arg */
    DETACH,           /* no arg */

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


/* The UI will send a message to an attached daemon at least every
 *  UI_PING ms. If an attached daemon hasn't heard from the UI in
 *  FREAKOUT sec it quits.
 */
#define UI_PING                5000
#define DAEMON_ATTACH_FREAKOUT 20

void send_ipc       (const int fd, const ipc_type type );
void send_ipc_text  (const int fd, const ipc_type type, const char * text);
void send_ipc_int   (const int fd, const ipc_type type, const int val);
gboolean create_gjay_ipc  (GjayIPC **ipc);
void destroy_gjay_ipc     (GjayIPC *ipc);

#endif /* __IPC_H__ */


