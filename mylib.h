#ifndef MYLIB_H
#define MYLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct MYSTREAM MYSTREAM;


MYSTREAM *myfopen(const char *pathname, const char *mode);
MYSTREAM *myfdopen(int filedesc, const char *mode);
int myfgetc(MYSTREAM *stream);
int myfputc(int c, MYSTREAM *stream);
int myfclose(MYSTREAM *stream);

//Problem 5 (extra credit)
MYSTREAM *myfopen_ex(const char *pathname, const char *mode, int bufsiz);
MYSTREAM *myfdopen_ex(int fd, const char *mode, int bufsiz);

#ifdef __cplusplus
}
#endif
#endif
