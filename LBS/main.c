#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Builder {
    char *output;
    char *inputPath;
    char **inputFiles;
    char **importPaths;
    char **importFiles;
};

int main(void) {
    FILE *file = fopen("Lumabuild", "r");
    if (!file) {
        perror("Failed to open file Lumabuild");
        return -1;
    }

    char ch;
    int startOfLine = 1;
    char currentLine[256];
    int linePos = 0;
    char *fileContents[256];
    int lineCount = 0;

    while ((ch = fgetc(file)) != EOF) {
        if (startOfLine) {
            if (ch == '#') {
                // Skip the rest of the line
                while ((ch = fgetc(file)) != EOF && ch != '\n')
                    ;
                startOfLine = 1;
                continue;
            }
        }

        // Store character in current line buffer
        if (ch != '\n' && linePos < 255) {
            currentLine[linePos++] = ch;
        }

        // When we hit newline or EOF, store the completed line
        if (ch == '\n' || ch == EOF) {
            if (linePos > 0) {
                currentLine[linePos] = '\0';
                fileContents[lineCount] = malloc(strlen(currentLine) + 1);
                strcpy(fileContents[lineCount], currentLine);
                lineCount++;
                linePos = 0;
            }
            startOfLine = 1;
        } else {
            startOfLine = 0;
        }
    }

    // Print stored lines
    for (int i = 0; i < lineCount; i++) {
        printf("Line %d: %s\n", i + 1, fileContents[i]);
        free(fileContents[i]); // Clean up
    }

    fclose(file);
    return 0;
}
