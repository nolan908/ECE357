#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAXARGS 256

typedef struct {
    char *argv[MAXARGS]; // command + args, NULL-terminated
    char *infile; // for "< file"
    char *outfile; // for "> file"
    char *errfile; // for "2> file"
    int   append_out; // 0: truncate, 1: append (we keep it 0 to match minimum spec)
    int   append_err; // ditto for stderr (unused unless you extend)
} Cmd;

static void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    if (n && (s[n-1] == '\n')) s[n-1] = '\0';
}

static bool is_blank(const char *s) {
    for (; *s; ++s) if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') return false;
    return true;
}

// Very simple tokenizer: split on whitespace.
// Recognize tokens: "<", ">", "2>" as separate tokens or combined forms like "<file" ">file" "2>file".
static int parse_line(char *line, Cmd *cmd) {
    memset(cmd, 0, sizeof(*cmd));

    // Tokenize
    char *tokv[MAXARGS];
    int ntok = 0;

    // Allow combined tokens like "2>file" or "<in"
    char *p = line;
    while (*p) {
        while (*p==' '||*p=='\t') ++p;
        if (!*p) break;

        if (*p=='#') { // comment at beginning of token: ignore rest of line
            break;     // (per spec: only # at line-begin is required, but this is fine)
        }

        // Capture a token until space
        char *start = p;
        while (*p && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r') ++p;
        size_t len = p - start;
        if (len == 0) break;

        // Duplicate token
        char *t = strndup(start, len);
        if (!t) die("oom");
        tokv[ntok++] = t;
        if (ntok >= MAXARGS-1) break;
    }
    tokv[ntok] = NULL;

    // Nothing to do?
    if (ntok == 0) return 0;

    // Build command + detect redirections
    int argc = 0;
    for (int i = 0; i < ntok; ++i) {
        char *t = tokv[i];

        // Handle combined forms: 2>file, >file, <file
        if (!strncmp(t, "2>", 2)) {
            if (t[2]) { cmd->errfile = strdup(t+2); }
            else if (i+1 < ntok) { cmd->errfile = strdup(tokv[++i]); }
            else { fprintf(stderr, "syntax: 2> <file>\n"); goto fail; }
            continue;
        }
        if (t[0] == '>' && t[1] != '>') {
            if (t[1]) { cmd->outfile = strdup(t+1); cmd->append_out = 0; }
            else if (i+1 < ntok) { cmd->outfile = strdup(tokv[++i]); cmd->append_out = 0; }
            else { fprintf(stderr, "syntax: > <file>\n"); goto fail; }
            continue;
        }
        if (t[0] == '<') {
            if (t[1]) { cmd->infile = strdup(t+1); }
            else if (i+1 < ntok) { cmd->infile = strdup(tokv[++i]); }
            else { fprintf(stderr, "syntax: < <file>\n"); goto fail; }
            continue;
        }

        // If you want to extend to >> or 2>>:
        // if (!strncmp(t, ">>", 2)) { cmd->outfile = strdup(t+2); cmd->append_out = 1; continue; }
        // if (!strncmp(t, "2>>", 3)) { cmd->errfile = strdup(t+3); cmd->append_err = 1; continue; }

        // Otherwise, it's argv material
        cmd->argv[argc++] = strdup(t);
    }
    cmd->argv[argc] = NULL;

    // Built-in empty command? (line was only redirects)
    if (argc == 0 && (cmd->infile || cmd->outfile || cmd->errfile)) {
        fprintf(stderr, "nothing to run\n");
        goto fail;
    }

    // free token copies (argv entries are separately duplicated)
    for (int i = 0; i < ntok; ++i) free(tokv[i]);
    return 1;

fail:
    for (int i = 0; i < ntok; ++i) free(tokv[i]);
    return -1;
}

static int run_command(Cmd *cmd) {
    // Built-ins: exit, cd
    if (cmd->argv[0] && strcmp(cmd->argv[0], "exit") == 0) {
        int code = 0;
        if (cmd->argv[1]) code = atoi(cmd->argv[1]);
        exit(code);
    }
    if (cmd->argv[0] && strcmp(cmd->argv[0], "cd") == 0) {
        const char *dir = cmd->argv[1] ? cmd->argv[1] : getenv("HOME");
        if (!dir) dir = "/";
        if (chdir(dir) != 0) perror("cd");
        return 0;
    }

    // Measure "real" wall time
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        // Child: establish redirections (dup2 + close), then exec
        // (Lecture shows dup2 then close the original fd — exactly this pattern.)
        // stdout/stderr messages from shell must not pollute child's stdout, so we print nothing here.

        // Input redirect
        if (cmd->infile) {
            int fd = open(cmd->infile, O_RDONLY);
            if (fd < 0) { perror(cmd->infile); _exit(1); } // child exits 1 on redir error
            if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 stdin"); _exit(1); }
            close(fd);
        }
        // Output redirect
        if (cmd->outfile) {
            int flags = O_WRONLY | O_CREAT | (cmd->append_out ? O_APPEND : O_TRUNC);
            int fd = open(cmd->outfile, flags, 0666);
            if (fd < 0) { perror(cmd->outfile); _exit(1); }
            if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 stdout"); _exit(1); }
            close(fd);
        }
        // Stderr redirect
        if (cmd->errfile) {
            int flags = O_WRONLY | O_CREAT | (cmd->append_err ? O_APPEND : O_TRUNC);
            int fd = open(cmd->errfile, flags, 0666);
            if (fd < 0) { perror(cmd->errfile); _exit(1); }
            if (dup2(fd, STDERR_FILENO) < 0) { perror("dup2 stderr"); _exit(1); }
            close(fd);
        }

        // Clean FD environment: only 0,1,2 should be open for the exec'd program.
        // We’re not leaking a script file because we opened it with O_CLOEXEC (see main).
        // If you want to be extra defensive, you could close everything >2 here.

        // Exec the command (search PATH)
        execvp(cmd->argv[0], cmd->argv);
        // If we got here, exec failed → per spec, exit 127
        perror(cmd->argv[0]);
        _exit(127);
    }

    // PARENT: wait and report status + timing + rusage
    int status = 0;
    struct rusage ru = {0};
    if (wait4(pid, &status, 0, &ru) < 0) {
        perror("wait4");
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double real_s = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec)/1e9;
    double usr_s  = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec/1e6;
    double sys_s  = ru.ru_stime.tv_sec + ru.ru_stime.tv_usec/1e6;

    // Report to stderr
    if (WIFEXITED(status)) {
        fprintf(stderr, "Child process %d exited normally\n", pid);
        fprintf(stderr, "Exit: %d  Real: %.3fs  User: %.3fs  Sys: %.3fs\n",
                WEXITSTATUS(status), real_s, usr_s, sys_s);
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        fprintf(stderr, "Child process %d exited with signal %d\n", pid, sig);
        // Many shells encode 128+signal as exit code; we just show signal as per spec text.
        fprintf(stderr, "Real: %.3fs  User: %.3fs  Sys: %.3fs\n", real_s, usr_s, sys_s);
    } else {
        fprintf(stderr, "Child process %d: unknown status 0x%x\n", pid, status);
    }

    return 0;
}

