/*
 * timing.c - A simple timing library for C
 * No external dependencies, uses only standard library
 *
 * Compile: gcc -o timing timing.c
 * Run: ./timing
 */

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

/* Timer structure */
typedef struct {
  struct timespec start;
  struct timespec end;
  int running;
} Timer;

/* High-resolution timer using clock_gettime */
void timer_init(Timer *t) {
  t->running = 0;
  t->start.tv_sec = 0;
  t->start.tv_nsec = 0;
  t->end.tv_sec = 0;
  t->end.tv_nsec = 0;
}

void timer_start(Timer *t) {
  clock_gettime(CLOCK_MONOTONIC, &t->start);
  t->running = 1;
}

void timer_stop(Timer *t) {
  clock_gettime(CLOCK_MONOTONIC, &t->end);
  t->running = 0;
}

double timer_elapsed_ns(Timer *t) {
  struct timespec end;
  if (t->running) {
    clock_gettime(CLOCK_MONOTONIC, &end);
  } else {
    end = t->end;
  }

  return (double)(end.tv_sec - t->start.tv_sec) * 1e9 +
         (double)(end.tv_nsec - t->start.tv_nsec);
}

double timer_elapsed_us(Timer *t) { return timer_elapsed_ns(t) / 1000.0; }

double timer_elapsed_ms(Timer *t) { return timer_elapsed_ns(t) / 1000000.0; }

double timer_elapsed_s(Timer *t) { return timer_elapsed_ns(t) / 1000000000.0; }

/* Sleep functions */
void sleep_ns(uint64_t ns) {
  struct timespec ts;
  ts.tv_sec = ns / 1000000000;
  ts.tv_nsec = ns % 1000000000;
  nanosleep(&ts, NULL);
}

void sleep_us(uint64_t us) { sleep_ns(us * 1000); }

void sleep_ms(uint64_t ms) { sleep_ns(ms * 1000000); }

/* Get current timestamp in various formats */
uint64_t timestamp_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000 + (uint64_t)ts.tv_nsec;
}

uint64_t timestamp_us(void) { return timestamp_ns() / 1000; }

uint64_t timestamp_ms(void) { return timestamp_ns() / 1000000; }

/* Frame rate limiter */
typedef struct {
  uint64_t last_frame_time;
  double target_frame_time_ns;
  double fps;
} FrameLimiter;

void fps_limiter_init(FrameLimiter *fl, double target_fps) {
  fl->fps = target_fps;
  fl->target_frame_time_ns = 1000000000.0 / target_fps;
  fl->last_frame_time = timestamp_ns();
}

void fps_limiter_wait(FrameLimiter *fl) {
  uint64_t current_time = timestamp_ns();
  uint64_t elapsed = current_time - fl->last_frame_time;

  if (elapsed < (uint64_t)fl->target_frame_time_ns) {
    uint64_t sleep_time = (uint64_t)fl->target_frame_time_ns - elapsed;
    sleep_ns(sleep_time);
  }

  fl->last_frame_time = timestamp_ns();
}

double fps_limiter_get_actual_fps(FrameLimiter *fl) {
  uint64_t current_time = timestamp_ns();
  uint64_t elapsed = current_time - fl->last_frame_time;
  if (elapsed == 0)
    return 0.0;
  return 1000000000.0 / (double)elapsed;
}

/* Benchmark helper - runs function n times and returns average time */
typedef void (*benchmark_func)(void);

typedef struct {
  double min_ms;
  double max_ms;
  double avg_ms;
  double total_ms;
  int iterations;
} BenchmarkResult;

BenchmarkResult benchmark_run(benchmark_func func, int iterations) {
  BenchmarkResult result;
  result.min_ms = 1e9;
  result.max_ms = 0.0;
  result.total_ms = 0.0;
  result.iterations = iterations;

  Timer t;
  timer_init(&t);

  for (int i = 0; i < iterations; i++) {
    timer_start(&t);
    func();
    timer_stop(&t);

    double elapsed = timer_elapsed_ms(&t);
    result.total_ms += elapsed;

    if (elapsed < result.min_ms)
      result.min_ms = elapsed;
    if (elapsed > result.max_ms)
      result.max_ms = elapsed;
  }

  result.avg_ms = result.total_ms / iterations;
  return result;
}

