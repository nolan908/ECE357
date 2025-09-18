#define _POSIX_C_SOURCE 200809L
#include "mylib.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

static void usage(const char *prog) {
    fprintf(stderr,
      "Usage:\n"
      "  %s [-b bufsiz] -o OUTFILE INFILE\n"
      "  %s [-b bufsiz] -o OUTFILE\n"
      "  %s [-b bufsiz] INFILE\n"
      "  %s [-b bufsiz]\n", prog, prog, prog, prog);
}

//parse positive integer, returns -1 on error
static int parse_pos_int(const char *s) {
    if (!s || !*s) return -1;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (*end != '\0' || v <= 0 || v > 1<<26) return -1; // cap ~67M
    return (int)v;
}

int main(int argc, char **argv)
{
    const char *infile  = NULL;
    const char *outfile = NULL;
    int bufsiz = 0; // 0 => default BUFSIZ in the library

    //very small, explicit option parser for -b and -o
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            if (i+1 >= argc) { usage(argv[0]); return 255; }
            bufsiz = parse_pos_int(argv[++i]);
            if (bufsiz <= 0) { fprintf(stderr, "Invalid -b value\n"); return 255; }
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i+1 >= argc) { usage(argv[0]); return 255; }
            outfile = argv[++i];
        } else if (argv[i][0] == '-') {
            usage(argv[0]); return 255;
        } else {
            if (infile) { usage(argv[0]); return 255; }
            infile = argv[i];
        }
    }

    //Open input
    MYSTREAM *in = NULL, *out = NULL;

    if (infile) {
        in = (bufsiz > 0) ? myfopen_ex(infile, "r", bufsiz)
                          : myfopen(infile, "r");
        if (!in) { fprintf(stderr, "open input '%s': %s\n", infile, strerror(errno)); return 255; }
    } else {
        in = (bufsiz > 0) ? myfdopen_ex(STDIN_FILENO, "r", bufsiz)
                          : myfdopen(STDIN_FILENO, "r");
        if (!in) { perror("fdopen stdin"); return 255; }
    }

    //Open output
    if (outfile) {
        out = (bufsiz > 0) ? myfopen_ex(outfile, "w", bufsiz)
                           : myfopen(outfile, "w");
        if (!out) {
            fprintf(stderr, "open output '%s': %s\n", outfile, strerror(errno));
            myfclose(in);
            return 255;
        }
    } else {
        out = (bufsiz > 0) ? myfdopen_ex(STDOUT_FILENO, "w", bufsiz)
                           : myfdopen(STDOUT_FILENO, "w");
        if (!out) { perror("fdopen stdout"); myfclose(in); return 255; }
    }

    //Core loop: replace '\t' with four spaces
    for (;;) {
        int ch = myfgetc(in);
        if (ch == -1) {
            if (errno == 0) break;            //EOF
            fprintf(stderr, "read error: %s\n", strerror(errno));
            myfclose(in); myfclose(out);
            return 255;
        }

        if (ch == '\t') {
            /* four spaces */
            if (myfputc(' ', out) == -1 ||
                myfputc(' ', out) == -1 ||
                myfputc(' ', out) == -1 ||
                myfputc(' ', out) == -1) {
                fprintf(stderr, "write error: %s\n", strerror(errno));
                myfclose(in); myfclose(out);
                return 255;
            }
        } else {
            if (myfputc(ch, out) == -1) {
                fprintf(stderr, "write error: %s\n", strerror(errno));
                myfclose(in); myfclose(out);
                return 255;
            }
        }
    }

    if (myfclose(in)  < 0) { perror("close input"); }  //fall through
    if (myfclose(out) < 0) { perror("close output"); return 255; }

    return 0; //success per spec
