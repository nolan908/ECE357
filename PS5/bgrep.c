#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>

static volatile sig_atomic_t sigbus_seen = 0;
static const char *current_file_name = NULL;
static void *current_map = NULL;
static size_t current_map_len = 0;
static sigjmp_buf sigbus_env;

static unsigned char *pattern = NULL;
static size_t pattern_len = 0;
static int context_bytes = 0;

static int had_error = 0;
static int had_match = 0;

static void sigbus_handler(int signo)
{
    (void)signo;
    if (current_file_name) {
        fprintf(stderr, "SIGBUS received while processing file %s\n",
                current_file_name);
    } else {
        fprintf(stderr, "SIGBUS received\n");
    }
    sigbus_seen = 1;
    siglongjmp(sigbus_env, 1);
}

static void search_in_file(const char *fname, int fd, off_t filesize)
{
    if (filesize == 0 || pattern_len == 0) {
        return;
    }

    void *map = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        had_error = 1;
        return;
    }

    current_file_name = fname;
    current_map = map;
    current_map_len = filesize;

    unsigned char *base = (unsigned char *)map;
    size_t n = (size_t)filesize;
    int found_here = 0;

    for (size_t i = 0; i + pattern_len <= n; i++) {
        if (memcmp(base + i, pattern, pattern_len) == 0) {
            found_here = 1;
            had_match = 1;

            if (context_bytes <= 0) {
                printf("%s:%zu\n", fname, i);
            } else {
                size_t ctxt = (size_t)context_bytes;
                size_t ctx_start = (i > ctxt) ? i - ctxt : 0;
                size_t ctx_end = i + pattern_len + ctxt;
                if (ctx_end > n) ctx_end = n;
                size_t ctx_len = ctx_end - ctx_start;

                printf("%s:%zu ", fname, i);

                // ASCII line
                for (size_t j = 0; j < ctx_len; j++) {
                    unsigned char b = base[ctx_start + j];
                    char ch = isprint(b) ? (char)b : '?';
                    if (j > 0) putchar(' ');
                    putchar(ch);
                }
                putchar('\n');

                // Hex line
                for (size_t j = 0; j < ctx_len; j++) {
                    unsigned char b = base[ctx_start + j];
                    if (j > 0) putchar(' ');
                    printf("%02X", b);
                }
                putchar('\n');
            }
        }
    }

    (void)found_here; // only needed if you want a per-file flag

    munmap(map, filesize);
    current_map = NULL;
    current_map_len = 0;
    current_file_name = NULL;
}

int main(int argc, char *argv[])
{
    int opt;
    const char *pattern_file = NULL;
    int pattern_from_file = 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigbus_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGBUS, &sa, NULL) < 0) {
        perror("sigaction");
        return -1;
    }

    // Basic option parsing: -p pattern_file, -c context_bytes
    while ((opt = getopt(argc, argv, "p:c:")) != -1) {
        switch (opt) {
        case 'p':
            pattern_file = optarg;
            pattern_from_file = 1;
            break;
        case 'c':
            context_bytes = atoi(optarg);
            if (context_bytes < 0) context_bytes = 0;
            break;
        default:
            fprintf(stderr, "Usage: bgrep [-c bytes] [-p pattern_file | pattern] [files...]\n");
            return -1;
        }
    }

    // Get pattern
    if (pattern_from_file) {
        if (optind >= argc && pattern_file == NULL) {
            fprintf(stderr, "No pattern file specified\n");
            return -1;
        }
        int pfd = open(pattern_file, O_RDONLY);
        if (pfd < 0) {
            perror("open pattern file");
            return -1;
        }
        struct stat pst;
        if (fstat(pfd, &pst) < 0) {
            perror("fstat pattern file");
            close(pfd);
            return -1;
        }
        if (pst.st_size <= 0) {
            fprintf(stderr, "Empty pattern file\n");
            close(pfd);
            return -1;
        }
        pattern_len = (size_t)pst.st_size;
        pattern = malloc(pattern_len);
        if (!pattern) {
            perror("malloc");
            close(pfd);
            return -1;
        }
        ssize_t rd = read(pfd, pattern, pattern_len);
        if (rd != (ssize_t)pattern_len) {
            perror("read pattern file");
            close(pfd);
            free(pattern);
            return -1;
        }
        close(pfd);
    } else {
        if (optind >= argc) {
            fprintf(stderr, "No pattern specified\n");
            return -1;
        }
        const char *pstr = argv[optind++];
        pattern_len = strlen(pstr);
        if (pattern_len == 0) {
            fprintf(stderr, "Empty pattern\n");
            return -1;
        }
        pattern = (unsigned char *)strdup(pstr);
        if (!pattern) {
            perror("strdup");
            return -1;
        }
    }

    int files_provided = (optind < argc);

    // If no files: use stdin
    if (!files_provided) {
        const char *fname = "<standard input>";
        int fd = STDIN_FILENO;
        struct stat st;
        if (fstat(fd, &st) < 0) {
            perror("fstat");
            had_error = 1;
        } else if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "Can't mmap %s: not a regular file\n", fname);
            had_error = 1;
        } else {
            current_file_name = fname;
            current_map = NULL;
            current_map_len = 0;
            sigbus_seen = 0;

            if (sigsetjmp(sigbus_env, 1) == 0) {
                search_in_file(fname, fd, st.st_size);
            } else {
                // came from SIGBUS
                had_error = 1;
            }
        }
    } else {
        // Process each filename
        for (int i = optind; i < argc; i++) {
            const char *fname = argv[i];
            int fd = open(fname, O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "Can’t open %s for reading:%s\n",
                        fname, strerror(errno));
                had_error = 1;
                continue;
            }
            struct stat st;
            if (fstat(fd, &st) < 0) {
                perror("fstat");
                had_error = 1;
                close(fd);
                continue;
            }
            if (!S_ISREG(st.st_mode)) {
                fprintf(stderr, "Can't mmap %s: not a regular file\n", fname);
                had_error = 1;
                close(fd);
                continue;
            }

            current_file_name = fname;
            current_map = NULL;
            current_map_len = 0;
            sigbus_seen = 0;

            if (sigsetjmp(sigbus_env, 1) == 0) {
                search_in_file(fname, fd, st.st_size);
            } else {
                // came from SIGBUS; search_in_file already cleaned up map
                had_error = 1;
            }

            close(fd);
        }
    }

    free(pattern);

    if (had_error)
        return -1;
    if (had_match)
        return 0;
    return 1;
}
