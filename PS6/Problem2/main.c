#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdint.h>

#include "fifo.h"

#define N_WRITERS 8
#define N_ITEMS   64000UL

struct shared_state {
    struct myfifo fifo;
};

static struct shared_state *make_shared_state(void)
{
    struct shared_state *p = mmap(
        NULL, sizeof(struct shared_state),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, -1, 0
    );
    if (p == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    return p;
}

static unsigned long make_word(unsigned int wid, unsigned long seq)
{
    return (((unsigned long)wid) << 32) | seq;
}

static void unpack_word(unsigned long w, unsigned int *wid, unsigned long *seq)
{
    *wid = w >> 32;
    *seq = w & 0xffffffffUL;
}

int main(void)
{
    struct shared_state *sh = make_shared_state();
    fifo_init(&sh->fifo);

    printf("Beginning acid test with %d writers, %lu items each\n",
           N_WRITERS, N_ITEMS);

    pid_t reader = fork();
    if (reader < 0) { perror("fork"); exit(1); }

    if (reader == 0) {
        long total = N_WRITERS * N_ITEMS, got = 0;
        long last[N_WRITERS];
        int done[N_WRITERS];

        for (int i = 0; i < N_WRITERS; i++) {
            last[i] = -1;
            done[i] = 0;
        }

        while (got < total) {
            unsigned long w = fifo_rd(&sh->fifo);
            unsigned int wid;
            unsigned long seq;
            unpack_word(w, &wid, &seq);

            if (wid >= N_WRITERS) {
                printf("ERROR: bad writer id %u\n", wid);
                _exit(1);
            }

            if (seq != (unsigned long)(last[wid] + 1)) {
                printf("ERROR: out of sequence from %u\n", wid);
                _exit(1);
            }

            last[wid] = seq;
            got++;

            if (seq == N_ITEMS - 1 && !done[wid]) {
                printf("Reader stream %u completed\n", wid);
                done[wid] = 1;
            }
        }

        printf("Reader: all %ld items received correctly.\n", got);
        _exit(0);
    }

    for (unsigned int wid = 0; wid < N_WRITERS; wid++) {
        pid_t p = fork();
        if (p < 0) { perror("fork"); exit(1); }
        if (p == 0) {
            for (unsigned long seq = 0; seq < N_ITEMS; seq++)
                fifo_wr(&sh->fifo, make_word(wid, seq));
            printf("Writer %u completed\n", wid);
            _exit(0);
        }
    }

    for (int i = 0; i < N_WRITERS + 1; i++)
        wait(NULL);

    printf("All writer and reader processes completed.\n");
    return 0;
}