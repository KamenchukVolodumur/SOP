// #define _GNU_SOURCE
// #define _POSIX_C_SOURCE 200809L

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <errno.h>
// #include <fcntl.h>
// #include <limits.h>

// /* Metadane plików, rozmiar fstat() oraz łącza nazwane mkfifo() */
// #include <sys/types.h>
// #include <sys/stat.h>

// /* Wektorowe I/O: writev(), readv(), struct iovec */
// #include <sys/uio.h>

// /* Pamięć wirtualna i mapowanie plików: mmap(), munmap(), msync() */
// #include <sys/mman.h>

// /* Multipleksowanie wejścia/wyjścia: poll(), struct pollfd, POLLIN, POLLHUP */
// #include <poll.h>

// /* Standardowe makro obsługi błędów systemowych (errno) */
// #define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// /* Funkcja informująca o błędnych argumentach CLI */
// void usage(const char *pname) {
//     fprintf(stderr, "USAGE: %s <argumenty>\n", pname);
//     exit(EXIT_FAILURE);
// }

// /* Pomocnicze stałe */
// #ifndef BUF_SIZE
// #define BUF_SIZE 1024
// #endif
// #ifndef MAX_PATH
// #define MAX_PATH 1024
// #endif


// int main( int argc, char ** argv){
//     if(argc < 3){
//         usage("Give all the args");
//     }
//     int fd;
//     if(access(argv[1], F_OK) != 0){
//         fd = open(argv[1], O_CREAT | O_RDWR, 0777);
//         if(fd == -1){
//             ERR("open");
//         }
//         char buff[32*64];
//         int offset = 0;
//         for(int i = 0; i < 32; ++i){
//             for(int j = 0; j < 63; ++j){
//                 buff[offset] = ' ';
//                 offset+=1;
//             }
//             buff[offset] = '\n';
//             offset++;
//         }
//         write(fd,buff ,64*32);
//     }else{
//         fd = open(argv[1], O_RDWR, 0777);
//         off_t file_size = 0;
//         if((file_size = lseek(fd, 0, SEEK_END) )< 0){
//             ERR("lseak");
//         }
//         if(file_size != 2048){
//             write(STDERR_FILENO,  "Ta ksiega to nie przepis!\n", 26);
//             return EXIT_FAILURE;
//         }
//         int file_size2;
//         if((file_size2 = lseek(fd, -file_size, SEEK_CUR)) < 0){
//             ERR("lseak");
//         }
//     }
//     struct pollfd fds[argc - 2];
//     for(int i = 2; i < argc; ++i ){
//         int fifo;
//         if((fifo = open(argv[i], O_RDONLY | O_NONBLOCK)) < 0){
//             if(errno == ENOENT){
//                 printf("Nie ma babci na wieży %s\n", argv[i]);
//                 return EXIT_FAILURE;
//             }
//             ERR("open");
//         }
//         struct stat st;

//         if(fstat(fifo, &st)!=0){
//             ERR("fstat");
//         }
//         if(!S_ISFIFO(st.st_mode)){
//             printf("Nie ma babci na wieży %s\n", argv[i]);
//             return EXIT_FAILURE;
//         }
//         fds[i - 2].fd = fifo;
//         fds[i - 2].events = POLLIN;
//         fds[i - 2].revents = 0;
//     }
//     char buffs[argc - 2][1000];
//     int firstParts[argc - 2];
//     int buff_size = 1000;
//     memset(firstParts, 0, sizeof(firstParts));
//     int closed = 0;
//     while(closed < argc-2){
//         int ret = poll(fds, argc - 2, 5000);
//         if (ret == -1) {
//             ERR("poll");
//         }
//         if (ret == 0) {
//             printf("Minelo 5 sekund bez zadnych danych...\n");
//             continue;
//         }

