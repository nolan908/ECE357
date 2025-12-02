#ifndef __SEM_H
#define __SEM_H

#include <sys/types.h>
#include "spinlock.h"

#define N_PROC 64

struct sem {
    spinlock_t lock;
    int count;
    int n_waiters;
    pid_t wait_pids[N_PROC];
};

void sem_init(struct sem *s, int count);
int  sem_try(struct sem *s);
void sem_wait(struct sem *s);
void sem_inc(struct sem *s);

#endif