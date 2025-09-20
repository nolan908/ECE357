#define _POSIX_C_SOURCE 200809L
#include "mylib.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef BUFSIZ
#define BUFSIZ 4096
#endif


enum { MY_READ = 0, MY_WRITE = 1 };

struct MYSTREAM {
    int fd;
    int mode;
    unsigned char *buf;
    size_t cap;
    size_t pos;
    size_t len;
    int eof;
};

//helpers
static int parse_mode(const char *mode, int *out)
{
    if (!mode || !*mode) { errno = EINVAL; return -1; }
    if (mode[0] == 'r' && mode[1] == '\0') { *out = MY_READ;  return 0; }
    if (mode[0] == 'w' && mode[1] == '\0') { *out = MY_WRITE; return 0; }
    errno = EINVAL;
    return -1;
}

static MYSTREAM *alloc_stream(int fd, int mode, int bufsiz)
{
    if (bufsiz <= 0) bufsiz = BUFSIZ;

    MYSTREAM *s = (MYSTREAM*)malloc(sizeof(*s));
    if (!s) return NULL;

    s->buf = (unsigned char*)malloc((size_t)bufsiz);
    if (!s->buf) { free(s); return NULL; }

    s->fd   = fd;
    s->mode = mode;
    s->cap  = (size_t)bufsiz;
    s->pos  = 0;
    s->len  = 0;
    s->eof  = 0;
    return s;
}

//extra-credit (Problem 5)

MYSTREAM *myfopen_ex(const char *pathname, const char *mode, int bufsiz)
{
    int m;
    if (parse_mode(mode, &m) < 0) return NULL;

    int flags, fd;
    if (m == MY_READ) {
        flags = O_RDONLY;
        fd = open(pathname, flags);
    } else {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        fd = open(pathname, flags, 0666);
    }
    if (fd < 0) return NULL;

    MYSTREAM *s = alloc_stream(fd, m, bufsiz);
    if (!s) { int saved = errno; close(fd); errno = saved; return NULL; }
    return s;
}

MYSTREAM *myfdopen_ex(int fd, const char *mode, int bufsiz)
{
    int m;
    if (parse_mode(mode, &m) < 0) return NULL;

    MYSTREAM *s = alloc_stream(fd, m, bufsiz);
    if (!s) return NULL;
    return s;
}

//Problem 3 required API (wrappers to _ex using default BUFSIZ)

MYSTREAM *myfopen(const char *pathname, const char *mode)
{
    return myfopen_ex(pathname, mode, BUFSIZ);
}
MYSTREAM *myfdopen(int filedesc, const char *mode)
{
    return myfdopen_ex(filedesc, mode, BUFSIZ);
}

int myfgetc(MYSTREAM *s)
{
    if (!s) { errno = EINVAL; return -1; }
    if (s->mode != MY_READ) { errno = EBADF; return -1; }

    if (s->eof) { errno = 0; return -1; }

    if (s->pos >= s->len) {

        ssize_t n = read(s->fd, s->buf, s->cap);
        if (n == 0) { s->eof = 1; errno = 0; return -1; }
        if (n < 0) {return -1; }
        s->len = (size_t)n;
        s->pos = 0;
    }

    return (int)s->buf[s->pos++];
}

static int flush_write_buffer(MYSTREAM *s)
{
    size_t to_write = s->pos;
    if (to_write == 0) return 0;

    ssize_t n = write(s->fd, s->buf, to_write);
    if (n < 0) {
        return -1;
    }
    if ((size_t)n != to_write) {
        errno = EIO;
        return -1;
    }
    s->pos = 0;
    return 0;
}

int myfputc(int c, MYSTREAM *s)
{
    if (!s) { errno = EINVAL; return -1; }
    if (s->mode != MY_WRITE) { errno = EBADF; return -1; }

    s->buf[s->pos++] = (unsigned char)c;

    if (s->pos == s->cap) {
        if (flush_write_buffer(s) < 0) return -1;
    }
    return (unsigned char)c;
}

int myfclose(MYSTREAM *s)
{
    if (!s) { errno = EINVAL; return -1; }

    int rc = 0;
    if (s->mode == MY_WRITE) {
        if (flush_write_buffer(s) < 0) rc = -1;
    }

    if (close(s->fd) < 0) rc = -1;

    free(s->buf);
    free(s);
    return rc;
}
