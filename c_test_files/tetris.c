#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define FIELD_W 14
#define FIELD_H 24
#define VISIBLE_H 23

static struct termios orig_termios;

void disable_raw_mode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  printf("\x1b[?25h"); // show cursor
  printf("\x1b[0m");   // reset colors
  fflush(stdout);
}

void enable_raw_mode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON | ISIG);
  raw.c_iflag &= ~(IXON | ICRNL);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  printf("\x1b[?25l"); // hide cursor
  fflush(stdout);
}

int kbhit() {
  fd_set set;
  struct timeval tv = {0, 0};
  FD_ZERO(&set);
  FD_SET(STDIN_FILENO, &set);
  return select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) > 0;
}

int readch() {
  char c = 0;
  if (read(STDIN_FILENO, &c, 1) <= 0)
    return 0;
  return (int)c;
}

// Tetromino definitions (7 pieces, each as 4x4 block string)
const char *tetromino[7] = {
    "...."
    "XXXX"
    "...."
    "....", // I

    ".X.."
    ".X.."
    ".XX."
    "....", // J

    "..X."
    "..X."
    ".XX."
    "....", // L

    ".XX."
    ".XX."
    "...."
    "....", // O

    ".XX."
    "XX.."
    "...."
    "....", // S

    ".X.."
    ".XXX"
    "...."
    "....", // T

    "XX.."
    ".XX."
    "...."
    "...." // Z
};

// ANSI color codes for each piece
const char *piece_colors[7] = {
    "\x1b[96m",       // Cyan - I piece
    "\x1b[94m",       // Blue - J piece
    "\x1b[38;5;208m", // Orange - L piece
    "\x1b[93m",       // Yellow - O piece
    "\x1b[92m",       // Green - S piece
    "\x1b[95m",       // Magenta - T piece
    "\x1b[91m"        // Red - Z piece
};

int rotate(int px, int py, int r) {
  switch (r & 3) {
  case 0:
    return py * 4 + px;
  case 1:
    return 12 + py - (px * 4);
  case 2:
    return 15 - (py * 4) - px;
  case 3:
    return 3 - py + (px * 4);
  }
  return 0;
}

int field[FIELD_W * FIELD_H];

void init_field() {
  for (int x = 0; x < FIELD_W; x++) {
    for (int y = 0; y < FIELD_H; y++) {
      if (x == 0 || x == FIELD_W - 1 || y == FIELD_H - 1) {
        field[y * FIELD_W + x] = 9; // wall
      } else {
        field[y * FIELD_W + x] = 0; // empty
      }
    }
  }
}

int doesPieceFit(int tetrominoIndex, int rotation, int posX, int posY) {
  for (int px = 0; px < 4; px++)
    for (int py = 0; py < 4; py++) {
      int pi = rotate(px, py, rotation);
      char pieceCell = tetromino[tetrominoIndex][pi];
      if (pieceCell != 'X')
        continue;
      int fx = posX + px;
      int fy = posY + py;
      if (fx >= 0 && fx < FIELD_W && fy >= 0 && fy < FIELD_H) {
        if (field[fy * FIELD_W + fx] != 0)
          return 0;
      } else {
        return 0;
      }
    }
  return 1;
}

