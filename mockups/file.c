#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// // what functions
enum { SUCCESS, ERROR };

typedef struct FILE {
    // store the pointer to the file
    FILE *fileptr;
    char *file_name;
    char *file_flag; // r, w, rw
    // tmp buffer to edit or use as a medium
    char *buffer;
    // position in the file
    int position;
    // Can be user defined so that a custom function can be run on file contents
    // or buffer flag 0 representing file flag 1 representing buffer
    void *(*fptr)(FILE *, int flag);
} File;
// file return should return a file struct or error information
typedef struct FILE_RET {
    File *file_core;
    int type;
    char *return_log;
} File_Re;

// private used to init the values of the file object
File_Re *finit(File *file, char *file_name, char *file_flag) {
    File_Re *fre;
    // FLAGS r, w, rw, a, ab, r+, w+,
    file = fre->file_core;
    file->file_name = file_name;
    file->file_flag = file_flag;
    file->position = 0;
    file->buffer = NULL;
    file->fileptr = NULL;
}

// used to open a specific file
File *file_open(char *file_name, char *file_flag, char *buffer) {
    File_Re *file;
    file->file_core = finit(file, file_name, file_flag);
    if (!buffer) {
        file->buffer = buffer;
    }
    if (!file->fileptr)
        file->fileptr = fopen(file_name, file_flag);
    // return what it is or what it inits as
    return file;
}

// used to close a file object and deinit
int file_close(File *file) {
    if (file->fileptr)
        fclose(file->fileptr);
    if (file->buffer)
        free(file->buffer);
    // re-use init so we can reset the values.
    finit(NULL, NULL, NULL);
    return SUCCESS;
}

int main(void) {

    File *file = file_open();

    file = file_close();

    return 0;
}
