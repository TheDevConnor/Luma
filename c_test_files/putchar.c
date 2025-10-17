#include <stdlib.h>
#include <stdio.h>

int putchar_custom(int c) {
    char cmd[16];
    if (c == '\n') {
        system("echo");
    } else {
        // Print single character without newline using printf -n
        // Some shells don't support -n, so fallback is printf '%c'
        snprintf(cmd, sizeof(cmd), "printf '%c'", c);
        system(cmd);
    }
    return c;
}

int main(void) {
    putchar_custom('H');
    putchar_custom('i');
    putchar_custom('!');
    putchar_custom('\n');
    return 0;
}
