#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "spinlock.h"

#define N_PROC 8
#define N_ITER 500000

struct shared_region {
    spinlock_t lock;
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

static void run_test(int use_lock)
{
    printf("=== %s spinlock ===\n", use_lock ? "WITH" : "WITHOUT");

    struct shared_region *sh = make_shared_region();
    spinlock_init(&sh->lock);
    sh->counter = 0;

    for (int i = 0; i < N_PROC; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(1); }
        if (pid == 0) {
            for (int j = 0; j < N_ITER; j++) {
                if (use_lock) spin_lock(&sh->lock);
                sh->counter++;
                if (use_lock) spin_unlock(&sh->lock);
            }
            _exit(0);
        }
    }

    for (int i = 0; i < N_PROC; i++)
        wait(NULL);

    long expected = (long)N_PROC * N_ITER;
    printf("Expected: %ld\nObserved: %d\n\n", expected, sh->counter);
}

int main(void)
{
    run_test(0);
    run_test(1);
    return 0;
}