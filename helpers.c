// Helper functions used both in the client and server programs

#include "helpers.h"

/*
Loops until all len bytes have been sent.
Returns the number of bytes left, so 0 in case of success
*/
int16_t sendall(int socket, char *buf, uint16_t len){
    int16_t total = 0, left = len, rv;

    while(total < len){
        if ((rv = send(socket, buf+total, left, 0)) == -1)
            break;

        total += rv;
        left -= rv;
    }

    return left;
}

/*
Loops until all len bytes have been recv'd.
Returns the number of bytes left, so 0 in case of success
*/
int16_t recvall(int socket, char * buf, uint16_t len){
    int16_t rv, total = 0, left = len;

    while(total < len){
        if((rv = recv(socket, buf+total, left, 0)) == -1)
            break;

        total += rv;
        left -= rv;
    }

    return left;
}
