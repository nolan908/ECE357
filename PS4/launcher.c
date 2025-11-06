//Problem 3B
//Prints: "Child <pid> exited with <code>" where <code> is exit code or signal.

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

static void xperror(const char *msg) { perror(msg); _exit(127); }

static void exec_child(const char *path, char *const argv[]) {
    execv(path, argv);
    xperror("execv");
}

int main(int argc, char **argv) {
    int p12[2], p23[2];
    if (pipe(p12) == -1) { perror("pipe p12"); return 1; }
    if (pipe(p23) == -1) { perror("pipe p23"); return 1; }

    //Child A
    pid_t a = fork();
    if (a == -1) { perror("fork"); return 1; }
    if (a == 0) {
        if (dup2(p12[1], STDOUT_FILENO) == -1) xperror("dup2 wordgen");
        close(p12[0]); close(p12[1]);
        close(p23[0]); close(p23[1]);

        char *gen_argv[3] = {"./wordgen", NULL, NULL};
        if (argc > 1) gen_argv[1] = argv[1];
        exec_child("./wordgen", gen_argv);
    }

    //Child B
    pid_t b = fork();
    if (b == -1) { perror("fork"); return 1; }
    if (b == 0) {
        if (dup2(p12[0], STDIN_FILENO)  == -1) xperror("dup2 wordsearch stdin");
        if (dup2(p23[1], STDOUT_FILENO) == -1) xperror("dup2 wordsearch stdout");
        close(p12[0]); close(p12[1]);
        close(p23[0]); close(p23[1]);

        char *ws_argv[] = {"./wordsearch", "words.txt", NULL};
        exec_child("./wordsearch", ws_argv);
    }

    //Child C
    pid_t c = fork();
    if (c == -1) { perror("fork"); return 1; }
    if (c == 0) {
        if (dup2(p23[0], STDIN_FILENO) == -1) xperror("dup2 pager");
        close(p12[0]); close(p12[1]);
        close(p23[0]); close(p23[1]);

        char *pg_argv[] = {"./pager", NULL};
        exec_child("./pager", pg_argv);
    }

    //Parent
    close(p12[0]); close(p12[1]);
    close(p23[0]); close(p23[1]);

    int status;
    pid_t pid;
    while ((pid = wait(&status)) > 0) {
        int code = 0;
        if (WIFEXITED(status))   code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) code = WTERMSIG(status);
        printf("Child %d exited with %d\n", pid, code);
    }
    return 0;
}