static int run_loop(FILE *in) {
    // If 'in' is a script file, it was opened O_CLOEXEC by main, so it won’t leak into exec’d children.
    char *line = NULL;
    size_t cap = 0;

    while (1) {
        ssize_t n = getline(&line, &cap, in);
        if (n < 0) {  // EOF
            // Match the assignment’s spirit: print an informational line to stderr
            // (they show an example “end of file read…” line).
            // We’ll use exit status 0 here.
            fprintf(stderr, "end of file read, exiting shell with exit code 0\n");
            free(line);
            return 0;
        }

        trim_newline(line);
        if (is_blank(line)) continue;
        if (line[0] == '#') continue; // comment line

        Cmd cmd;
        int ok = parse_line(line, &cmd);
        if (ok < 0) continue;       // syntax error already reported
        if (ok == 0) continue;      // empty

        if (!cmd.argv[0]) continue; // nothing to do

        run_command(&cmd);

        // Free argv/redirect strings
        for (int i = 0; cmd.argv[i]; ++i) free(cmd.argv[i]);
        free(cmd.infile);
        free(cmd.outfile);
        free(cmd.errfile);
    }
}

int main(int argc, char **argv) {
    // If launched with a single argument: script mode — open file directly (DO NOT redirect stdin),
    // and ensure no leak into child by using O_CLOEXEC (clean FD env) per problem text.
    // (We could also fcntl(fd, F_SETFD, FD_CLOEXEC) if O_CLOEXEC isn’t available.)
    if (argc == 2) {
        int fd = open(argv[1], O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            fprintf(stderr, "cannot open script '%s': %s\n", argv[1], strerror(errno));
            return 127;
        }
        FILE *f = fdopen(fd, "r");
        if (!f) {
            fprintf(stderr, "fdopen failed: %s\n", strerror(errno));
            close(fd);
            return 127;
        }
        return run_loop(f);
    }

    // Interactive (or piped) mode: read from stdin
    return run_loop(stdin);
}
