/* Copyright 2018 Stanford University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <immintrin.h>
#include <cassert>
#include <sys/time.h>
#include "core.h"
#include "core_kernel.h"

void execute_kernel_empty(const Kernel &kernel)
{
  // Do nothing...
}

long long execute_kernel_busy_wait(const Kernel &kernel)
{
  long long acc = 113;
  for (long iter = 0; iter < kernel.iterations; iter++) {
    acc = acc * 139 % 2147483647;
  }
  return acc;
}

void execute_kernel_memory(const Kernel &kernel,
                           char *scratch_ptr, size_t scratch_bytes)
{
  long long jump = kernel.jump;
  long long N = scratch_bytes / (3 * sizeof(double));

  double *A = reinterpret_cast<double *>(scratch_ptr);
  double *B = reinterpret_cast<double *>(scratch_ptr + N * sizeof(double));
  double *C = reinterpret_cast<double *>(scratch_ptr + 2 * N * sizeof(double));

  for (long iter = 0; iter < kernel.iterations; iter++) {
    for (long i = 0; i < jump; i++) {
      for (long j = 0; j < (N/jump); j++) {
        long t = (i + j * jump) % (N);
        C[t] = A[t] + B[t];
      }
    }
  }
  // printf("execute_kernel_memory! C[N-1]=%f, N=%lld, jump=%lld\n", C[N-1], N, jump);
}

double execute_kernel_compute(const Kernel &kernel)
{
#if 0 
  double A[32];
  
  for (int i = 0; i < 32; i++) {
    A[i] = 1.2345;
  }
  
  for (long iter = 0; iter < kernel.iterations; iter++) {
    for (int i = 0; i < 32; i++) {
        A[i] *= A[i];
    }
  }
  
  double dot = 1.0;
  for (int i = 0; i < 32; i++) {
    dot *= A[i];
  }
  return dot; 
#else
  __m256d A[8];
  
  for (int i = 0; i < 8; i++) {
    A[i] = _mm256_set_pd(1.0f, 2.0f, 3.0f, 4.0f);
  }
  
  for (long iter = 0; iter < kernel.iterations; iter++) {
    for (int i = 0; i < 8; i++) {
      A[i] = _mm256_mul_pd(A[i], A[i]);
       //A[i] = _mm256_fmadd_pd(A[i], A[i], A[i]);
       //  A[i] = _mm256_add_pd(A[i], A[i]);
    }
  }
  
  double *C = (double *)A;
  double dot = 1.0;
  for (int i = 0; i < 32; i++) {
    dot *= C[i];
  }
  return dot; 
#endif  
}

double execute_kernel_compute2(const Kernel &kernel)
{
  constexpr size_t N = 32;
  double A[N] = {0};
  double B[N] = {0};
  double C[N] = {0};

  for (size_t i = 0; i < N; ++i) {
    A[i] = 1.2345;
    B[i] = 1.010101;
  }

  for (long iter = 0; iter < kernel.iterations; iter++) {
    for (size_t i = 0; i < N; ++i) {
      C[i] = C[i] + (A[i] * B[i]);
    }
  }

  double sum = 0;
  for (size_t i = 0; i < N; ++i) {
    sum += C[i];
  }
  return sum;
}

void execute_kernel_io(const Kernel &kernel)
{
  assert(false);
}

double execute_kernel_imbalance(const Kernel &kernel)
{
  // Use current time as seed for random generator
  //struct timeval tv;
  //gettimeofday(&tv,NULL);
  //long t = tv.tv_sec *1e6 + tv.tv_usec;
  //srand(t);

  long iterations = rand() % kernel.iterations;
  Kernel k(kernel);
  k.iterations = iterations;
  //printf("iteration %d\n", iterations);
  return execute_kernel_compute(k);
}