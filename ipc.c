#include <string.h>
#include "ipc.h"
#include "gjay.h"

char * daemon_pipe;
char * ui_pipe;
char * gjay_pipe_dir;

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
