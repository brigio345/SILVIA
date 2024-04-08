#include <iostream>
#include <random>

#define DEBUG
#define M 8
#define N 8
#define P 8
#define TESTS 10

// a: mxn; b: nxp; c: mxp
void matrix_multiplication_gold(char *a, char *b, char *c, int m, int n,
                                int p) {
  for (int i = 0; i < (m * p); ++i)
    c[i] = 0;

  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      for (int k = 0; k < p; ++k) {
        c[i * p + k] += a[i * n + j] * b[j * p + k];
      }
    }
  }
}

char compute_epsilon_nu(char *x) {
  char epsilon = 0;
  for (int j = 1; j <= (N / 2); ++j)
    epsilon += (x[2 * j - 1 - 1] * x[2 * j - 1]);

  return epsilon;
}

char inner_product_wino(char *x, char *y, char epsilon, char nu, int x_offset) {
  char prod = -(epsilon + nu);
  if (N % 2)
    prod += (x[x_offset + N - 1] * y[N - 1]);

  for (int j = 1; j <= (N / 2); ++j)
    prod += ((x[x_offset + 2 * j - 1 - 1] + y[2 * j - 1]) *
             (x[x_offset + 2 * j - 1] + y[2 * j - 1 - 1]));

  return prod;
}

char compute_inner_product_wino(char *x, char *y) {
  return inner_product_wino(x, y, compute_epsilon_nu(x), compute_epsilon_nu(y),
                            0);
}

void matrix_multiplication_wino(char a[M * N], char b[N * P], char c[M * P]) {
#pragma HLS interface mode = ap_memory port = a
#pragma HLS interface mode = ap_memory port = b
#pragma HLS interface mode = ap_memory port = c
  for (int i = 0; i < M; ++i) {
    char epsilon = compute_epsilon_nu(&(a[i * N]));
    for (int k = 0; k < P; ++k) {
#pragma HLS pipeline
      char b_tmp[N];
      for (int j = 0; j < N; ++j)
        b_tmp[j] = b[j * P + k];
      char nu = compute_epsilon_nu(b_tmp);
      c[i * P + k] = inner_product_wino(a, b_tmp, epsilon, nu, i * N);
    }
  }
}

char inner_product(char *x, char *y) {
  char prod = 0;

  for (int j = 0; j < N; ++j)
    prod += (x[j] * y[j]);

  return prod;
}

int main() {
  char a[M * N];
  char b[N * P];
  char c[M * P];
  char c_gold[M * P];
  int err;

  err = 0;
  for (int j = 0; j < TESTS; ++j) {
    for (int i = 0; i < (M * N); ++i)
      a[i] = (std::rand() % 128);
    for (int i = 0; i < (N * P); ++i)
      b[i] = (std::rand() % 128);

    matrix_multiplication_gold(a, b, c_gold, M, N, P);
    matrix_multiplication_wino(a, b, c);

    for (int i = 0; i < (M * P); ++i) {
      if (c[i] != c_gold[i]) {
        err++;
#ifdef DEBUG
        std::cout << "c[" << i % P << "][" << i / P << "]: wino=" << (int)c[i]
                  << "\tc_gold=" << (int)c_gold[i] << std::endl;
#endif /* DEBUG */
      }
    }
  }

  if (err > 0) {
    std::cout << "FAILED with " << err << " errors" << std::endl;
    return 1;
  }

  std::cout << "PASS" << std::endl;
  return 0;
}
