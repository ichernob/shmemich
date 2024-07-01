#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>

#include "pshm_common.h"

volatile sig_atomic_t done = 0;

void interrupt_handler(int signum) {
    printf("Handling signal %d\n", signum);
    done = 1;
}

int main(int argc, char *argv[]) {
    int fd;
    char *shmempath, *tag_string;
    size_t tag_len;
    struct shmblock *shm_ptr;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s /shm-path tag-string\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGTERM, interrupt_handler);

    shmempath = argv[1];
    tag_string = argv[2];
    tag_len = strlen(tag_string);

    if (tag_len > (1 << 3)) {
        fprintf(stderr, "Tag string is too long, it should not exceed %d characters.\n", (1 << 3));
        exit(EXIT_FAILURE);
    }

    fd = shm_open(shmempath, O_RDWR, 0);
    if (fd == -1) {
        perror_exit("shm_open");
    }

    shm_ptr = mmap(NULL, sizeof(*shm_ptr), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror_exit("mmap");
    }
    char buffer[BF_SZ], *cp;

    printf("Type something:\n");

    while(!done) {
        fgets(buffer, BF_SZ - tag_len - 2, stdin);
        int length = strlen(buffer);
        if (buffer[length - 1] == '\n') {
            buffer[length - 1] = '\0';
        }
        if (sem_wait(&shm_ptr->sem2) == -1) {//wait for signal from logger.
            perror_exit("sem_wait: buffer-count-sem");
        }

        if (sem_wait(&shm_ptr->sem3) == -1) {//wait for first signal from logger or for any signal from other senders.
            perror_exit("sem_wait: mutex-sem");
        }

        time_t now = time(NULL);
        cp = ctime(&now);
        int cplen = strlen(cp);
        if (*(cp + cplen) == '\n') {
            *(cp + cplen) = '\0';
        }
        // bf_ix used by senders to avoid data override.
        sprintf(shm_ptr->bfs[shm_ptr->bf_ix], "[%s]: %d: %s %s\n", tag_string, getpid(), cp, buffer);
        (shm_ptr->bf_ix)++;
        if (shm_ptr->bf_ix == MAX_BFS) {
            shm_ptr->bf_ix = 0;
        }

        if (sem_post(&shm_ptr->sem3) == -1) {// signal to another sender.
            perror_exit("sem_post: mutex-sem");
        }
        if (sem_post(&shm_ptr->sem1) == -1) {//signal to logger.
            perror_exit("sem_post: spool_signal_sem");
        }
        printf("Type something:\n");
    }
    if (munmap(shm_ptr, sizeof(*shm_ptr)) == -1) {
        perror_exit("munmap");
    }
    exit(EXIT_SUCCESS);
}