// wordsearch.c — Problem 3C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

static volatile sig_atomic_t got_sigpipe = 0;
static void on_sigpipe(int sig) { (void)sig; got_sigpipe = 1; }

static void upcase_inplace(char *s){
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

static int is_all_AZ(const char *s){
    if (!*s) return 0;
    for (; *s; ++s) if (*s < 'A' || *s > 'Z') return 0;
    return 1;
}

static char *dupstr(const char *s){
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void strip_newline(char *s){
    size_t n = strcspn(s, "\r\n");
    s[n] = '\0';
}

int main(int argc, char **argv){
    if (argc < 2){
        fprintf(stderr, "usage: %s <words.txt>\n", argv[0]);
        return 2;
    }

    //3C: ensure we always print "Matched ..." even if pager quits
    signal(SIGPIPE, on_sigpipe);

    //Load dictionary (uppercase, only A–Z lines)
    FILE *df = fopen(argv[1], "r");
    if (!df){ perror("open dict"); return 1; }

    size_t cap = 2048, n = 0;
    char **dict = (char **)malloc(cap * sizeof *dict);
    if (!dict){ perror("malloc dict"); fclose(df); return 1; }

    long accepted = 0, rejected = 0;
    char buf[4096];

    while (fgets(buf, sizeof buf, df)){
        strip_newline(buf);
        upcase_inplace(buf);
        if (!is_all_AZ(buf)){ rejected++; continue; }

        if (n == cap){
            cap <<= 1;
            char **tmp = (char **)realloc(dict, cap * sizeof *tmp);
            if (!tmp){ perror("realloc dict"); fclose(df); return 1; }
            dict = tmp;
        }
        dict[n] = dupstr(buf);
        if (!dict[n]){ perror("strdup"); fclose(df); return 1; }
        n++; accepted++;
    }
    fclose(df);

    fprintf(stderr, "Accepted %ld words, rejected %ld\n", accepted, rejected);

    //Filter stdin; echo matches; survive SIGPIPE and still report
    long matched = 0;
    char in[256];

    while (!got_sigpipe && fgets(in, sizeof in, stdin)){
        strip_newline(in);
        upcase_inplace(in);
        if (!is_all_AZ(in)) continue;

        int hit = 0;
        for (size_t i = 0; i < n; ++i){
            if (strcmp(in, dict[i]) == 0){ hit = 1; break; }
        }
        if (hit){
            if (puts(in) == EOF || got_sigpipe) break;
            matched++;
        }
    }

    fprintf(stderr, "Matched %ld words\n", matched);

    for (size_t i = 0; i < n; ++i) free(dict[i]);
    free(dict);
    return 0;
}
