#include <stdint.h>
#include <sys/socket.h>

int16_t sendall(int socket, char *buf, uint16_t len);
int16_t recvall(int socket, char * buf, uint16_t len);