void draw_screen(int currentPiece, int currentRotation, int currentX,
                 int currentY, int score, int lines, int nextPiece) {
  printf("\x1b[3;1H"); // move cursor to row 3, column 1 (leaving top 2 rows as
                       // padding)

  // Draw header with box drawing characters
  printf("\x1b[0m");
  printf("╔════════════════════════════╗  ╔═══════════════╗\n");
  printf("║      \x1b[1;97mT E T R I S\x1b[0m         ║  ║  \x1b[1mNEXT "
         "PIECE\x1b[0m  ║\n");
  printf("╠════════════════════════════╣  ║               ║\n");

  // Draw visible area with next piece preview
  for (int y = 0; y < VISIBLE_H; y++) {
    printf("║");
    for (int x = 0; x < FIELD_W; x++) {
      int cell = field[(y)*FIELD_W + x];

      // Check if current piece occupies this cell
      int occupied = 0;
      for (int px = 0; px < 4; px++) {
        for (int py = 0; py < 4; py++) {
          int pi = rotate(px, py, currentRotation);
          if (tetromino[currentPiece][pi] != 'X')
            continue;
          int fx = currentX + px;
          int fy = currentY + py;
          if (fx == x && fy == y)
            occupied = 1;
        }
      }

      if (occupied) {
        printf("%s██\x1b[0m", piece_colors[currentPiece]);
      } else if (cell == 9) {
        printf("\x1b[90m▓▓\x1b[0m"); // wall
      } else if (cell > 0 && cell < 9) {
        printf("%s██\x1b[0m", piece_colors[cell - 1]);
      } else {
        printf("  ");
      }
    }
    printf("\x1b[0m║");

    // Draw next piece preview and stats on the right
    if (y == 1 || y == 2 || y == 3 || y == 4) {
      int py = y - 1;
      printf("  ║ ");
      for (int px = 0; px < 4; px++) {
        int pi = rotate(px, py, 0);
        if (tetromino[nextPiece][pi] == 'X') {
          printf("%s██\x1b[0m", piece_colors[nextPiece]);
        } else {
          printf("  ");
        }
      }
      printf("\x1b[0m ║");
    } else if (y == 5) {
      printf("  ╠═══════════════╣");
    } else if (y == 6) {
      printf("  ║ \x1b[1mScore:\x1b[0m        ║");
    } else if (y == 7) {
      printf("  ║ \x1b[93m%-13d\x1b[0m ║", score);
    } else if (y == 8) {
      printf("  ║               ║");
    } else if (y == 9) {
      printf("  ║ \x1b[1mLines:\x1b[0m        ║");
    } else if (y == 10) {
      printf("  ║ \x1b[92m%-13d\x1b[0m ║", lines);
    } else if (y == 11) {
      printf("  ╠═══════════════╣");
    } else if (y == 12) {
      printf("  ║ \x1b[1mControls:\x1b[0m     ║");
    } else if (y == 13) {
      printf("  ║ \x1b[96mW/↑\x1b[0m - Rotate  ║");
    } else if (y == 14) {
      printf("  ║ \x1b[96mA/←\x1b[0m - Left    ║");
    } else if (y == 15) {
      printf("  ║ \x1b[96mD/→\x1b[0m - Right   ║");
    } else if (y == 16) {
      printf("  ║ \x1b[96mS/↓\x1b[0m - Down    ║");
    } else if (y == 17) {
      printf("  ║ \x1b[96mSPACE\x1b[0m - Drop  ║");
    } else if (y == 18) {
      printf("  ║ \x1b[96mQ\x1b[0m - Quit      ║");
    } else if (y == VISIBLE_H - 1) {
      printf("  ╚═══════════════╝");
    } else {
      printf("  ║               ║");
    }

    printf("\n");
  }
  printf("╚════════════════════════════╝\n");

  fflush(stdout);
}