/* Stopwatch for measuring multiple intervals */
#define MAX_LAPS 100

typedef struct {
  Timer timer;
  double laps[MAX_LAPS];
  int lap_count;
} Stopwatch;

void stopwatch_init(Stopwatch *sw) {
  timer_init(&sw->timer);
  sw->lap_count = 0;
}

void stopwatch_start(Stopwatch *sw) {
  timer_start(&sw->timer);
  sw->lap_count = 0;
}

void stopwatch_lap(Stopwatch *sw) {
  if (sw->lap_count < MAX_LAPS) {
    sw->laps[sw->lap_count++] = timer_elapsed_ms(&sw->timer);
  }
}

void stopwatch_stop(Stopwatch *sw) { timer_stop(&sw->timer); }

double stopwatch_total_ms(Stopwatch *sw) {
  return timer_elapsed_ms(&sw->timer);
}

double stopwatch_lap_time(Stopwatch *sw, int lap) {
  if (lap < 0 || lap >= sw->lap_count)
    return 0.0;
  if (lap == 0)
    return sw->laps[0];
  return sw->laps[lap] - sw->laps[lap - 1];
}

/* ============================================ */
/*           TEST PROGRAM BELOW                */
/* ============================================ */

void dummy_work(void) {
  volatile int sum = 0;
  for (int i = 0; i < 1000000; i++) {
    sum += i;
  }
}

void test_basic_timer(void) {
  printf("=== Basic Timer Test ===\n");
  Timer t;
  timer_init(&t);

  timer_start(&t);
  sleep_ms(100);
  timer_stop(&t);

  printf("Sleep 100ms took: %.2f ms\n", timer_elapsed_ms(&t));
  printf("              or: %.2f us\n", timer_elapsed_us(&t));
  printf("              or: %.0f ns\n\n", timer_elapsed_ns(&t));
}

void test_stopwatch(void) {
  printf("=== Stopwatch Test ===\n");
  Stopwatch sw;
  stopwatch_init(&sw);
  stopwatch_start(&sw);

  for (int i = 0; i < 5; i++) {
    sleep_ms(50);
    stopwatch_lap(&sw);
    printf("Lap %d: %.2f ms (split: %.2f ms)\n", i + 1, sw.laps[i],
           stopwatch_lap_time(&sw, i));
  }

  stopwatch_stop(&sw);
  printf("Total time: %.2f ms\n\n", stopwatch_total_ms(&sw));
}

void test_benchmark(void) {
  printf("=== Benchmark Test ===\n");
  BenchmarkResult result = benchmark_run(dummy_work, 10);

  printf("Iterations: %d\n", result.iterations);
  printf("Average:    %.3f ms\n", result.avg_ms);
  printf("Min:        %.3f ms\n", result.min_ms);
  printf("Max:        %.3f ms\n", result.max_ms);
  printf("Total:      %.3f ms\n\n", result.total_ms);
}

void test_fps_limiter(void) {
  printf("=== FPS Limiter Test (60 FPS) ===\n");
  FrameLimiter fl;
  fps_limiter_init(&fl, 60.0);

  for (int i = 0; i < 10; i++) {
    uint64_t start = timestamp_ms();

    // Simulate some work
    sleep_us(5000); // 5ms of "work"

    fps_limiter_wait(&fl);

    uint64_t end = timestamp_ms();
    printf("Frame %d: %llu ms (target: %.1f ms)\n", i + 1,
           (unsigned long long)(end - start), 1000.0 / 60.0);
  }
  printf("\n");
}

void test_timestamps(void) {
  printf("=== Timestamp Test ===\n");
  uint64_t ns = timestamp_ns();
  uint64_t us = timestamp_us();
  uint64_t ms = timestamp_ms();

  printf("Timestamp (ns): %llu\n", (unsigned long long)ns);
  printf("Timestamp (us): %llu\n", (unsigned long long)us);
  printf("Timestamp (ms): %llu\n\n", (unsigned long long)ms);
}

int main(void) {
  printf("Timing Library Test Program\n");
  printf("============================\n\n");

  test_basic_timer();
  test_stopwatch();
  test_benchmark();
  test_fps_limiter();
  test_timestamps();

  printf("All tests complete!\n");
  return 0;
}
