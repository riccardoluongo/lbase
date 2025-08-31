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

        fprintf(new, "L\n");

        printf("file %s created successfully\n", argv[1]);
        exit(0);
    }
    if(argc < 4 || (argc < 5 && (strcmp(argv[2], "-r") != 0 && strcmp(argv[2], "-d") != 0))){
        printf("error: target element not specified\n");
        return -1;
    } else if((strcmp(argv[2], "-w") == 0 && argc > 5) || ((strcmp(argv[2], "-r") == 0 || strcmp(argv[2], "-d") == 0) && argc > 4)){
        printf("error: too many arguments\n");
        return -1;
    }
    for(uint8_t i = 1; i < argc; i++){
        if(strcmp(argv[i], "-h") == 0){
            printf("command format: lbase [filename] [mode [-w, -r]] [key] [value]\n");
        }
    }

    if(argv[2][0] != '-' || (argv[2][1] != 'r' && argv[2][1] != 'w' && argv[2][1] != 'd') || (argv[2][1] == 'w' && strlen(argv[4]) > 31)){
        printf("error: invalid argument format, run with -h for help\n");
        return -1;
    }

    if(strcmp(argv[2], "-w") == 0)
        fprintf(fopen(argv[1], "ab"), "r:%s %s\n", argv[3], argv[4]);
    else if(strcmp(argv[2], "-r") == 0){
        char c;
        char key[32], val[1024], record_type[3];
        int8_t fscanf_status;
        unsigned long prev_record_end;

        db = fopen(argv[1], "rb");
        fseek(db, -2, SEEK_END);

        while(ftell(db) > 0){
            if((c = fgetc(db)) != '\n')
                fseek(db, -2, SEEK_CUR); //find start of the record
            else{
                prev_record_end = ftell(db) - 2;

                if((fscanf_status = fscanf(db, "%2s", record_type))){

                    if(record_type[0] == 'r'){ //is regular record
                        if((fscanf_status = fscanf(db, "%31s %1023s", key, val) == 2)){
                            if(strcmp(key, argv[3]) == 0){
                                printf("%s\n", val);
                                break;
                            }
                            fseek(db, prev_record_end, SEEK_SET);
                        }
                        else if(fscanf_status == EOF) goto eof;
                        else goto corrupt_err;
                    }
                    else if(record_type[0] == 't') break;//is tombstone
                    else goto corrupt_err;
                }
                else if(fscanf_status == EOF) goto eof;
                else goto corrupt_err;
            }
        }
    } else if(strcmp(argv[2], "-d") == 0)
        fprintf(fopen(argv[1], "ab"), "t:%s\n", argv[3]);

    exit(0);

    corrupt_err:
        printf("error: data is corrupted\n");
        exit(1);
    eof:
        printf("error: fscanf() returned EOF\n");
        exit(1);
}