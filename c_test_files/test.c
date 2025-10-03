#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Custom random number generator using LCG algorithm
unsigned int custom_rand(unsigned int *seed) {
  // LCG parameters (same as glibc)
  *seed = (*seed * 1103515245 + 12345) & 2147483647;
  return *seed;
}

#define MIN_SPEED 1
#define MAX_SPEED 3
#define FIXED_ROWS 30
#define FIXED_COLS 100

typedef struct {
  int y;
  int length;
  int speed;
  char *chars;
} Drop;

void get_terminal_size(int *rows, int *cols) {
    *rows = FIXED_ROWS;
    *cols = FIXED_COLS;
}


char random_char(unsigned int *seed) {
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0"
                         "123456789!@#$%^&*()";
  return charset[custom_rand(seed) % (sizeof(charset) - 1)];
}

void init_drop(Drop *drop, int max_rows, unsigned int *seed) {
  drop->y = -(custom_rand(seed) % max_rows);
  drop->length = 10 + custom_rand(seed) % 15;
  drop->speed = MIN_SPEED + custom_rand(seed) % (MAX_SPEED - MIN_SPEED + 1);
  drop->chars = malloc(drop->length * sizeof(char));
  for (int i = 0; i < drop->length; i++) {
    drop->chars[i] = random_char(seed);
  }
}

void update_drop(Drop *drop, int max_rows, unsigned int *seed) {
  drop->y += drop->speed;
  if (drop->y - drop->length > max_rows) {
    free(drop->chars);
    init_drop(drop, max_rows, seed);
  }
  // Randomly change characters
  if (custom_rand(seed) % 5 == 0) {
    int idx = custom_rand(seed) % drop->length;
    drop->chars[idx] = random_char(seed);
  }
}

void render(Drop *drops, int num_drops, int rows, int cols) {
  char **screen = malloc(rows * sizeof(char *));
  for (int i = 0; i < rows; i++) {
    screen[i] = malloc((cols + 1) * sizeof(char));
    memset(screen[i], ' ', cols);
    screen[i][cols] = '\0';
  }

  for (int i = 0; i < num_drops; i++) {
    Drop *d = &drops[i];
    for (int j = 0; j < d->length; j++) {
      int y = d->y - j;
      if (y >= 0 && y < rows && i < cols) {
        screen[y][i] = d->chars[j];
      }
    }
  }

  printf("\033[H");   // Move cursor to home
  printf("\033[32m"); // Green color

  for (int i = 0; i < rows; i++) {
    // Fade effect - make older characters dimmer
    printf("%s\n", screen[i]);
    free(screen[i]);
  }
  free(screen);

  printf("\033[0m"); // Reset color
  fflush(stdout);
}

int main() {
  unsigned int seed = 123456789;

  int rows, cols;
  get_terminal_size(&rows, &cols);

  Drop *drops = malloc(cols * sizeof(Drop));
  for (int i = 0; i < cols; i++) {
    init_drop(&drops[i], rows, &seed);
  }

  printf("\033[2J");   // Clear screen
  printf("\033[?25l"); // Hide cursor

  while (1) {
    render(drops, cols, rows, cols);

    for (int i = 0; i < cols; i++) {
      update_drop(&drops[i], rows, &seed);
    }

    usleep(50000); // 50ms delay
  }

  printf("\033[?25h"); // Show cursor

  for (int i = 0; i < cols; i++) {
    free(drops[i].chars);
  }
  free(drops);

  return 0;
}
