#include "ipc.h"


void send_ipc_text(int fd, ipc_type type, char * text) {
    int len, slen;
    slen = strlen(text);
    len = sizeof(ipc_type) + slen;
    write(fd, &len,  sizeof(int)); 
    write(fd, &type, sizeof(ipc_type)); 
    write(fd, text,  slen);
}


void send_ipc_int (int fd, ipc_type type, int val) {
    int len;
    
    len = sizeof(ipc_type) + sizeof(int);
    write(fd, &len,  sizeof(int)); 
    write(fd, &type, sizeof(ipc_type)); 
    write(fd, &val,  sizeof(int));
}


void send_ipc (int fd, ipc_type type) {
    int len;
    
    len = sizeof(ipc_type);
    write(fd, &len,  sizeof(int)); 
    write(fd, &type, sizeof(ipc_type)); 
}
