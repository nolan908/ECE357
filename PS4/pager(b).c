#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    FILE *tty = fopen("/dev/tty", "r");
    if (!tty) { perror("open /dev/tty"); return 1; }

    char *line = NULL;
    size_t cap = 0;
    long shown = 0;

    for (;;) {
        ssize_t n = getline(&line, &cap, stdin);
        if (n == -1) break;

        if (fwrite(line, 1, (size_t)n, stdout) < (size_t)n) {
            // stdout error (should be rare here)
            break;
        }
        shown++;

        if (shown == 23) {
            fputs("---Press RETURN for more---\n", stdout);
            fflush(stdout);

            char reply[128];
            if (!fgets(reply, sizeof reply, tty)) break;      // EOF on tty

            if (reply[0] == 'q' || reply[0] == 'Q') {
                fprintf(stderr, "*** Pager terminated by Q command ***\n");
                free(line);
                fclose(tty);
                return 0;
            }
            shown = 0;
        }
    }

    free(line);
    fclose(tty);
    return 0;
}
