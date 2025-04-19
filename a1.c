#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <fcntl.h>

#define MAX_BUFFER 1024
#define MAGIC "SF"


    void list_dir(const char* path, int recursive, int size_greater, int check, int* found) {
        struct dirent* entry;
        struct stat file_stat;
        DIR* dir = opendir(path);


        if(!dir) {
            printf("ERROR\ninvalid diretory path");
            return;
        }

        char files[MAX_BUFFER][512];
        int count = 0;

        while((entry = readdir(dir)) != NULL) {
            if(strcmp(entry ->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
                continue;
            }

            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

            if(stat(full_path, &file_stat) == 0){
                int valid = 1;
                
                if(size_greater > 0 && S_ISREG(file_stat.st_mode) && file_stat.st_size <= size_greater) {
                    valid = 0;
                }

                if(check && !(file_stat.st_mode & S_IXUSR)) {
                    valid = 0;
                }

                if(valid) {
                    if(*found == 0) {
                        printf("SUCCESS \n");
                    }

                    *found = 1;
                    strcpy(files[count++], full_path);
                }

                if(recursive && S_ISDIR(file_stat.st_mode)) {
                    list_dir(full_path, recursive, size_greater, check, found);
                }
                
            }
        } 
        closedir(dir);
        
        qsort(files, count, 512, (int (*)(const void *, const void *)) strcmp);
        for(int i = 0; i < count; i++) {
            printf("%s\n", files[i]);
        }
    }


    void parse_dir(const char* path){
        int fd = open(path, O_RDONLY);
        if(fd < 0) {
            printf("ERROR\ninvalid file\n");
            return;
        } 
        char magic;
        if(read(fd, &magic, 1) != 1 || magic != 'x') {
            printf("ERROR\nwrong magic\n");
            close(fd);
            return;
        }
        unsigned short header_size;
        unsigned int version;
        unsigned char no_sections;

        if(read(fd, &header_size, 2) != 2 ||
           read(fd, &version, 4) != 4 ||
           read(fd, &no_sections, 1) != 1) {
            printf("ERROR\ncorupt file \n");
            close(fd);
            return;
           }

           if(version < 82 || version > 150) {
            printf("ERROR\nwrong version\n");
            close(fd);
            return;
           }

           if(no_sections != 2 && (no_sections < 8 || no_sections > 10)) {
            printf("ERROR\nwrong sect_nr\n");
            close(fd);
            return;
           }

           printf("SUCCESS\n");
           printf("version=%d\n", version);
           printf("nr_sections=%d\n", no_sections);

           for(int i = 0; i < no_sections; i++) {
            char name[11] = {0};
            unsigned char type;
            unsigned int offset;
            unsigned int size;

            if(read(fd, name, 11) != 11 ||
                read(fd, &type, 1) != 1 ||
                read(fd, &offset, 4) != 4 ||
                read(fd, &size, 4) != 4) {
                    printf("ERROR\ncorrupt file\n");
                    close(fd);
                    return;
                }
                if(type != 17 && type != 34) {
                    printf("ERROR\nwrong sect_types");
                    close(fd);
                    return;
                }
                printf("section%d: %s %d %d\n", i+1, name,type, size);
           }

    close(fd);
    }
    
    void extract_dir(const char* path, int sect_nr, int line_nr) {
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            printf("ERROR\ninvalid file\n");
            return;
        }
    
        char magic;
        if (read(fd, &magic, 1) != 1 || magic != 'x') {
            printf("ERROR\nwrong magic\n");
            close(fd);
            return;
        }
    
        unsigned short header_size;
        unsigned int version;
        unsigned char no_sections;
    
        if (read(fd, &header_size, 2) != 2 ||
            read(fd, &version, 4) != 4 ||
            read(fd, &no_sections, 1) != 1) {
            printf("ERROR\ncannot read\n");
            close(fd);
            return;
        }
    
        if (version < 82 || version > 150) {
            printf("ERROR\nwrong version\n");
            close(fd);
            return;
        }
    
        if (!(no_sections == 2 || (no_sections >= 8 && no_sections <= 10))) {
            printf("ERROR\nwrong sect_nr\n");
            close(fd);
            return;
        }
    
        if (sect_nr < 1 || sect_nr > no_sections) {
            printf("ERROR\ninvalid section\n");
            close(fd);
            return;
        }
    
        lseek(fd, 8, SEEK_SET);
    
        unsigned int offset = 0;
        unsigned int size = 0;
    
        for (int i = 0; i < no_sections; i++) {
            char name[11];
            unsigned char type;
            unsigned int temp_offset;
            unsigned int temp_size;
            if (read(fd, name, 11) != 11 ||
                read(fd, &type, 1) != 1 ||
                read(fd, &temp_offset, 4) != 4 ||
                read(fd, &temp_size, 4) != 4) {
                printf("ERROR\ncannot read\n");
                close(fd);
                return;
            }
    
            if (i == sect_nr - 1){
                offset = temp_offset;
                size = temp_size;
            }
                
        }
    
        struct stat st;
        fstat(fd, &st);
        if ((off_t)(offset + size) > st.st_size) {
            printf("ERROR\ncannot read\n");
            close(fd);
            return;
        }
    
        lseek(fd, offset, SEEK_SET);
        char* data = (char*)malloc(size);
        if (!data) {
            printf("ERROR\ncannot read\n");
            close(fd);
            return;
        }
    
        if (read(fd, data, size) != size) {
            printf("ERROR\ncannot read\n");
            free(data);
            close(fd);
            return;
        }
    
        int line_start[1024], line_end[1024], count = 0;
        int start = 0;
    
        for (int i = 0; i < size - 1; i++) {
            if (data[i] == '\r' && data[i + 1] == '\n') {
                line_start[count] = start;
                line_end[count] = i;
                count++;
                start = i + 2;
                i++;
            }
        }
    
        if (start < size) {
            line_start[count] = start;
            line_end[count] = size;
            count++;
        }
    
        if (line_nr < 1 || line_nr > count) {
            printf("ERROR\ninvalid line\n");
            free(data);
            close(fd);
            return;
        }
    
        printf("SUCCESS\n");
        int idx = count - line_nr;
        for (int i = line_end[idx] - 1; i >= line_start[idx]; i--) {
            printf("%c", data[i]);
        }
        printf("\n");
    
        free(data);
        close(fd);
    }

    void findall(const char* path, int* found) {
        DIR* dir = opendir(path);
        if(!dir){ 
            return;
        }
        struct dirent* entry;
        struct stat st;

        while((entry = readdir(dir)) != NULL) {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

            if(lstat(full_path, &st) == -1) {
                continue;
            }
            if(S_ISDIR(st.st_mode)) {
                findall(full_path, found);
            }else if(S_ISREG(st.st_mode)){
                int fd = open(full_path, O_RDONLY);
                if(fd < 0) {
                    continue;
                }
                char magic;
                if(read(fd, &magic, 1) != 1 || magic != 'x') {
                    close(fd);
                    continue;
                }

                unsigned short header_size;
                unsigned int version;
                unsigned char no_section;

                if(read(fd, &header_size, 2) != 2 || read(fd, &version, 4) != 4 || read(fd, &no_section, 1) != 1) {
                    close(fd);
                    continue;
                }
                if(!(no_section == 2 || (no_section >= 8 && no_section <= 10)) || version < 82 || version > 150) {
                    close(fd);
                    continue;
                }
                lseek(fd, 8, SEEK_SET);

                int type_count = 0;
                int valid = 1;

                for(int i = 0; i < no_section; i++){
                    char name[11];
                    unsigned char type;
                    unsigned int offset;
                    unsigned int size;

                    if(read(fd, name, 11) != 11 || read(fd, &type, 1) != 1 || read(fd, &offset, 4) != 4 || read(fd, &size, 4) != 4) {
                        valid = 0;
                        break;
                    }
                    if(type == 34) {
                        type_count++;
                    }else if(type != 17) {
                        valid = 0;
                        break;
                    }
                }
                close(fd);
                if(valid && type_count >= 4) {
                    if(*found == 0) {
                        printf("SUCCESS\n");
                        *found = 1;
                    }
                    printf("%s\n", full_path);
                }
            }
        }
        closedir(dir);
    }
    
    

