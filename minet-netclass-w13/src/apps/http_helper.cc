#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>

/*
void printAll(int sock, FILE * printLocation) {
    char b;
    int error;
    while((error = minet_read(sock, &b, 1)) != 0) {
        if(error < 0) {
            minet_perror("Error reading ");
            return;
        } else {
            fprintf(printLocation, "%c", b);
        }
    }
}
*/
