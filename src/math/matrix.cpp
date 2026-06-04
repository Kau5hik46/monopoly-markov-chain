#include "math/matrix.h"

#include <cmath>

namespace monopoly::math {

Matrix Matrix::operator*(const Matrix& rhs) const {
  Matrix out(rows_, rhs.cols_, 0.0);
  for (std::size_t i = 0; i < rows_; ++i) {
    for (std::size_t k = 0; k < cols_; ++k) {
      const double aik = (*this)(i, k);
      if (aik == 0.0) continue;
      for (std::size_t j = 0; j < rhs.cols_; ++j) {
        out(i, j) += aik * rhs(k, j);
      }
    }
  }
  return out;
}

std::vector<double> Matrix::vecMul(const std::vector<double>& v) const {
  std::vector<double> out(cols_, 0.0);
  for (std::size_t i = 0; i < rows_; ++i) {
    const double vi = v[i];
    if (vi == 0.0) continue;
    for (std::size_t j = 0; j < cols_; ++j) {
      out[j] += vi * (*this)(i, j);
    }
  }
  return out;
}

std::vector<double> stationaryDistribution(const Matrix& P, double tol,
                                           std::size_t maxIter) {
  const std::size_t n = P.rows();
  std::vector<double> v(n, 1.0 / static_cast<double>(n));
  for (std::size_t iter = 0; iter < maxIter; ++iter) {
    std::vector<double> next = P.vecMul(v);
    double sum = 0.0;
    for (double x : next) sum += x;
    if (sum > 0.0) {
      for (double& x : next) x /= sum;  // guard against drift
    }
    double diff = 0.0;
    for (std::size_t i = 0; i < n; ++i) diff += std::fabs(next[i] - v[i]);
    v.swap(next);
    if (diff < tol) break;
  }
  return v;
}

}  // namespace monopoly::math