//         for(int i = 0; i < argc - 2; ++i){
//             if (fds[i].revents & POLLIN) {
//                 int bytes_read;
//                 while((bytes_read = read(fds[i].fd, buffs[i] + firstParts[i], buff_size-1 - firstParts[i])) > 0){
//                     firstParts[i] += bytes_read;
//                     if(buffs[i][firstParts[i] - 1] == '\n'){
//                         //printf("\n\nFirst char -- %c\n\n", buff[0]);
//                         char *end;
//                         int row = strtol(buffs[i], &end, 10); 
//                         if(row <= 32){
//                             //printf("%s\n\n", buff);
//                             //printf("%s", buff + 3);
                            
//                             int currPointer = lseek(fd, 64 * row, 0);
//                             if(currPointer < 0){
//                                 ERR("lseek");
//                             }

//                             int written = write(fd, buffs[i] + 3, firstParts[i] - 4);
//                             write(STDOUT_FILENO, "\nKrok ", 6);
//                             write(STDOUT_FILENO, buffs[i], 3);
//                             write(STDOUT_FILENO, " zapisany\n", 10);
//                             if(written < 0){
//                                 ERR("write");
//                             }
//                             lseek(fd, 0, SEEK_SET);
//                         }else{
//                             printf("\n%s\n", buffs[i]);
//                             write(STDERR_FILENO,"\nbabcia zagadala\n", 18);
//                         }   
//                         firstParts[i] = 0;
//                     }
//                 }
//                 if(bytes_read < 0 && errno != EWOULDBLOCK && errno != EAGAIN){
//                     ERR("read");
//                 }
//             }
//             if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
//                 close(fds[i].fd);
//                 fds[i].fd = -1;
//                 closed++;
//             }
//         }
//     }
//     char *buff = mmap(NULL, 64*32, PROT_READ | PROT_WRITE, MAP_SHARED,fd, 0);
//     if(buff == MAP_FAILED){
//         ERR("mmap");
//     }

//     int count = 0;
//     for(int i = 0; i < 64*32; ++i){
//         if(buff[i] == '~'){
//             count++;
//             buff[i] = 'p';
//             buff[i-1] = 's';
//         } 
//     }
//     msync(buff, 64*32, MS_SYNC);
//     munmap(buff, 64*32);
//     if(close(fd) == -1){
//         ERR("close");
//     }
//     printf("Zmienilismy - %d slow", count);

// }



// #define _GNU_SOURCE
// #define _POSIX_C_SOURCE 200809L

// #define _GNU_SOURCE
// #define _XOPEN_SOURCE 500



// #include <time.h>



// /* Przeglądanie katalogów */
// #include <dirent.h>

// /* Rekurencyjne przeszukiwanie drzewa katalogów */
// #include <ftw.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <errno.h>
// #include <fcntl.h>
// #include <limits.h>

// /* Metadane plików, rozmiar fstat() oraz łącza nazwane mkfifo() */
// #include <sys/types.h>
// #include <sys/stat.h>

// /* Wektorowe I/O: writev(), readv(), struct iovec */
// #include <sys/uio.h>

// /* Pamięć wirtualna i mapowanie plików: mmap(), munmap(), msync() */
// #include <sys/mman.h>

// /* Multipleksowanie wejścia/wyjścia: poll(), struct pollfd, POLLIN, POLLHUP */
// #include <poll.h>

// /* Standardowe makro obsługi błędów systemowych (errno) */
// #define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// /* Funkcja informująca o błędnych argumentach CLI */
// void usage(const char *pname) {
//     fprintf(stderr, "USAGE: %s <argumenty>\n", pname);
//     exit(EXIT_FAILURE);
// }

// /* Pomocnicze stałe */
// #ifndef BUF_SIZE
// #define BUF_SIZE 1024
// #endif
// #ifndef MAX_PATH
// #define MAX_PATH 1024
// #endif

// int main(int argc, char **argv){
//     if(argc < 3){
//         usage("give 2 pathes");
//     }
//     DIR* dirp = NULL;
//     struct dirent* dp;
//     if ((dirp = opendir(argv[1])) == NULL){
//         ERR("opendir");
//     }

