#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "fifo.h"

#define N_ITEMS 100000UL

struct shared_state {
    struct myfifo fifo;
};

static struct shared_state *make_shared_state(void)
{
    struct shared_state *p = mmap(
        NULL,
        sizeof(struct shared_state),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );
    if (p == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    return p;
}

int main(void)
{
    struct shared_state *sh = make_shared_state();

    fifo_init(&sh->fifo);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /* Child: writer */
        for (unsigned long i = 0; i < N_ITEMS; i++) {
            fifo_wr(&sh->fifo, i);
        }
        _exit(0);
    }

    /* Parent: reader */
    int ok = 1;
    for (unsigned long expected = 0; expected < N_ITEMS; expected++) {
        unsigned long v = fifo_rd(&sh->fifo);
        if (v != expected) {
            printf("ERROR: expected %lu, got %lu\n", expected, v);
            ok = 0;
            break;
        }
    }

    wait(NULL);

    if (ok) {
        printf("FIFO 1-writer/1-reader test PASSED for %lu items.\n", N_ITEMS);
    } else {
        printf("FIFO 1-writer/1-reader test FAILED.\n");
    }

    return 0;
}