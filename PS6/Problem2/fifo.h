#ifndef __FIFO_H
#define __FIFO_H

#include "sem.h"

#define MYFIFO_BUFSIZ 4096

struct myfifo {
    unsigned long buf[MYFIFO_BUFSIZ];
    int read_pos;
    int write_pos;

    struct sem mutex;
    struct sem empty;
    struct sem full;
};

void fifo_init(struct myfifo *f);
void fifo_wr(struct myfifo *f, unsigned long d);
unsigned long fifo_rd(struct myfifo *f);

#endif