#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "sem.h"

#define N_CHILDREN 8
#define N_ITER     100000

struct shared_region {
    struct sem sem;
    int counter;
};

static struct shared_region *make_shared_region(void)
{
    struct shared_region *p = mmap(
        NULL, sizeof(struct shared_region),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );
    if (p == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    return p;
}

static void mutex_test(void)
{
    printf("=== Semaphore mutex test ===\n");

    struct shared_region *sh = make_shared_region();
    sem_init(&sh->sem, 1);
    sh->counter = 0;

    for (int i = 0; i < N_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(1); }
        if (pid == 0) {
            for (int j = 0; j < N_ITER; j++) {
                sem_wait(&sh->sem);
                sh->counter++;
                sem_inc(&sh->sem);
            }
            _exit(0);
        }
    }

    for (int i = 0; i < N_CHILDREN; i++)
        wait(NULL);

    long expected = (long)N_CHILDREN * N_ITER;
    printf("Expected: %ld, Observed: %d\n\n", expected, sh->counter);
}

static void blocking_test(void)
{
    printf("=== Semaphore blocking test ===\n");

    struct shared_region *sh = make_shared_region();
    sem_init(&sh->sem, 0);
    sh->counter = 0;

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }

    if (pid == 0) {
        printf("Child: calling sem_wait...\n");
        fflush(stdout);

        sem_wait(&sh->sem);

        printf("Child: woke up.\n");
        fflush(stdout);

        sh->counter = 42;
        _exit(0);
    }

    sleep(1);

    printf("Parent: waking child.\n");
    fflush(stdout);
    sem_inc(&sh->sem);

    wait(NULL);
    printf("Parent: child exited, counter = %d\n\n", sh->counter);
}

int main(void)
{
    mutex_test();
    blocking_test();
    return 0;
}