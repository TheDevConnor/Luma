#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define MAX_LINES 1000
#define MAX_LINE_LENGTH 1024
#define VERSION "0.1.0"

typedef enum { MODE_NORMAL, MODE_INSERT, MODE_COMMAND } Mode;

typedef struct {
  char *lines[MAX_LINES];
  int num_lines;
  int cursor_x;
  int cursor_y;
  Mode mode;
  char filename[256];
  int modified;
  char message[256];
} Editor;

struct termios orig_termios;

void die(const char *s) {
  perror(s);
  exit(1);
}

void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

void enable_raw_mode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void editor_init(Editor *ed) {
  ed->num_lines = 1;
  ed->lines[0] = malloc(1);
  ed->lines[0][0] = '\0';
  ed->cursor_x = 0;
  ed->cursor_y = 0;
  ed->mode = MODE_NORMAL;
  ed->filename[0] = '\0';
  ed->modified = 0;
  ed->message[0] = '\0';
}

void editor_free(Editor *ed) {
  for (int i = 0; i < ed->num_lines; i++) {
    free(ed->lines[i]);
  }
}

void editor_open_file(Editor *ed, const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    snprintf(ed->message, sizeof(ed->message), "New file: %s", filename);
    strncpy(ed->filename, filename, sizeof(ed->filename) - 1);
    return;
  }

  char line[MAX_LINE_LENGTH];
  ed->num_lines = 0;

  while (fgets(line, sizeof(line), fp)) {
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
      len--;
    }

    ed->lines[ed->num_lines] = malloc(len + 1);
    strcpy(ed->lines[ed->num_lines], line);
    ed->num_lines++;

    if (ed->num_lines >= MAX_LINES)
      break;
  }

  if (ed->num_lines == 0) {
    ed->num_lines = 1;
    ed->lines[0] = malloc(1);
    ed->lines[0][0] = '\0';
  }

  fclose(fp);
  strncpy(ed->filename, filename, sizeof(ed->filename) - 1);
  ed->modified = 0;
  snprintf(ed->message, sizeof(ed->message), "Loaded: %s", filename);
}

void editor_save_file(Editor *ed) {
  if (ed->filename[0] == '\0') {
    snprintf(ed->message, sizeof(ed->message), "No filename");
    return;
  }

  FILE *fp = fopen(ed->filename, "w");
  if (!fp) {
    snprintf(ed->message, sizeof(ed->message), "Error saving file");
    return;
  }

  for (int i = 0; i < ed->num_lines; i++) {
    fprintf(fp, "%s\n", ed->lines[i]);
  }

  fclose(fp);
  ed->modified = 0;
  snprintf(ed->message, sizeof(ed->message), "Saved: %s", ed->filename);
}

void editor_insert_char(Editor *ed, char c) {
  int len = strlen(ed->lines[ed->cursor_y]);
  char *new_line = malloc(len + 2);

  strncpy(new_line, ed->lines[ed->cursor_y], ed->cursor_x);
  new_line[ed->cursor_x] = c;
  strcpy(new_line + ed->cursor_x + 1, ed->lines[ed->cursor_y] + ed->cursor_x);

  free(ed->lines[ed->cursor_y]);
  ed->lines[ed->cursor_y] = new_line;
  ed->cursor_x++;
  ed->modified = 1;
}

void editor_delete_char(Editor *ed) {
  if (ed->cursor_x == 0 && ed->cursor_y == 0)
    return;

  if (ed->cursor_x > 0) {
    int len = strlen(ed->lines[ed->cursor_y]);
    char *new_line = malloc(len);

    strncpy(new_line, ed->lines[ed->cursor_y], ed->cursor_x - 1);
    strcpy(new_line + ed->cursor_x - 1, ed->lines[ed->cursor_y] + ed->cursor_x);

    free(ed->lines[ed->cursor_y]);
    ed->lines[ed->cursor_y] = new_line;
    ed->cursor_x--;
    ed->modified = 1;
  } else {
    int prev_len = strlen(ed->lines[ed->cursor_y - 1]);
    int curr_len = strlen(ed->lines[ed->cursor_y]);
    char *new_line = malloc(prev_len + curr_len + 1);

    strcpy(new_line, ed->lines[ed->cursor_y - 1]);
    strcat(new_line, ed->lines[ed->cursor_y]);

    free(ed->lines[ed->cursor_y - 1]);
    free(ed->lines[ed->cursor_y]);
    ed->lines[ed->cursor_y - 1] = new_line;

    for (int i = ed->cursor_y; i < ed->num_lines - 1; i++) {
      ed->lines[i] = ed->lines[i + 1];
    }

    ed->num_lines--;
    ed->cursor_y--;
    ed->cursor_x = prev_len;
    ed->modified = 1;
  }
}

