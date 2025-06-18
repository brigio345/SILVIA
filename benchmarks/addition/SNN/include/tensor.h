#ifndef SILVIA_TENSOR_H_
#define SILVIA_TENSOR_H_

template <typename T>
struct Tensor {
  Tensor(void) {}
};

template <typename T, unsigned N>
struct Tensor1d : Tensor<T> {
  Tensor1d(void) = default;

  // Copy constructor with single value.
  Tensor1d(T val) { fill(val); }

  // Copy constructor with plain C array.
  Tensor1d(const T t[N]) {
    for (unsigned i = 0; i < N; i++) {
      data[i] = t[i];
    }
  }

  // Copy constructor with tensor.
  Tensor1d(const Tensor1d& t) : Tensor1d(t.data) {}

  // Fill the array with a value.
  void fill(T val) {
    for (unsigned i = 0; i < N; i++) {
      data[i] = val;
    }
  }

  T data[N];
};

template <typename T, unsigned N0, unsigned N1>
struct Tensor2d : Tensor<T> {
  Tensor2d(void) = default;

  // Copy constructor with single value.
  Tensor2d(T val) { fill(val); }

  // Copy constructor with plain C array.
  Tensor2d(const T t[N0][N1]) {
    for (unsigned i = 0; i < N0; i++) {
      for (unsigned j = 0; j < N1; j++) {
        data[i][j] = t[i][j];
      }
    }
  }

  // Copy constructor with tensor.
  Tensor2d(const Tensor2d& t) : Tensor2d(t.data) {}

  // Fill the array with a value.
  void fill(T val) {
    for (unsigned i = 0; i < N0; i++) {
      for (unsigned j = 0; j < N1; j++) {
        data[i][j] = val;
      }
    }
  }

  T data[N0][N1];
};

template <typename T, unsigned N0, unsigned N1, unsigned N2>
struct Tensor3d : Tensor<T> {
  Tensor3d(void) = default;

  // Copy constructor with single value.
  Tensor3d(T val) { fill(val); }

  // Copy constructor with plain C array.
  Tensor3d(const T t[N0][N1][N2]) {
    for (unsigned i = 0; i < N0; i++) {
      for (unsigned j = 0; j < N1; j++) {
        for (unsigned k = 0; k < N2; k++) {
          data[i][j][k] = t[i][j][k];
        }
      }
    }
  }

  // Copy constructor with tensor.
  Tensor3d(const Tensor3d& t) : Tensor3d(t.data) {}

  // Fill the array with a value.
  void fill(T val) {
    for (unsigned i = 0; i < N0; i++) {
      for (unsigned j = 0; j < N1; j++) {
        for (unsigned k = 0; k < N2; k++) {
          data[i][j][k] = val;
        }
      }
    }
  }

  T data[N0][N1][N2];
};

template <typename T, unsigned N0, unsigned N1, unsigned N2, unsigned N3>
struct Tensor4d : Tensor<T> {
  Tensor4d(void) = default;

  // Copy constructor with single value.
  Tensor4d(T val) { fill(val); }

  // Copy constructor with plain C array.
  Tensor4d(const T t[N0][N1][N2][N3]) {
    for (unsigned i = 0; i < N0; i++) {
      for (unsigned j = 0; j < N1; j++) {
        for (unsigned k = 0; k < N2; k++) {
          for (unsigned l = 0; l < N3; l++) {
            data[i][j][k][l] = t[i][j][k][l];
          }
        }
      }
    }
  }

  // Copy constructor with tensor.
  Tensor4d(const Tensor4d& t) : Tensor4d(t.data) {}

  // Fill the array with a value.
  void fill(T val) {
    for (unsigned i = 0; i < N0; i++) {
      for (unsigned j = 0; j < N1; j++) {
        for (unsigned k = 0; k < N2; k++) {
          for (unsigned l = 0; l < N3; l++) {
            data[i][j][k][l] = val;
          }
        }
      }
    }
  }

  T data[N0][N1][N2][N3];
};

#endif  // SILVIA_TENSOR_H_