int main(int argc, char **argv)
{
    if(argc >= 2 && strcmp(argv[1], "variant") == 0) {
            printf("76185\n");
            return 0;
    }

    int recursive = 0;
    int size_greater = -1;
    int check = 0;
    char path[1024] = "";
    int found = 0;
    int is_list = 0;
    int is_parse = 0;
    int is_extract = 0;
    int is_findall = 0;
    int sect_nr = -1;
    int line_nr = -1;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "list") == 0) {
            is_list = 1;
        } else if(strcmp(argv[i], "parse") == 0) {
            is_parse = 1;
        }else if(strcmp(argv[i], "recursive") == 0) {
            recursive = 1;
        } else if(strncmp(argv[i], "size_greater=", 13) == 0) {
            size_greater = atoi(argv[i] + 13);
        } else if(strcmp(argv[i], "has_perm_execute") == 0) {
            check = 1;
        } else if(strncmp(argv[i], "path=", 5) == 0) {
            strcpy(path, argv[i] + 5);
        }else if(strcmp(argv[i], "extract") == 0) {
            is_extract = 1;
        } else if(strcmp(argv[i], "findall") == 0){
            is_findall = 1;
        }else if(strncmp(argv[i], "section=", 8) == 0) {
            sect_nr = atoi(argv[i] + 8);
        }else if(strncmp(argv[i], "line=", 5) == 0) {
            line_nr = atoi(argv[i] + 5);
        }
    }

    if(strlen(path) == 0) {
        printf("ERROR\ninvalid directory path \n");
        return 1;
    }

    if(is_list){
        list_dir(path, recursive, size_greater, check, &found);

    if(!found) {
        printf("SUCCESS\n");
    }
} else if(is_parse) {
    parse_dir(path);
} else if(is_extract) {
    if(sect_nr < 1 || line_nr < 1) {
        printf("ERROR\ninvalid section\n");
        return 1;
    }
    extract_dir(path, sect_nr, line_nr);
}else if(is_findall){
    struct stat st;
    if(stat(path, &st) == -1 || !S_ISDIR(st.st_mode)){
        printf("ERROR\ninvalid directory path");
        return 1;
    }
    findall(path, &found);
    if(!found) {
        printf("SUCCESS\n");
    }
}else{
    printf("ERROR\nInvalid command\n");
    return 1;
}
        

    return 0;
}