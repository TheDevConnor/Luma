#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// used to prototype options
enum { SUCCESS, ERROR };
enum {
    FAIL_OPEN = 1,
    FAIL_READ = 2,
    FAIL_WRITE = 3,
    FAIL_ACCESS = 4,
    NONE = 0
};

typedef struct FILE_STRUCT {
    // store the pointer to the file
    FILE *fileptr;
    char *file_name;
    char *file_flag; // r, w, rw
    // position in the file
    int position;

} File;

// file return should return a file struct or error information
typedef struct FILE_RET {
    File *file_core;
    int OK_ERR;
    int FAIL_ON;
} File_Re;

// private used to init the values of the file object
File_Re *file_init(char *file_name, char *file_flag) {
    File_Re *fre = (File_Re *)malloc(sizeof(File_Re));
    if (!fre)
        return NULL;

    File *file = (File *)malloc(sizeof(File));
    if (!file) {
        free(fre);
        return NULL;
    }

    // FLAGS r, w, rw, a, ab, r+, w+,
    file->file_name = file_name;
    file->file_flag = file_flag;
    file->position = 0;
    file->fileptr = NULL;

    fre->file_core = file;
    fre->OK_ERR = SUCCESS;
    fre->FAIL_ON = NONE;

    return fre;
}

// used to open a specific file. Strict error handling so if a empty buffer was
// passed in we say NO
File_Re *file_open(File_Re *fre) {
    if (!fre->file_core) {
        if (!fre)
            return NULL;
        fre->OK_ERR = ERROR;
        fre->FAIL_ON = FAIL_OPEN;
        return fre;
    }

    // always try and open file
    fre->file_core->fileptr =
        fopen(fre->file_core->file_name, fre->file_core->file_flag);
    if (!fre->file_core->fileptr) {
        fre->OK_ERR = ERROR;
        fre->FAIL_ON = FAIL_OPEN;
    } else {
        fre->OK_ERR = SUCCESS;
        fre->FAIL_ON = NONE;
    }
    return fre;
}

// used to close a file object and deinit
File_Re *file_close(File_Re *fre) {
    if (!fre)
        return NULL;

    if (fre->file_core) {
        if (fre->file_core->fileptr) {
            fclose(fre->file_core->fileptr);
            fre->file_core->fileptr = NULL;
        }
        free(fre->file_core);
        fre->file_core = NULL;
    }
    fre->OK_ERR = SUCCESS;
    fre->FAIL_ON = NONE;
    return fre;
}

char *fail_type(int type) {
    char *to_return;
    switch (type) {
    case FAIL_OPEN:
        to_return = "FAIL_OPEN";
        break;
    case FAIL_READ:
        to_return = "FAIL_READ";
        break;
    case FAIL_WRITE:
        to_return = "FAIL_WRITE";
        break;
    case FAIL_ACCESS:
        to_return = "FAIL_ACCESS";
        break;
    case NONE:
        to_return = "NONE";
        break;
    }
    return to_return;
}

int main(void) {

    File_Re *fre = file_init("text.txt", "r");
    if (!fre) {
        printf("Failed to init file\n");
    }
    printf("after init\n");

    fre = file_open(fre);
    if (fre->OK_ERR == ERROR)
        printf("Failed to open file, error: %s\n", fail_type(fre->FAIL_ON));
    else
        printf("After open\n");

    fre = file_close(fre);
    printf("after close\n");

    free(fre);

    return 0;
}