//     errno = 0;
//     typedef struct 
//     {
//         int size;
//         char name[256];
//     } statistics;
//     statistics st1[256];
//     struct iovec iov[256];
//     int count = 0;
//     errno = 0;
//     while((dp = readdir(dirp)) != NULL){
//         if(strcmp(".", dp->d_name) == 0){
//             printf("skiped .\n");
//             continue;
//         }
//         if(strcmp("..", dp->d_name) == 0){
//             printf("skiped ..\n");
//             continue;
//         }
//         struct stat st;
//         if(fstatat(dirfd(dirp), dp->d_name, &st, 0) < 0 ){
//             ERR("fstatat");
//         }
//         int size = strlen(dp->d_name);
//         snprintf(st1[count].name, size+2, "%s\n", dp->d_name);
//         printf("%s", st1[count].name);
//         st1[count].size = st.st_size;
//         printf("%d", st1[count].size);
//         iov[count].iov_base = &st1[count];
//         iov[count].iov_len = size+2 + 4;
//         count++;
//         errno = 0;
//     }
//     if(errno != 0){
//         ERR("readdir");
//     }
//     int d = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0777);
//     if(writev(d, iov, count) < 0){
//         ERR("writev");
//     }
//     close(d);
//     closedir(dirp);
// }


#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>

/* Metadane plików, rozmiar fstat() oraz łącza nazwane mkfifo() */
#include <sys/types.h>
#include <sys/stat.h>

/* Wektorowe I/O: writev(), readv(), struct iovec */
#include <sys/uio.h>

/* Pamięć wirtualna i mapowanie plików: mmap(), munmap(), msync() */
#include <sys/mman.h>

/* Multipleksowanie wejścia/wyjścia: poll(), struct pollfd, POLLIN, POLLHUP */
#include <poll.h>

/* Standardowe makro obsługi błędów systemowych (errno) */
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

/* Funkcja informująca o błędnych argumentach CLI */
void usage(const char *pname) {
    fprintf(stderr, "USAGE: %s <argumenty>\n", pname);
    exit(EXIT_FAILURE);
}

/* Pomocnicze stałe */
#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

