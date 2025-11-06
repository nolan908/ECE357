/* Generate a series of random potential words, one per line. Each of these "words" consists of between 3 and 10 characters. The number of characters and the value of each character are all chosen randomly. Be sure to
seed the pseudorandom number generator, e.g. from the pid or time of day, so that the output is indeed random from
run to run. For simplicity, pick characters from among the UPPERCASE letters only. If this command has an
argument, that is the limit of how many potential words to generate. Otherwise, if the argument is 0 or missing,
generate words in an endless loop. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h> // for getpid()

#define MIN_LEN 3
#define MAX_LEN 10

int main(int argc, char *argv[]) {
    int limit = 0; // default to infinite
    if (argc > 1) {
        limit = atoi(argv[1]);  // use argument as limit
    } else {
        limit = 0;  // default: infinite
    }

    srand(time(NULL) ^ getpid()); // seeded with time and pid

    int count = 0;
    while (limit == 0 || count < limit) { // if no arguement is given
        int len = MIN_LEN + rand() % (MAX_LEN - MIN_LEN + 1);
        char word[MAX_LEN + 1];

        for (int i = 0; i < len; i++) {
            word[i] = 'A' + rand() % 26; // pick only uppercase
        }
        word[len] = '\0';

        printf("%s\n", word);
        count++;
    }

    return 0;
}
