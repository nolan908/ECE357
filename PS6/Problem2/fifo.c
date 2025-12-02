#include "fifo.h"

void fifo_init(struct myfifo *f)
{
    f->read_pos = 0;
    f->write_pos = 0;

    sem_init(&f->mutex, 1);
    sem_init(&f->empty, MYFIFO_BUFSIZ);
    sem_init(&f->full, 0);
}

void fifo_wr(struct myfifo *f, unsigned long d)
{
    sem_wait(&f->empty);
    sem_wait(&f->mutex);

    f->buf[f->write_pos] = d;
    f->write_pos = (f->write_pos + 1) % MYFIFO_BUFSIZ;

    sem_inc(&f->mutex);
    sem_inc(&f->full);
}

unsigned long fifo_rd(struct myfifo *f)
{
    sem_wait(&f->full);
    sem_wait(&f->mutex);

    unsigned long v = f->buf[f->read_pos];
    f->read_pos = (f->read_pos + 1) % MYFIFO_BUFSIZ;

    sem_inc(&f->mutex);
    sem_inc(&f->empty);
    return v;
}