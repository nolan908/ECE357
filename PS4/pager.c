#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define LINES_PER_PAGE 23
#define MAX_LINE_LEN   2048

int main(void) {
    char line[MAX_LINE_LEN]; // buffer for lines from stdin
    char reply[MAX_LINE_LEN]; // buffer for tty input
    size_t count = 0; 

    FILE *tty = NULL;
#ifdef _WIN32
    tty = fopen("CONIN$", "r"); // Windows (CONIN from Microsoft Docs)
#else
    tty = fopen("/dev/tty", "r"); // for normal POSIX LINUX
#endif

if (!tty) {
    fprintf(stderr, "fopen(/dev/tty): %s\n", strerror(errno));
    return 1;
}

    if (!tty) {
        fprintf(stderr, "fopen(/dev/tty): %s\n", strerror(errno)); // error opening tty
        return 1;
    }

    while (fgets(line, sizeof(line), stdin)) { // read stdin a line at a time
        fputs(line, stdout); // echo to stdout
        count++; 

        if (count == LINES_PER_PAGE) { //page full
            fputs("---Press RETURN for more---", stdout); 
            fflush(stdout); 

            if (!fgets(reply, sizeof(reply), tty)) {
                // if we can't read from tty (error), skip
                break;
            }

            if (reply[0] == 'q' || reply[0] == 'Q') {
                fputc('\n', stdout); 
                fflush(stdout);
                fclose(tty);
                return 0;
            }

            fputc('\n', stdout); //cleanup
            fflush(stdout);
            count = 0; 
        }
    }

    fclose(tty); 
    return 0;

}
