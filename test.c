#include <math.h>
#include <stdio.h>
#include <time.h>

#define PI 3.14159265358979323
#define TWO_PI (2 * PI)
#define DT 0.1
#define VALUES_COUNT 64
#define BENCHMARK_ITERATIONS 1000000

double values[VALUES_COUNT] = {0.0,
                               0.09983341664682815,
                               0.19866933079506122,
                               0.2955202066613396,
                               0.3894183423086505,
                               0.479425538604203,
                               0.5646424733950355,
                               0.6442176872376911,
                               0.7173560908995228,
                               0.7833269096274834,
                               0.8414709848078965,
                               0.8912073600614354,
                               0.9320390859672264,
                               0.963558185417193,
                               0.9854497299884603,
                               0.9974949866040544,
                               0.9995736030415051,
                               0.9916648104524686,
                               0.9738476308781951,
                               0.9463000876874145,
                               0.9092974268256817,
                               0.8632093666488737,
                               0.8084964038195901,
                               0.74570521217672,
                               0.6754631805511506,
                               0.5984721441039564,
                               0.5155013718214642,
                               0.4273798802338298,
                               0.33498815015590466,
                               0.23924932921398198,
                               0.1411200080598672,
                               0.04158066243329049,
                               -0.058374143427580086,
                               -0.15774569414324865,
                               -0.25554110202683167,
                               -0.35078322768961984,
                               -0.44252044329485246,
                               -0.5298361409084934,
                               -0.6118578909427193,
                               -0.6877661591839741,
                               -0.7568024953079282,
                               -0.8182771110644108,
                               -0.8715757724135882,
                               -0.9161659367494549,
                               -0.9516020738895161,
                               -0.977530117665097,
                               -0.9936910036334645,
                               -0.9999232575641008,
                               -0.9961646088358406,
                               -0.9824526126243325,
                               -0.9589242746631385,
                               -0.9258146823277321,
                               -0.8834546557201531,
                               -0.8322674422239008,
                               -0.7727644875559871,
                               -0.7055403255703919,
                               -0.6312666378723208,
                               -0.5506855425976376,
                               -0.4646021794137566,
                               -0.373876664830236,
                               -0.27941549819892586,
                               -0.18216250427209502,
                               -0.0830894028174964,
                               0.0168139004843506};

// Optimized version - avoids expensive fmod and division
double sin_approx(double x) {
  // Fast range reduction using bit manipulation for positive values
  // This assumes x is reasonably sized (not extremely large)
  static const double INV_TWO_PI = 1.0 / TWO_PI;
  static const double R = 1.0 / DT; // Precompute reciprocal

  // Fast range reduction - convert to [0, 2π)
  x = x - TWO_PI * floor(x * INV_TWO_PI);

  // Convert to table index with fractional part
  double scaled = x * R;
  int index = (int)scaled;
  double t = scaled - index;

  // Ensure we don't go out of bounds (should rarely happen with proper range
  // reduction)
  if (index >= VALUES_COUNT - 1) {
    index = VALUES_COUNT - 2;
    t = 1.0;
  }

  // Linear interpolation
  return values[index] + t * (values[index + 1] - values[index]);
}

// Even faster version - minimal error checking
double sin_approx_fast(double x) {
  static const double INV_TWO_PI = 1.0 / TWO_PI;
  static const double R = 1.0 / DT;

  // Assume x is already in reasonable range, just wrap around
  x = x - TWO_PI * (int)(x * INV_TWO_PI);
  if (x < 0)
    x += TWO_PI;

  double scaled = x * R;
  int index = (int)scaled;
  double t = scaled - index;

  // Fast boundary check using bitwise AND (works because VALUES_COUNT is power
  // of 2 - 64) Actually VALUES_COUNT is 64, not a power of 2, so use modulo
  // int next_index = (index + 1) % VALUES_COUNT;

  return values[index] + t * (values[index + 1] - values[index]);
}

// Ultra-fast version with no range checking (dangerous but fastest)
double sin_approx_unsafe(double x) {
  static const double R = 1.0 / DT;

  // Assume x is already in [0, 2π) range
  double scaled = x * R;
  int index = (int)scaled;
  double t = scaled - index;

  return values[index] + t * (values[index + 1] - values[index]);
}

int main() {
  clock_t start, end;
  double elapsed;
  volatile double dummy = 0.0; // Prevent compiler optimization

  // Generate test values
  double test_values[BENCHMARK_ITERATIONS];
  for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
    test_values[i] = (double)i * TWO_PI / BENCHMARK_ITERATIONS;
  }

  // Benchmark sin_approx
  start = clock();
  for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
    dummy += sin_approx(test_values[i]);
  }
  end = clock();
  elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Time (approx): %f seconds (%f)\n", elapsed, dummy);

  // Benchmark sin_approx_fast
  dummy = 0.0;
  start = clock();
  for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
    dummy += sin_approx_fast(test_values[i]);
  }
  end = clock();
  elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Time (approx_fast): %f seconds (%f)\n", elapsed, dummy);

  // Benchmark sin_approx_unsafe (only safe because test_values are in [0,2π])
  dummy = 0.0;
  start = clock();
  for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
    dummy += sin_approx_unsafe(test_values[i]);
  }
  end = clock();
  elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Time (approx_unsafe): %f seconds (%f)\n", elapsed, dummy);

  // Reset dummy to ensure fair comparison
  dummy = 0.0;

  // Benchmark standard sin
  start = clock();
  for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
    dummy += sin(test_values[i]);
  }
  end = clock();
  elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Time (std): %f seconds (%f)\n", elapsed, dummy);

  // Compute max error using the same test values
  double max_error = 0.0;
  for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
    double error = fabs(sin_approx(test_values[i]) - sin(test_values[i]));
    if (error > max_error) {
      max_error = error;
    }
  }
  printf("Max error: %f\n", max_error);

  // Use dummy to prevent optimization (won't actually print due to very small
  // probability)
  if (dummy == -999999.999999) {
    printf("This prevents optimization: %f\n", dummy);
  }

  return 0;
}
