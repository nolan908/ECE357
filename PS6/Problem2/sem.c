#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include "sem.h"

static void sem_wakeup_handler(int sig)
{
    (void)sig;
}

void sem_init(struct sem *s, int count)
{
    if (signal(SIGUSR1, sem_wakeup_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    spinlock_init(&s->lock);
    s->count = count;
    s->n_waiters = 0;

    for (int i = 0; i < N_PROC; i++)
        s->wait_pids[i] = 0;
}

int sem_try(struct sem *s)
{
    int ok = 0;
    spin_lock(&s->lock);
    if (s->count > 0) {
        s->count--;
        ok = 1;
    }
    spin_unlock(&s->lock);
    return ok;
}

void sem_wait(struct sem *s)
{
    sigset_t blockset, oldset, suspendset;

    sigemptyset(&blockset);
    sigaddset(&blockset, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &blockset, &oldset) < 0) {
        perror("sigprocmask");
        exit(1);
    }

    for (;;) {
        spin_lock(&s->lock);

        if (s->count > 0) {
            s->count--;
            spin_unlock(&s->lock);
            sigprocmask(SIG_SETMASK, &oldset, NULL);
            return;
        }

        pid_t me = getpid();
        int found = 0;
        for (int i = 0; i < s->n_waiters; i++)
            if (s->wait_pids[i] == me)
                found = 1;

        if (!found && s->n_waiters < N_PROC)
            s->wait_pids[s->n_waiters++] = me;

        spin_unlock(&s->lock);

        suspendset = oldset;
        sigdelset(&suspendset, SIGUSR1);

        if (sigsuspend(&suspendset) == -1 && errno != EINTR) {
            perror("sigsuspend");
            exit(1);
        }
    }
}

void sem_inc(struct sem *s)
{
    spin_lock(&s->lock);
    s->count++;

    for (int i = 0; i < s->n_waiters; i++)
        if (s->wait_pids[i] > 0)
            kill(s->wait_pids[i], SIGUSR1);

    s->n_waiters = 0;
    spin_unlock(&s->lock);
}