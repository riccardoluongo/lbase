#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *db;

char *strrev(char *str){
      char *p1, *p2;

      if (! str || ! *str)
            return str;
      for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2){
            *p1 ^= *p2;
            *p2 ^= *p1;
            *p1 ^= *p2;
      }
      return str;
}

int main(int argc, char ** argv){
    if(argc < 2){
        printf("error: not enough arguments\n");
        return -1;
    }
    if(argc == 2){
        FILE *new = fopen(argv[1], "rb");

        if(new != NULL){
            printf("error: file %s already exists\n", argv[1]);
            exit(1);
        }
        if((new = fopen(argv[1], "wb")) == NULL){
            printf("error: can't create file\n");
            exit(1);
        };

        printf("file %s created successfully\n", argv[1]);
        exit(0);
    }
    if(argc < 4 || (argc < 6 && strcmp(argv[2], "-r") != 0)){
        printf("error: target element not specified\n");
        return -1;
    } else if((strcmp(argv[2], "-w") == 0 && argc > 6) || (strcmp(argv[2], "-r") == 0 && argc > 4)){
        printf("error: too many arguments\n");
        return -1;
    }
    for(uint8_t i = 1; i < argc; i++){
        if(strcmp(argv[i], "-h") == 0){
            printf("command format: lbase [filename] [mode [-w, -r]] [type [i, s, b]] [key] [value]\n");
        }
    }

    if(
        argv[2][0] != '-' ||
        (argv[2][1] != 'r' && argv[2][1] != 'w') ||
        (argv[2][1] == 'w' && (strlen(argv[3]) != 1 || (argv[3][0] != 'i' && argv[3][0] != 's' && argv[3][0] != 'b') || strlen(argv[4]) > 31))
    ){
        printf("error: invalid argument format, run with -h for help\n");
        return -1;
    }

    if(strcmp(argv[2], "-w") == 0){
        db = fopen(argv[1], "ab");
        fprintf(db, "%s %c %s\n", argv[4], argv[3][0], argv[5]);
    }
    else if(strcmp(argv[2], "-r") == 0){
        char buf[32], c;
        unsigned long prev_record_end;

        db = fopen(argv[1], "rb");
        fseek(db, -2, SEEK_END);

        while(ftell(db) > 0){
            if((c = fgetc(db)) != '\n')
                fseek(db, -2, SEEK_CUR); //find start of the record
            else{
                uint8_t bufpos = 0;
                prev_record_end = ftell(db) - 2;

                while((c = fgetc(db)) != ' ') //store key
                    buf[bufpos++] = c;
                buf[bufpos] = '\0';

                if(strcmp(buf, argv[3]) == 0){ //key matches
                    fseek(db, 2, SEEK_CUR); //skip value type
                    while(putchar(fgetc(db)) != '\n') //print requested value
                        ;
                    break;
                }

                fseek(db, prev_record_end, SEEK_SET); //skip already processed record
            }
        }
    }

    exit(0);
}