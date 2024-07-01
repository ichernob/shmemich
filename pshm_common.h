#ifndef PSHM_COMMON_H
#define PSHM_COMMON_H

#include <semaphore.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define perror_exit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while(0)
#define BF_SZ 1024
#define MAX_BFS 4

struct shmblock {
    sem_t sem1;
    sem_t sem2;
    sem_t sem3;
    char bfs[MAX_BFS][BF_SZ];
    int bf_ix;
    int bf_log_ix;
};

#endif  // include guard