int main() {
  srand((unsigned int)time(NULL));
  enable_raw_mode();
  printf("\x1b[2J"); // clear screen
  printf("\n\n");    // Add some top padding so first line is visible
  init_field();

  int currentPiece = rand() % 7;
  int nextPiece = rand() % 7;
  int currentRotation = 0;
  int currentX = FIELD_W / 2 - 2;
  int currentY = 2;   // Start at row 2 so piece is fully visible
  int firstPiece = 1; // Flag to track first piece

  int gameOver = 0;
  int score = 0;
  int linesClearedTotal = 0;
  int speed = 12; // Starting speed - lower = faster (was 20)
  int speedCount = 0;
  int forceDown = 0;
  int pieceCount = 0;
  int key = 0;

  struct timespec lastTime;
  clock_gettime(CLOCK_MONOTONIC, &lastTime);

  while (!gameOver) {
    // TIMING
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int elapsed_ms = (now.tv_sec - lastTime.tv_sec) * 1000 +
                     (now.tv_nsec - lastTime.tv_nsec) / 1000000;
    if (elapsed_ms >= 50) {
      lastTime = now;
      speedCount++;
      if (speedCount >= speed) {
        forceDown = 1;
        speedCount = 0;
      }
    }

    // INPUT
    key = 0;
    if (kbhit()) {
      int c = readch();
      if (c == 'q' || c == 'Q') {
        gameOver = 1;
        break;
      }
      if (c == 'a' || c == 'A' || c == 68)
        key = 'L';
      if (c == 'd' || c == 'D' || c == 67)
        key = 'R';
      if (c == 's' || c == 'S' || c == 66)
        key = 'D';
      if (c == 'w' || c == 'W' || c == 65)
        key = 'U';
      if (c == ' ')
        key = ' ';
    }

    // GAME LOGIC
    if (key == 'L') {
      if (doesPieceFit(currentPiece, currentRotation, currentX - 1, currentY))
        currentX--;
    } else if (key == 'R') {
      if (doesPieceFit(currentPiece, currentRotation, currentX + 1, currentY))
        currentX++;
    } else if (key == 'D') {
      if (doesPieceFit(currentPiece, currentRotation, currentX, currentY + 1))
        currentY++;
    } else if (key == 'U') {
      if (doesPieceFit(currentPiece, currentRotation + 1, currentX, currentY))
        currentRotation = (currentRotation + 1) & 3;
    } else if (key == ' ') {
      while (
          doesPieceFit(currentPiece, currentRotation, currentX, currentY + 1)) {
        currentY++;
        score += 2;
      }
      forceDown = 1;
    }

    if (forceDown) {
      // Don't force down on the very first frame
      if (firstPiece && speedCount == 0) {
        firstPiece = 0;
        forceDown = 0;
      } else if (doesPieceFit(currentPiece, currentRotation, currentX,
                              currentY + 1)) {
        currentY++;
      } else {
        // Lock piece
        for (int px = 0; px < 4; px++)
          for (int py = 0; py < 4; py++) {
            int pi = rotate(px, py, currentRotation);
            if (tetromino[currentPiece][pi] == 'X') {
              int fx = currentX + px;
              int fy = currentY + py;
              field[fy * FIELD_W + fx] = currentPiece + 1;
            }
          }

        pieceCount++;
        if (pieceCount % 8 == 0 &&
            speed > 4) // Speed up every 8 pieces (was 10), min speed 4 (was 8)
          speed--;

        // Check for lines
        int lines = 0;
        for (int py = 0; py < 4; py++) {
          int row = currentY + py;
          if (row >= FIELD_H - 1)
            continue;
          int full = 1;
          for (int x = 1; x < FIELD_W - 1; x++) {
            if (field[row * FIELD_W + x] == 0) {
              full = 0;
              break;
            }
          }
          if (full) {
            for (int x = 1; x < FIELD_W - 1; x++)
              field[row * FIELD_W + x] = 8;
            lines++;
          }
        }

        if (lines > 0) {
          switch (lines) {
          case 1:
            score += 100;
            break;
          case 2:
            score += 300;
            break;
          case 3:
            score += 500;
            break;
          case 4:
            score += 800;
            break;
          }
          linesClearedTotal += lines;

          // Collapse lines
          for (int y = currentY + 3; y >= 0; y--) {
            for (int x = 1; x < FIELD_W - 1; x++) {
              if (field[y * FIELD_W + x] == 8) {
                for (int ty = y; ty > 0; ty--) {
                  field[ty * FIELD_W + x] = field[(ty - 1) * FIELD_W + x];
                }
                field[0 * FIELD_W + x] = 0;
              }
            }
          }
        }

        // Spawn next piece
        currentX = FIELD_W / 2 - 2;
        currentY = 2; // Start at row 2 so piece is fully visible
        currentRotation = 0;
        currentPiece = nextPiece;
        nextPiece = rand() % 7;
        firstPiece = 0; // Reset flag after first piece

        if (!doesPieceFit(currentPiece, currentRotation, currentX, currentY)) {
          printf("\x1b[2J\x1b[H");
          printf("\n\n\n");
          printf("╔════════════════════════════╗\n");
          printf("║       \x1b[1;91mGAME OVER!\x1b[0m          ║\n");
          printf("╠════════════════════════════╣\n");
          printf("║                            ║\n");
          printf("║  Final Score: \x1b[1;93m%-11d\x1b[0m ║\n", score);
          printf("║  Lines:       \x1b[1;92m%-11d\x1b[0m ║\n",
                 linesClearedTotal);
          printf("║                            ║\n");
          printf("╠════════════════════════════╣\n");
          printf("║  Press Q to quit           ║\n");
          printf("╚════════════════════════════╝\n");
          fflush(stdout);

          while (1) {
            if (kbhit()) {
              int c = readch();
              if (c == 'q' || c == 'Q') {
                gameOver = 1;
                break;
              }
            }
            usleep(10000);
          }
        }
      }
      forceDown = 0;
    }

    // RENDER
    draw_screen(currentPiece, currentRotation, currentX, currentY, score,
                linesClearedTotal, nextPiece);

    usleep(20000);
  }

  disable_raw_mode();
  printf("\n\x1b[1;96mThanks for playing!\x1b[0m\n\n");
  return 0;
}
