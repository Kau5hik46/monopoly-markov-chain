#pragma once
#include <cstddef>
#include <vector>

namespace monopoly::math {

// Dense, row-major matrix of doubles.
class Matrix {
 public:
  Matrix() = default;
  Matrix(std::size_t rows, std::size_t cols, double init = 0.0)
      : rows_(rows), cols_(cols), data_(rows * cols, init) {}

  double& operator()(std::size_t r, std::size_t c) { return data_[r * cols_ + c]; }
  double operator()(std::size_t r, std::size_t c) const { return data_[r * cols_ + c]; }

  std::size_t rows() const noexcept { return rows_; }
  std::size_t cols() const noexcept { return cols_; }

  // Standard matrix product (this * rhs). Requires cols() == rhs.rows().
  Matrix operator*(const Matrix& rhs) const;

  // Row-vector times matrix: result[j] = sum_i v[i] * (*this)(i, j).
  // Requires v.size() == rows().
  [[nodiscard]] std::vector<double> vecMul(const std::vector<double>& v) const;

 private:
  std::size_t rows_ = 0;
  std::size_t cols_ = 0;
  std::vector<double> data_;  // row-major, size rows_*cols_
};

}  // namespace monopoly::math
