#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

static volatile sig_atomic_t got_sigpipe = 0;
static void on_sigpipe(int sig) { (void)sig; got_sigpipe = 1; }

static void upcase_inplace(char *s) {
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

static int is_all_letters(const char *s) {
    if (!*s) return 0;
    for (; *s; ++s) if (!(*s >= 'A' && *s <= 'Z')) return 0;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <words.txt>\n", argv[0]);
        return 2;
    }

    //(c) Install SIGPIPE handler so we can report "Matched ..." before exiting
    struct sigaction sa = {0};
    sa.sa_handler = on_sigpipe;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);

    FILE *df = fopen(argv[1], "r");
    if (!df) { perror("open dict"); return 1; }

    size_t cap = 1 << 15, n = 0;
    char **dict = malloc(cap * sizeof *dict);
    if (!dict) { perror("malloc dict"); return 1; }

    char *line = NULL; size_t lcap = 0;
    long accepted = 0, rejected = 0;

    while (getline(&line, &lcap, df) != -1) {
        size_t L = strcspn(line, "\r\n");
        line[L] = '\0';
        upcase_inplace(line);

        if (!is_all_letters(line)) { rejected++; continue; }

        if (n == cap) {
            cap <<= 1;
            char **tmp = realloc(dict, cap * sizeof *tmp);
            if (!tmp) { perror("realloc dict"); return 1; }
            dict = tmp;
        }
        dict[n++] = strdup(line);
        accepted++;
    }
    free(line); line = NULL;
    fclose(df);

    fprintf(stderr, "Accepted %ld words, rejected %ld\n", accepted, rejected);

    // Filter stdin against the dictionary (linear search on purpose)
    long matched = 0;
    lcap = 0;
    while (!got_sigpipe && getline(&line, &lcap, stdin) != -1) {
        size_t L = strcspn(line, "\r\n");
        line[L] = '\0';
        upcase_inplace(line);
        if (!is_all_letters(line)) continue;

        int hit = 0;
        for (size_t i = 0; i < n; i++) {
            if (strcmp(line, dict[i]) == 0) { hit = 1; break; }
        }

        if (hit) {
            // Printing to a closed pipe triggers SIGPIPE; our handler sets got_sigpipe.
            if (printf("%s\n", line) < 0 || got_sigpipe) break;
            matched++;
        }
    }

    fprintf(stderr, "Matched %ld words\n", matched);

    // Cleanup
    free(line);
    for (size_t i = 0; i < n; i++) free(dict[i]);
    free(dict);
    return 0;
}
