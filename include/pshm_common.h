#ifndef PSHM_COMMON_H
#define PSHM_COMMON_H

#include <semaphore.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define perror_exit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while(0)
#define BF_SZ 1024
#define MAX_BFS 4

typedef enum BufferStateEnum: uint8_t {
    READY_FOR_WRITE,
    READY_FOR_READ,
    READING_IN_PROGRESS,
    WRITING_IN_PROGRESS,
} BufferState;

typedef struct buffer_with_control_byte {
    BufferState flag;
    char buffer[BF_SZ];
} ctrl_buffer;

struct shmblock {
    ctrl_buffer ctrl_buffers[MAX_BFS];
    char reserved_area[128];
    sem_t sem1;
    sem_t sem2;
    sem_t sem3;
};

#endif  // include guard