int main(int argc, char ** argv){
    if( argc < 3 ){
        usage("Too few args");
    }
    int fd ;
    if(access(argv[1], F_OK) != 0){
        int curr = 0;
        char buff[64 * 32];
        for(int i = 0; i < 32; ++i){
            for(int j = 0; j < 63; ++j){
                buff[curr] = ' ';
                curr++;
            }
            buff[curr] = '\n';
            curr++;
        }

        
        fd = open(argv[1], O_RDWR| O_CREAT, 0777);
        if(fd < 0){
            ERR("open");
        }
        if(write(fd, buff, 64 * 32) < 0){
            ERR("write");
        }
    }else{
        fd = open(argv[1], O_RDWR);
        if(fd < 0){
            ERR("open");
        }
        int currPos;
        if((currPos = lseek(fd , 0, SEEK_END)) < 0){
            ERR("lseek");
        }
        if(currPos != 2048){
            write(STDERR_FILENO,"\nTa ksiega nie przepis\n", 23);
            return EXIT_FAILURE;
        }
    }
    struct pollfd fds[argc - 2];
    for(int i = 2; i < argc; ++i){  
        if(access(argv[i], F_OK) != 0){
            write(STDOUT_FILENO, "Nie ma babci na wieży ", 24);
            char numberOfTower = argv[i][strlen(argv[i]) - 1];
            write(STDOUT_FILENO, &numberOfTower, 1);
            write(STDOUT_FILENO, "\n", 1);
            return EXIT_FAILURE;
        }
        int fd_fifo;
        if((fd_fifo = open(argv[i], O_RDONLY | O_NONBLOCK)) < 0){
            ERR("open");
        } 
        struct stat st;
        if(fstat(fd_fifo, &st) < 0){
            ERR("fstat");
        }
        if(!S_ISFIFO(st.st_mode)){
            write(STDOUT_FILENO, "Nie ma babci na wieży ", 24);
            char numberOfTower = argv[i][strlen(argv[i]) - 1];
            write(STDOUT_FILENO, &numberOfTower, 1);
            write(STDOUT_FILENO, "\n", 1);
            return EXIT_FAILURE;
        }
        fds[i-2].fd = fd_fifo;
        fds[i-2].events = POLLIN;
        fds[i-2].revents = 0;
        printf("%d", fds[i-2].fd);
    }
    
    // if(access(argv[2], F_OK) != 0){
    //     write(STDOUT_FILENO, "Nie ma babci na wieży ", 24);
    //     char numberOfTower = argv[2][strlen(argv[2]) - 1];
    //     write(STDOUT_FILENO, &numberOfTower, 1);
    //     write(STDOUT_FILENO, "\n", 1);
    //     return EXIT_FAILURE;
    // }
    // int fd_fifo;
    // if((fd_fifo = open(argv[2], O_RDONLY)) < 0){
    //     ERR("open");
    // } 
    // struct stat st;
    // if(fstat(fd_fifo, &st) < 0){
    //     ERR("fstat");
    // }
    // if(!S_ISFIFO(st.st_mode)){
    //     write(STDOUT_FILENO, "Nie ma babci na wieży ", 24);
    //     char numberOfTower = argv[2][strlen(argv[2]) - 1];
    //     write(STDOUT_FILENO, &numberOfTower, 1);
    //     write(STDOUT_FILENO, "\n", 1);
    //     return EXIT_FAILURE;
    // }

    char buff[argc - 2][500];
    int newBytes[argc - 2];
    int fullMessage[argc - 2];
    memset(fullMessage, 0, sizeof(fullMessage));
    int numberOfOpenFifos = argc - 2;
    while(numberOfOpenFifos > 0){
        int r = poll(fds, argc - 2, 5000);
        if(r < 0){
            ERR("poll");
        }
        if(r == 0){
            write(STDOUT_FILENO, "Babcie dolewają herbatki…\n", 27);
        }

        for(int i = 0; i < argc - 2; ++i){
            if(fds[i].revents & POLLIN){
                printf("POLLIN %d\n", fds[i].fd);
            while((newBytes[i] = read(fds[i].fd, buff[i] + fullMessage[i], 500)) > 0){
                fullMessage[i] += newBytes[i];
                if(buff[i][fullMessage[i] - 1] == '\n'){
                    char *end;
                    int row = strtol(buff[i], &end, 10);
                    if(row >= 32){
                        if( write(STDERR_FILENO, "Babcia się zagadała: ",  23) < 0){
                            ERR("write");
                        }
                        if(write(STDERR_FILENO, buff[i], 2) < 0){
                            ERR("write");
                        }
                        if(write(STDERR_FILENO, "\n", 1) < 0){
                            ERR("write");
                        }
                        fullMessage[i] = 0;
                        continue;
                    }
                    if(lseek(fd, row * 64, 0) < 0){
                        ERR("lseak");
                    }
                    if( write(fd, buff[i]+3, fullMessage[i] - 3 ) < 0){
                        ERR("write");
                    }
                    if(write(STDOUT_FILENO, "Krok zapisany ", 14) < 0){
                        ERR("write");
                    }
                    if(write(STDOUT_FILENO, buff[i], 2) < 0){
                        ERR("write");
                    }
                    if(write(STDOUT_FILENO, "\n", 1) < 0){
                        ERR("write");
                    }
                    if(lseek(fd, 0, 0) < 0){
                        ERR("lseak");
                    }
                    fullMessage[i] = 0;
                }
            }
            if(newBytes[i] < 0){
                if(!(errno == EAGAIN || errno == EWOULDBLOCK)){
                    ERR("read");
                }
            }
            }
            if(fds[i].revents & (POLLHUP | POLLERR)){
                fds[i].fd = -1;
                numberOfOpenFifos--;
                close(fds[i].fd
                );
            }
        }
    }
    close(fd);
}   