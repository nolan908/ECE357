#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fnmatch.h>
#include <limits.h>
#include <dirent.h>
#include <locale.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <inttypes.h>

/* 
 *  -l : print verbose info for each node (like "find -ls")
 *  -x : do not cross onto other filesystems (stay on same st_dev)
 *  -n : filter names against a shell pattern (fnmatch)
  */

// Command-line flags
static int flag_long = 0;            // -l
static int flag_xdev = 0;            // -x
static const char *name_pat = NULL;  // -n pattern

static dev_t start_dev = (dev_t)-1;  // starting device (for -x)

// Join two path components safely into dst.
static int join_path(char dst[PATH_MAX], const char *a, const char *b) {
    int n = snprintf(dst, PATH_MAX, "%s/%s", a, b);
    return (n < 0 || (size_t)n >= PATH_MAX) ? -1 : 0;
}

// Check for empty string or illegal "/" in a name.
static int invalid_component(const char *name) {
    if (!name || !*name) return 1;
    for (const unsigned char *p = (const unsigned char*)name; *p; ++p) {
        if (*p == '/') return 1;
    }
    return 0;
}

// Convert mode bits into something like "drwxr-xr-x"
static void mode_to_string(mode_t m, char out[11]) {
    out[0] = S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : S_ISCHR(m) ? 'c' :
             S_ISBLK(m) ? 'b' : S_ISFIFO(m) ? 'p' : S_ISSOCK(m) ? 's' : '-';

    const char rwx[] = {'r','w','x'};
    for (int i = 0; i < 9; i++) {
        out[i+1] = (m & (1 << (8 - i))) ? rwx[i%3] : '-';
    }
    if (m & S_ISUID) out[3] = (out[3] == 'x') ? 's' : 'S';
    if (m & S_ISGID) out[6] = (out[6] == 'x') ? 's' : 'S';
    if (m & S_ISVTX) out[9] = (out[9] == 'x') ? 't' : 'T';
    out[10] = '\0';
}

// Format modification time like ls does (recent files show HH:MM, old ones show year)
static void time_to_ls(const struct timespec *ts, char *buf, size_t bufsz) {
    time_t now = time(NULL);
    time_t t = ts->tv_sec;
    struct tm tmv, nowtm;
    localtime_r(&t, &tmv);
    localtime_r(&now, &nowtm);

    const double SIX_MONTHS = 15552000.0; // ~180 days
    if (difftime(now, t) >= -SIX_MONTHS && difftime(now, t) <= SIX_MONTHS) {
        strftime(buf, bufsz, "%b %e %H:%M", &tmv);
    } else {
        strftime(buf, bufsz, "%b %e  %Y", &tmv);
    }
}

// Print a line of output in -l (verbose) mode
static void print_ls_line(const char *path, const char *name, const struct stat *sb, int is_symlink) {
    printf("%ju ", (uintmax_t)sb->st_ino);            // inode
    printf("%jd ", (intmax_t)(sb->st_blocks / 2));   // blocks in 1K units

    char mstr[11];
    mode_to_string(sb->st_mode, mstr);
    printf("%s ", mstr);

    printf("%ju ", (uintmax_t)sb->st_nlink);          // link count

    struct passwd *pw = getpwuid(sb->st_uid);
    struct group  *gr = getgrgid(sb->st_gid);
    if (pw) printf("%s ", pw->pw_name); else printf("%u ", sb->st_uid);
    if (gr) printf("%s ", gr->gr_name); else printf("%u ", sb->st_gid);

    if (S_ISCHR(sb->st_mode) || S_ISBLK(sb->st_mode)) {
        printf("%u, %u ", major(sb->st_rdev), minor(sb->st_rdev));
    } else {
        printf("%jd ", (intmax_t)sb->st_size);
    }

    char tbuf[64];
    struct timespec mt = { .tv_sec = sb->st_mtime, .tv_nsec = 0 };
    #ifdef st_mtim
    mt = sb->st_mtim;
    #endif
    time_to_ls(&mt, tbuf, sizeof tbuf);
    printf("%s ", tbuf);

    fputs(path, stdout);

    if (is_symlink) {
        char tgt[PATH_MAX];
        ssize_t n = readlink(path, tgt, sizeof(tgt)-1);
        if (n >= 0) {
            tgt[n] = '\0';
            printf(" -> %s", tgt);
        }
    }
    putchar('\n');
}

// Decide whether to print this node, depending on -n filter and -l flag
static void visit_node(const char *dirpath, const char *name, const char *fullpath, const struct stat *sb) {
    if (name_pat && fnmatch(name_pat, name, 0) != 0) {
        return; // skip if it doesn’t match
    }

    if (flag_long) {
        int is_lnk = S_ISLNK(sb->st_mode) ? 1 : 0;
        print_ls_line(fullpath, name, sb, is_lnk);
    } else {
        puts(fullpath);
    }
}

// Walk through a directory recursively
static void explore_directory(const char *dirpath) {
    DIR *dp = opendir(dirpath);
    if (!dp) {
        fprintf(stderr, "Warning: Unable to open directory '%s': %s\n", dirpath, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        const char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        if (invalid_component(name)) continue;

        char fullpath[PATH_MAX];
        if (join_path(fullpath, dirpath, name) < 0) {
            fprintf(stderr, "Warning: path too long, skipping '%s/%s'\n", dirpath, name);
            continue;
        }

        struct stat sb;
        if (lstat(fullpath, &sb) == -1) {
            fprintf(stderr, "Warning: lstat failed for '%s': %s\n", fullpath, strerror(errno));
            continue;
        }

        visit_node(dirpath, name, fullpath, &sb);

        if (S_ISDIR(sb.st_mode)) {
            if (flag_xdev && start_dev != (dev_t)-1 && sb.st_dev != start_dev) {
                continue; // don’t cross to another device
            }
            explore_directory(fullpath);
        }
    }
    closedir(dp);
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    const char *startpath = ".";

    int opt;
    while ((opt = getopt(argc, argv, "lxn:")) != -1) {
        switch (opt) {
            case 'l': flag_long = 1; break;
            case 'x': flag_xdev = 1; break;
            case 'n': name_pat = optarg; break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-x] [-n pattern] [starting_path]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        startpath = argv[optind];
    }

    if (flag_xdev) {
        struct stat s0;
        if (lstat(startpath, &s0) == -1) {
            fprintf(stderr, "Error: cannot lstat('%s'): %s\n", startpath, strerror(errno));
            return EXIT_FAILURE;
        }
        start_dev = s0.st_dev;
    }

    struct stat sb;
    if (lstat(startpath, &sb) == -1) {
        fprintf(stderr, "Error: cannot lstat('%s'): %s\n", startpath, strerror(errno));
        return EXIT_FAILURE;
    }

    char startbuf[PATH_MAX];
    if (snprintf(startbuf, sizeof startbuf, "%s", startpath) >= (int)sizeof startbuf) {
        fprintf(stderr, "Error: starting path too long\n");
        return EXIT_FAILURE;
    }

    const char *base = strrchr(startbuf, '/');
    base = base ? base + 1 : startbuf;
    if (!invalid_component(base)) {
        visit_node("", base, startbuf, &sb);
    }

    if (S_ISDIR(sb.st_mode)) {
        explore_directory(startbuf);
    }

    return EXIT_SUCCESS;
}
