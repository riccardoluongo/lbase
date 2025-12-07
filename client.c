#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "helpers.h"

#define MAXDATASIZE 1200 // maximum size of the request buffer

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char ** argv){
    int sockfd, numbytes, rv; //rv is used for return values of various functions
    char request_buf[MAXDATASIZE], addr_buf[INET6_ADDRSTRLEN];
    struct addrinfo hints, *servinfo, *p;
    char *addr = argv[1], *port;

    if(argc < 3){
        printf("error: not enough arguments\n");
        return -1;
    }
    if(argc < 4 || (argc < 5 && (strcmp(argv[2], "-r") != 0 && strcmp(argv[2], "-d") != 0))){
        printf("error: target element not specified\n");
        return -1;
    } else if((strcmp(argv[2], "-w") == 0 && argc > 5) || ((strcmp(argv[2], "-r") == 0 || strcmp(argv[2], "-d") == 0) && argc > 4)){
        printf("error: too many arguments\n");
        return -1;
    }

    for(uint8_t i = 1; i < argc; i++)
        if(strcmp(argv[i], "-h") == 0)
            printf("command format: lbase [addr:port] [mode [-w, -r]] [key] [value]\n");

    if(argv[2][0] != '-' || (argv[2][1] != 'r' && argv[2][1] != 'w' && argv[2][1] != 'd') || (argv[2][1] == 'w' && strlen(argv[4]) > 31)){
        printf("error: invalid argument format, run with -h for help\n");
        return -1;
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    while(*argv[1] != '\0'){ //parse address and port from argv
        if(*argv[1]++ == ':'){
            port = argv[1];
            *--argv[1] = '\0';

            break;
        }
    }

    request_buf[0] = '\0';
    for(uint8_t i = 1; i < argc; i++){ //create request by concatenating args
        strcat(request_buf, argv[i]);
        strcat(request_buf, " ");
    }

    if ((rv = getaddrinfo(addr, port, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; ; p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("client: socket");
            continue;
        }

        inet_ntop(p->ai_family,get_in_addr((struct sockaddr *)p->ai_addr),addr_buf, sizeof addr_buf);

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            if(p->ai_next == NULL){
                perror("client: connect");
                exit(-4);
            }

            continue;
        }

        break;
    }

    if (p == NULL){
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family,get_in_addr((struct sockaddr *)p->ai_addr),addr_buf, sizeof addr_buf);

    rv = strlen(request_buf);
    if (send(sockfd, &rv, sizeof(rv), 0) == -1 || sendall(sockfd, request_buf, rv) != 0){
        perror("send");
        exit(-4);
    }

    if(argv[2][1] == 'r'){
        int16_t response_code;
        char *response_buf;

        if(recv(sockfd, &response_code, sizeof(response_code), 0) == -1){
            perror("recv");
            exit(-4);
        }

        switch(response_code){
            case 0:
                exit(1);
            case -1: case -3: case -4: case -5:
                fprintf(stderr, "error: I/O operation failed. check the server\n");
                exit(response_code);
            case -2:
                fprintf(stderr, "error: server detected unrecognized record type\n");
                exit(-2);
            default:
                response_buf = malloc(response_code + 1);

                if(recvall(sockfd, response_buf, response_code) != 0){
                    perror("recv");
                    exit(-4);
                }

                response_buf[response_code] = '\0';
                puts(response_buf);

                exit(0);
        }
    } else{
        int8_t response;

        if(recv(sockfd, &response, sizeof(response), 0) == -1){
            perror("recv");
            exit(-4);
        }

        switch(response){
            case -1:
                fprintf(stderr, "error: could not write to database\n");
                exit(-1);
            case -2:
                fprintf(stderr, "error: could not write to cache: hash table is too big\n");
                exit(-2);
            case -3:
                fprintf(stderr, "error: could write to cache: cannot allocate more memory\n");
                exit(-3);
            case -5:
                fprintf(stderr, "error: server failed to open database file\n");
                exit(-5);
            case -6:
                fprintf(stderr, "error: I/O operation failed. Check the server log\n");
                exit(-6);
            default:
                exit(0);
        }
    }
}