void editor_insert_newline(Editor *ed) {
  if (ed->num_lines >= MAX_LINES)
    return;

  for (int i = ed->num_lines; i > ed->cursor_y; i--) {
    ed->lines[i] = ed->lines[i - 1];
  }

  int len = strlen(ed->lines[ed->cursor_y]);
  char *new_line = malloc(len - ed->cursor_x + 1);
  strcpy(new_line, ed->lines[ed->cursor_y] + ed->cursor_x);

  ed->lines[ed->cursor_y + 1] = new_line;
  ed->lines[ed->cursor_y][ed->cursor_x] = '\0';

  ed->num_lines++;
  ed->cursor_y++;
  ed->cursor_x = 0;
  ed->modified = 1;
}

void editor_refresh_screen(Editor *ed) {
  struct winsize ws;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  int screen_rows = ws.ws_row;

  write(STDOUT_FILENO, "\x1b[?25l", 6);
  write(STDOUT_FILENO, "\x1b[H", 3);

  char buf[64];
  for (int i = 0; i < ed->num_lines && i < screen_rows - 2; i++) {
    write(STDOUT_FILENO, "\x1b[K", 3);
    write(STDOUT_FILENO, ed->lines[i], strlen(ed->lines[i]));
    write(STDOUT_FILENO, "\r\n", 2);
  }

  for (int i = ed->num_lines; i < screen_rows - 2; i++) {
    write(STDOUT_FILENO, "\x1b[K~\r\n", 6);
  }

  write(STDOUT_FILENO, "\x1b[K", 3);
  write(STDOUT_FILENO, "\x1b[7m", 4);
  const char *mode_str = ed->mode == MODE_INSERT    ? "-- INSERT --"
                         : ed->mode == MODE_COMMAND ? ":"
                                                    : "";
  char status[256];
  snprintf(status, sizeof(status), " %s %s%s",
           ed->filename[0] ? ed->filename : "[No Name]",
           ed->modified ? "[+] " : "", mode_str);
  int status_len = strlen(status);
  write(STDOUT_FILENO, status, status_len);
  for (int i = status_len; i < ws.ws_col; i++) {
    write(STDOUT_FILENO, " ", 1);
  }
  write(STDOUT_FILENO, "\x1b[m", 3);
  write(STDOUT_FILENO, "\r\n", 2);

  write(STDOUT_FILENO, "\x1b[K", 3);
  if (ed->message[0]) {
    write(STDOUT_FILENO, ed->message, strlen(ed->message));
  }

  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", ed->cursor_y + 1, ed->cursor_x + 1);
  write(STDOUT_FILENO, buf, strlen(buf));
  write(STDOUT_FILENO, "\x1b[?25h", 6);
}

void editor_process_keypress(Editor *ed) {
  char c;
  if (read(STDIN_FILENO, &c, 1) != 1)
    return;

  if (ed->mode == MODE_INSERT) {
    if (c == 27) {
      ed->mode = MODE_NORMAL;
      if (ed->cursor_x > 0)
        ed->cursor_x--;
    } else if (c == 127) {
      editor_delete_char(ed);
    } else if (c == '\r') {
      editor_insert_newline(ed);
    } else if (!iscntrl(c)) {
      editor_insert_char(ed, c);
    }
  } else if (ed->mode == MODE_NORMAL) {
    ed->message[0] = '\0';
    switch (c) {
    case 'i':
      ed->mode = MODE_INSERT;
      break;
    case 'a':
      ed->mode = MODE_INSERT;
      if (ed->cursor_x < (int)strlen(ed->lines[ed->cursor_y])) {
        ed->cursor_x++;
      }
      break;
    case 'h':
      if (ed->cursor_x > 0)
        ed->cursor_x--;
      break;
    case 'l':
      if (ed->cursor_x < (int)strlen(ed->lines[ed->cursor_y])) {
        ed->cursor_x++;
      }
      break;
    case 'j':
      if (ed->cursor_y < ed->num_lines - 1) {
        ed->cursor_y++;
        int len = strlen(ed->lines[ed->cursor_y]);
        if (ed->cursor_x > len)
          ed->cursor_x = len;
      }
      break;
    case 'k':
      if (ed->cursor_y > 0) {
        ed->cursor_y--;
        int len = strlen(ed->lines[ed->cursor_y]);
        if (ed->cursor_x > len)
          ed->cursor_x = len;
      }
      break;
    case ':':
      ed->mode = MODE_COMMAND;
      break;
    }
  } else if (ed->mode == MODE_COMMAND) {
    if (c == 'w') {
      editor_save_file(ed);
      ed->mode = MODE_NORMAL;
    } else if (c == 'q') {
      if (!ed->modified) {
        editor_free(ed);
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
      } else {
        snprintf(ed->message, sizeof(ed->message),
                 "Unsaved changes! Use :q! to force quit");
        ed->mode = MODE_NORMAL;
      }
    } else if (c == '!') {
      editor_free(ed);
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
    } else {
      ed->mode = MODE_NORMAL;
    }
  }
}

int main(int argc, char *argv[]) {
  enable_raw_mode();

  Editor ed;
  editor_init(&ed);

  if (argc >= 2) {
    editor_open_file(&ed, argv[1]);
  }

  while (1) {
    editor_refresh_screen(&ed);
    editor_process_keypress(&ed);
  }

  return 0;
}
