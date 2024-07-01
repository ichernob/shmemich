#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>//not neede ??
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include "pshm_common.h"

#define LOG_FILE "/tmp/logich.log"

volatile sig_atomic_t done = 0;

void interrupt_handler(int signum) {
    printf("Handling signal %d\n", signum);
    done = 1;
}

int main(int argc, char *argv[]) {
    int fd;
    int fd_log;
    char *shmempath;
    struct shmblock *shm_ptr;
    char buffer[BF_SZ];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s /shm-path\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fd_log = open(LOG_FILE, O_CREAT | O_WRONLY | O_APPEND | O_SYNC, 0666);
    if (fd_log == -1) {
        perror_exit("flogopen");
    }
    signal(SIGTERM, interrupt_handler);
    shmempath = argv[1];

    fd = shm_open(shmempath, O_CREAT | O_EXCL | O_RDWR, 0660);
    if (fd == -1) {
        perror_exit("shm_open");
    }
    if (ftruncate(fd, sizeof(struct shmblock)) == -1) {
        perror_exit("ftruncate");
    }

    shm_ptr = mmap(NULL, sizeof(*shm_ptr), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror_exit("mmap");
    }

    if (sem_init(&shm_ptr->sem1, 1, 0) == -1) {
        perror_exit("sem_init-sem1");
    }
    if (sem_init(&shm_ptr->sem2, 1, MAX_BFS) == -1) {
        perror_exit("sem_init-sem2");
    }
    if (sem_init(&shm_ptr->sem3, 1, 0) == -1) {
        perror_exit("sem_init-sem3");
    }

    if (sem_post(&shm_ptr->sem3) == -1) {//signal to the first any sender.
        perror_exit("sem_post-mutex-sem");
    }

    while (!done) {
        if (sem_wait(&shm_ptr->sem1) == -1) {//wait for signal from sender.
            perror_exit("sem_wait: spool-signal-sem");
        }
        // bf_log_ix used by logger to count buffer to take.
        strcpy(buffer, shm_ptr->bfs[shm_ptr->bf_log_ix]);
        (shm_ptr->bf_log_ix)++;
        if (shm_ptr->bf_log_ix == MAX_BFS) {
            shm_ptr->bf_log_ix = 0;
        }

        if (sem_post(&shm_ptr->sem2) == -1) {//signal to any sender.
            perror_exit("sem_post: buffer-count-sem");
        }

        if (write(fd_log, buffer, strlen(buffer)) != strlen(buffer)) {
            perror_exit("write: logfile");
        }
    }
    shm_unlink(shmempath);
    exit(EXIT_SUCCESS);
}