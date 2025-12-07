#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "types.h"
#include "helpers.h"

#define BACKLOG 10
#define REQ_BUF_SIZE 2048
#define DEFAULT_CACHE_SIZE 1024
#define MAX_VAL_LEN 1023

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Send error code ERR thru socket SOCK and print an error message based on errno, preceded by PREFIX
// This function handles errors internally since its only purpose is to reduce clutter
void send_err(int8_t err, int sock, char *prefix){
    if(err != 0){
        printf("%d\n", err);
        perror(prefix);
        errno = 0; // We set errno to 0 so in case perror is called again without an error, it does not print the same message again
    }

    if(send(sock, &err, sizeof(err), 0) == -1)
        perror("send");
}

/*
Read a value from CACHE (or fallback to DB) with key WANTED and store it in VAL.
Return values:
    1 on found,
    0 on not found,
    -1 on fgetc failure
    -2 on file formatting error (corrupted data)
    -3 on fseek failure
    -4 on ftell failure
    -5 on fscanf failure
*/
int8_t read_value(const char wanted[32], char val[1024], FILE *db, ht *cache){
    long offset;

    // Check if the value is in cache before reading from disk
    if((offset = ht_get(cache, wanted)) >= 0){
        char record_type;

        if(fseek(db, offset, SEEK_SET) == -1)
            return -3;

        if((record_type = fgetc(db)) == 'r'){
            if(fseek(db, strlen(wanted) + 1, SEEK_CUR) == -1) //Skip separator and key
                return -3;
            return fscanf(db, "%1023s", val) == 1 ? 1 : -5;
        } else if(record_type == 't')
            return 0;
        else if(record_type == EOF)
            return -1;
        else
            return -2;
    } else{
        char record_type[3], key[32], c;
        size_t prev_record_end;
        long ftell_rv;
        int fscanf_rv;

        if(fseek(db, -2, SEEK_END) == -1)
            return -3;

        while((ftell_rv = ftell(db) > 0)){
            if((c = fgetc(db)) != '\n'){
                if(fseek(db, -2, SEEK_CUR) == -1) // Find start of the record
                    return -3;
            } else if(c != EOF){
                if((prev_record_end = ftell(db)) == -1)
                    return -4;

                if((fscanf_rv = fscanf(db, "%2s", record_type)) == 1){
                    if(record_type[0] == 'r'){ //is regular record
                        if((fscanf_rv = fscanf(db, "%31s %1023s", key, val)) == 2){
                            if(strcmp(key, wanted) == 0)
                                return 1;

                            if(fseek(db, prev_record_end - 2, SEEK_SET) == -1)
                                return -3;
                        }
                        else if(fscanf_rv == EOF)
                            return -5;
                        else
                            return -2;
                    }
                    else if(record_type[0] == 't') // Is tombstone
                        break;
                    else
                        return -2;
                }
                else if(fscanf_rv == EOF)
                    return 6;
                else
                    return -2;
            }
            else
                return -1;
        }

        if(ftell_rv == -1)
            return -4;
    }

    return 0;
}

