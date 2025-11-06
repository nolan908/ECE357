/* : First read in a list of dictionary words, one per line, from a file that is supplied as the commandline argument. Then read a line at a time from standard input and determine if that potential word matches your word
list. If so, echo the input word (including the newline) to standard output, otherwise ignore it. Id est, wordsearch
is a filter. The program continues until end-of-file on standard input. To simplify, you can convert all of the words in
your word list to UPPERCASE. I don’t care how efficiently you search for each candidate word. In fact, a linear
search will be best because it wastes the most time, and we want this filter to be slow and CPU-intensive. */

/* wordsearch: slow linear filter
   Reads a list of dictionary words from a file (supplied as the command line argument),
   converts them to uppercase, and stores them in memory.
   Then reads potential words from standard input line by line.
   If an input word matches a dictionary entry (case-insensitive),
   echo the word (including newline) to standard output.
   Prints status lines (Accepted/Rejected/Matched) to stderr.
   Uses a linear search intentionally (slow by design). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define MAX_WORD_LEN 128
#define INITIAL_DICT_SIZE 128

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <dictionary-file>\n", argv[0]); // dictionary file name
        return 1; // if none, return 1
    }

    FILE *dictFile = fopen(argv[1], "r"); // open dictionary file
    if (!dictFile) {
        fprintf(stderr, "fopen: %s\n", strerror(errno)); // error handling for file open
        return 1;
    }

    char **dictionary = malloc(INITIAL_DICT_SIZE * sizeof(char *)); // create string pointer to store dictionary words
    if (!dictionary) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        fclose(dictFile);
        return 1;
    }

    size_t dictSize = 0,
           dictCapacity = INITIAL_DICT_SIZE; // how many words loaded
    size_t accepted = 0, rejected = 0; // dictionary stats
    size_t matched = 0;                // matched word count
    char buffer[MAX_WORD_LEN]; // hold each line it's read

    while (fgets(buffer, sizeof(buffer), dictFile)) { // read dict one line at a time
        size_t len = strlen(buffer); // stops EOF

        while (len && isspace((unsigned char)buffer[len - 1])) buffer[--len] = '\0'; // if whitespace, replace with string terminator

        // skips over leading spaces/tabs
        size_t start = 0;
        while (buffer[start] && isspace((unsigned char)buffer[start])) start++;
        if (start) memmove(buffer, buffer + start, strlen(buffer + start) + 1); // shifts word using memmove()
        len = strlen(buffer); // update length
        if (len == 0) continue; // if line empty after trimming, skip

        // verify only letters; reject digits, hyphens, etc.
        int only_letters = 1;
        for (size_t i = 0; i < len; i++) {
            if (!isalpha((unsigned char)buffer[i])) { 
                only_letters = 0;
                break;
            }
        }
        if (!only_letters) { rejected++; continue; }

        for (size_t i = 0; i < len; i++)
            buffer[i] = (char)toupper((unsigned char)buffer[i]); // converts every character to uppercase

        if (dictSize == dictCapacity) {
            size_t newCap = dictCapacity * 2; // double capacity if cap is already full
            char **tmp = realloc(dictionary, newCap * sizeof(*dictionary)); // realloc requests mem block for larger array
            if (!tmp) {
                fprintf(stderr, "realloc: %s\n", strerror(errno)); // see if enough memory
                fclose(dictFile);
                for (size_t j = 0; j < dictSize; j++) free(dictionary[j]); // cleanup if fails
                free(dictionary);
                return 1;
            }
            dictionary = tmp; // update dictionary to point to bigger block
            dictCapacity = newCap;
        }

        dictionary[dictSize] = malloc(len + 1); // read one word into buffer 
        if (!dictionary[dictSize]) { // error handling
            fprintf(stderr, "malloc: %s\n", strerror(errno));
            fclose(dictFile);
            for (size_t j = 0; j < dictSize; j++) free(dictionary[j]);
            free(dictionary);
            return 1;
        }
        memcpy(dictionary[dictSize], buffer, len + 1); // copy trimmed word into dictionary
        dictSize++; // increment word count
        accepted++; // count accepted word
    }
    fclose(dictFile); // done reading dictionary

    fprintf(stderr, "Accepted %zu words, rejected %zu\n", accepted, rejected); // status line 1

    while (fgets(buffer, sizeof(buffer), stdin)) { // read one line at a time
        size_t len = strlen(buffer);

        while (len && isspace((unsigned char)buffer[len - 1])) buffer[--len] = '\0'; // remove trailing whitespace

        size_t start = 0;
        while (buffer[start] && isspace((unsigned char)buffer[start])) start++; // remove leading whitespace
        if (start) memmove(buffer, buffer + start, strlen(buffer + start) + 1);
        len = strlen(buffer);

        char temp[MAX_WORD_LEN]; // holds copy
        strncpy(temp, buffer, sizeof(temp));
        temp[sizeof(temp) - 1] = '\0';
        for (size_t i = 0; i < strlen(temp); i++)
            temp[i] = (char)toupper((unsigned char)temp[i]); // uppercased input word for case insensitive comparison

        int found = 0; // linear search
        for (size_t i = 0; i < dictSize; i++) {
            if (strcmp(temp, dictionary[i]) == 0) { // check if word matches
                found = 1;
                break;
            }
        }

        if (found) {
            printf("%s\n", buffer); // print original line as output from buffer
            matched++; // count matched word
        }
    }

    fprintf(stderr, "Matched %zu words\n", matched); 

    // cleanup
    for (size_t i = 0; i < dictSize; i++) free(dictionary[i]);
    free(dictionary);

    return 0;
}

