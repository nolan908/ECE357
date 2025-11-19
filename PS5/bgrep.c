#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>

static unsigned char *g_pattern = NULL;
static size_t g_pattern_len = 0;
static int g_context_bytes = 0;
static int g_had_error = 0, g_had_match = 0;

static jmp_buf g_sigbus_env;
static const char *g_current_filename = NULL;
static void *g_current_map = NULL;
static size_t g_current_map_len = 0;
static int g_current_fd = -1;

void sigbus_handler(int signo) {
    (void)signo;
    if (g_current_filename)
        fprintf(stderr, "SIGBUS received while processing file %s\n", g_current_filename);
    else
        fprintf(stderr, "SIGBUS received\n");
    longjmp(g_sigbus_env, 1);
}

int main(int argc, char *argv[]) {
    int i = 1, have_pattern_file = 0; 
    const char *pattern_file = NULL;

    if (argc == 1) {
        fprintf(stderr,
            "Usage: bgrep [-c context_bytes] -p pattern_file [file...]\n"
            "   or: bgrep [-c context_bytes] pattern [file...]\n");
        return -1;
    }

    if (signal(SIGBUS, sigbus_handler) == SIG_ERR) {
        perror("signal");
        return -1;
    }

    /* parse options -p and -c */
    g_context_bytes = 0;
    while (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        if (strcmp(argv[i], "-p") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr, "-p requires a pattern file\n");
                return -1;
            }
            pattern_file = argv[i];
            have_pattern_file = 1;
            i++;
        } else if (strcmp(argv[i], "-c") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr, "-c requires a context byte count\n");
                return -1;
            }
            g_context_bytes = atoi(argv[i]);
            if (g_context_bytes < 0) g_context_bytes = 0;
            i++;
        } else break;
    }

    /* load pattern */
    if (have_pattern_file) {
        int pfd; struct stat pst; ssize_t rd;
        pfd = open(pattern_file, O_RDONLY);
        if (pfd < 0) {
            fprintf(stderr, "Can't open %s for reading:%s\n", pattern_file, strerror(errno));
            return -1;
        }
        if (fstat(pfd, &pst) < 0) {
            fprintf(stderr, "fstat failed on %s:%s\n", pattern_file, strerror(errno));
            close(pfd);
            return -1;
        }
        if (pst.st_size <= 0) {
            fprintf(stderr, "Empty pattern file %s\n", pattern_file);
            close(pfd);
            return -1;
        }
        g_pattern_len = (size_t)pst.st_size;
        g_pattern = (unsigned char *)malloc(g_pattern_len);
        if (!g_pattern) {
            fprintf(stderr, "malloc failed for pattern:%s\n", strerror(errno));
            close(pfd);
            return -1;
        }
        rd = read(pfd, g_pattern, g_pattern_len);
        if (rd < 0 || (size_t)rd != g_pattern_len) {
            fprintf(stderr, "Error reading pattern file %s:%s\n", pattern_file, strerror(errno));
            close(pfd);
            free(g_pattern);
            g_pattern = NULL; g_pattern_len = 0;
            return -1;
        }
        close(pfd);
    } else {
        const char *pstr;
        if (i >= argc) {
            fprintf(stderr, "No pattern specified\n");
            return -1;
        }
        pstr = argv[i];
        g_pattern_len = strlen(pstr);
        if (g_pattern_len == 0) {
            fprintf(stderr, "Empty pattern\n");
            return -1;
        }
        g_pattern = (unsigned char *)strdup(pstr);
        if (!g_pattern) {
            fprintf(stderr, "strdup failed for pattern:%s\n", strerror(errno));
            g_pattern_len = 0;
            return -1;
        }
        i++;
    }

    /* unified loop over inputs: stdin if no files, else each file */
    int use_stdin = (i >= argc);
    int start = use_stdin ? 0 : i;
    int end   = use_stdin ? 0 : argc - 1;

    for (int idx = start; idx <= end; idx++) {
        const char *fname;
        int fd;
        struct stat st;

        if (use_stdin) {
            fname = "<standard input>";
            fd = STDIN_FILENO;
        } else {
            fname = argv[idx];
            fd = open(fname, O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "Can't open %s for reading:%s\n", fname, strerror(errno));
                g_had_error = 1;
                continue;
            }
        }

        g_current_filename = fname;
        g_current_fd = fd;
        g_current_map = NULL;
        g_current_map_len = 0;

        if (setjmp(g_sigbus_env) != 0) {
            g_had_error = 1;
            if (g_current_map) {
                munmap(g_current_map, g_current_map_len);
                g_current_map = NULL; g_current_map_len = 0;
            }
            if (g_current_fd >= 0 && g_current_fd != STDIN_FILENO) {
                close(g_current_fd);
            }
            g_current_fd = -1;
            g_current_filename = NULL;
            continue;
        }

        if (fstat(fd, &st) < 0) {
            fprintf(stderr, "fstat failed on %s:%s\n", fname, strerror(errno));
            g_had_error = 1;
            if (!use_stdin) close(fd);
            g_current_fd = -1; g_current_filename = NULL;
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "Can't mmap %s: not a regular file\n", fname);
            g_had_error = 1;
            if (!use_stdin) close(fd);
            g_current_fd = -1; g_current_filename = NULL;
            continue;
        }

        if (st.st_size == 0) {
            if (!use_stdin) close(fd);
            g_current_fd = -1; g_current_filename = NULL;
            continue;
        }

        g_current_map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (g_current_map == MAP_FAILED) {
            fprintf(stderr, "mmap failed on %s:%s\n", fname, strerror(errno));
            g_had_error = 1;
            g_current_map = NULL; g_current_map_len = 0;
            if (!use_stdin) close(fd);
            g_current_fd = -1; g_current_filename = NULL;
            continue;
        }
        g_current_map_len = (size_t)st.st_size;

        /* search for pattern */
        {
            unsigned char *base = (unsigned char *)g_current_map;
            size_t n = (size_t)st.st_size;
            for (size_t pos = 0; pos + g_pattern_len <= n; pos++) {
                if (memcmp(base + pos, g_pattern, g_pattern_len) == 0) {
                    g_had_match = 1;
                    if (g_context_bytes <= 0) {
                        printf("%s:%zu\n", fname, pos);
                    } else {
                        size_t ctxt = (size_t)g_context_bytes;
                        size_t ctx_start = (pos > ctxt) ? pos - ctxt : 0;
                        size_t ctx_end = pos + g_pattern_len + ctxt;
                        if (ctx_end > n) ctx_end = n;
                        size_t ctx_len = ctx_end - ctx_start;
                        printf("%s:%zu ", fname, pos);
                        for (size_t j = 0; j < ctx_len; j++) {
                            unsigned char b = base[ctx_start + j];
                            char ch = isprint(b) ? (char)b : '?';
                            if (j > 0) putchar(' ');
                            putchar(ch);
                        }
                        putchar('\n');
                        for (size_t j = 0; j < ctx_len; j++) {
                            unsigned char b = base[ctx_start + j];
                            if (j > 0) putchar(' ');
                            printf("%02X", b);
                        }
                        putchar('\n');
                    }
                }
            }
        }

        munmap(g_current_map, st.st_size);
        g_current_map = NULL; g_current_map_len = 0;
        if (!use_stdin) close(fd);
        g_current_fd = -1; g_current_filename = NULL;

        if (use_stdin) break; /* only one stdin "file" */
    }

    if (g_pattern) free(g_pattern);

    if (g_had_error) return -1;
    if (g_had_match) return 0;
    return 1;
}