int main(int argc, char ** argv){
    int sockfd, new_fd, yes=1, reqsize, cache_size;
    int16_t rv; // Used to temporarily store return values of various functions
    int8_t args_err = 0; // Indicate if there was an error parsing arguments to avoid repeating error handling code due to nested loops
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    char addr_buf[INET6_ADDRSTRLEN], request_buf[REQ_BUF_SIZE], *filename, *port = "2102", req_mode[3], req_key[32], req_val[1024];
    FILE *db;
    ht *cache_table;

    // Check and parse arguments

    if(argc < 2){
        printf("format: server [-c max_cache_size (optional, default = 256)] [-p port (optional, default = 2102)] [filename]\n");
        return 1;
    } else if(argc == 2)
        filename = argv[1];
    else if(argc == 4 || argc == 6){
        int8_t i;

        for(i = 1; i <= argc - 3; i+=2){ // -3 is to skip the filename and the last argument's value
            if(argv[i][0] == '-'){
                if(argv[i][1] == 'c'){
                    if((cache_size = atoi(argv[i+1]) == 0)){
                        fprintf(stderr, "server: error: invalid cache size provided\n");
                        return 1;
                    }
                }
                else if(argv[i][1] == 'p')
                    port = argv[i+1];
                else
                    args_err = 1;
            }
            else
                args_err = 1;
        }

        filename = argv[i];
    }
    else
        args_err = 1;

    if(args_err){
        fprintf(stderr, "server: error: invalid arguments provided\n");
        return 1;
    }

    // Initialize socket bullshit (why does this have to be so complicated?)

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1){
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1){
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL){
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1){
        perror("listen");
        exit(1);
    }

    // Allocate hash table

    cache_table = ht_alloc(64, cache_size);

    printf("server: now listening on port %s\n", port);

    // Start main server loop

    while(1){
        sin_size = sizeof client_addr;
        if ((new_fd = accept(sockfd, (struct sockaddr *)&client_addr,&sin_size)) == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(client_addr.ss_family,get_in_addr((struct sockaddr *)&client_addr),addr_buf, sizeof addr_buf);
        printf("server: got connection from %s\n", addr_buf);

        if((rv = recv(new_fd, &reqsize, sizeof(reqsize), 0)) != -1 && recvall(new_fd, request_buf, reqsize) == 0){
            sscanf(request_buf, "%2s ", req_mode);

            if((db = fopen(filename, "a+b")) == NULL){
                fprintf(stderr, "server: error: could not open database file\n");

                int8_t error = -5;
                if(send(new_fd, &error, sizeof(error), 0) == -1)
                    perror("send");

                exit(-5);
            }

            if(strcmp(req_mode, "-r") == 0){
                // Is a read request

                sscanf(request_buf + 3, "%31s ", req_key);

                char val[1024];
                val[0] = '\0';

                int8_t read_result = read_value(req_key, val, db, cache_table);
                int16_t val_len;

                char *failed_funcs[] = {"", "fgetc", "", "fseek", "ftell", "fscanf"};

                if(read_result == 1){
                    val_len = strlen(val);
                    if(send(new_fd, &val_len, sizeof(val_len), 0) == -1 || sendall(new_fd, val, val_len) != 0)
                        perror("send");
                } else
                    send_err(read_result, new_fd, failed_funcs[read_result * -1]);
            }
            else if(strcmp(req_mode, "-w") == 0 || strcmp(req_mode, "-d") == 0){
                // Is a write request
                long write_pos;

                sscanf(request_buf + 3, "%31s %1023s ", req_key, req_val);

                if(fseek(db, 0, SEEK_END) != 0){ // Go to the end of the file to make ftell work
                    send_err(-6, new_fd, "fseek");
                    continue;
                }

                if((write_pos = ftell(db)) == 0)
                    fputs("L\n", db); // This is a new file, add file start flag
                else if(write_pos < 0){
                    send_err(-6, new_fd, "ftell");
                    continue;
                }

                write_pos = ftell(db);

                if(fprintf(db, strcmp(req_mode, "-w") == 0 ? "r:%s %s\n" : "t:%s\n", req_key, req_val) >= 1){
                    printf("server: wrote %s to database file\n", req_key);
                    send_err(0, new_fd, "fprintf");
                }
                else{
                    send_err(-1, new_fd, "fprintf");
                    continue;
                }

                fflush(db);

                if((rv = ht_set(cache_table, req_key, write_pos)) == -1){
                    fprintf(stderr, "server: couldn't write to cache: hash table is too big\n");
                    send_err(-2, new_fd, ""); // prefix is empty because we don't want perror to print anything, as this is not a library function
                }
                else if(rv == -2)
                    send_err(-3, new_fd, "server: couldn't write to cache");

                close(new_fd);
            }
            else{
                close(new_fd);
                fprintf(stderr, "server: invalid request mode\n");
                continue;
            }
        } else{
            perror("recv");
            exit(1);
        }
    }
}