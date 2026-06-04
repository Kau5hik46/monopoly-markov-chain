#include <gtest/gtest.h>
#include <cmath>
#include "math/matrix.h"

using monopoly::math::Matrix;
using monopoly::math::stationaryDistribution;

TEST(Matrix, ElementAccessAndDims) {
  Matrix m(2, 3, 0.0);
  EXPECT_EQ(m.rows(), 2u);
  EXPECT_EQ(m.cols(), 3u);
  m(1, 2) = 4.5;
  EXPECT_DOUBLE_EQ(m(1, 2), 4.5);
  EXPECT_DOUBLE_EQ(m(0, 0), 0.0);
}

TEST(Matrix, Multiply) {
  Matrix a(2, 2, 0.0);
  a(0, 0) = 1; a(0, 1) = 2; a(1, 0) = 3; a(1, 1) = 4;
  Matrix b(2, 2, 0.0);
  b(0, 0) = 5; b(0, 1) = 6; b(1, 0) = 7; b(1, 1) = 8;
  Matrix c = a * b;  // [[19,22],[43,50]]
  EXPECT_DOUBLE_EQ(c(0, 0), 19);
  EXPECT_DOUBLE_EQ(c(0, 1), 22);
  EXPECT_DOUBLE_EQ(c(1, 0), 43);
  EXPECT_DOUBLE_EQ(c(1, 1), 50);
}

TEST(Matrix, RowVectorTimesMatrix) {
  // result[j] = sum_i v[i] * M(i,j)
  Matrix m(2, 2, 0.0);
  m(0, 0) = 1; m(0, 1) = 2; m(1, 0) = 3; m(1, 1) = 4;
  auto r = m.vecMul({1.0, 1.0});  // {1*1+1*3, 1*2+1*4} = {4,6}
  ASSERT_EQ(r.size(), 2u);
  EXPECT_DOUBLE_EQ(r[0], 4);
  EXPECT_DOUBLE_EQ(r[1], 6);
}

TEST(Stationary, TwoStateChain) {
  // P = [[0.9,0.1],[0.5,0.5]]  =>  pi = [5/6, 1/6]
  Matrix p(2, 2, 0.0);
  p(0, 0) = 0.9; p(0, 1) = 0.1;
  p(1, 0) = 0.5; p(1, 1) = 0.5;
  auto pi = stationaryDistribution(p);
  ASSERT_EQ(pi.size(), 2u);
  EXPECT_NEAR(pi[0], 5.0 / 6.0, 1e-9);
  EXPECT_NEAR(pi[1], 1.0 / 6.0, 1e-9);
  EXPECT_NEAR(pi[0] + pi[1], 1.0, 1e-12);
}
