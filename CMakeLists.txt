cmake_minimum_required(VERSION 3.16)
project(monopoly_advisor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic)

# --- Core library (sources added as milestones progress) ---
add_library(monopoly_core
  src/math/matrix.cpp
  src/probability/dice.cpp
)
target_include_directories(monopoly_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(monopoly_core PUBLIC cxx_std_17)

# --- REPL executable ---
add_executable(monopoly src/app/main.cpp)
target_link_libraries(monopoly PRIVATE monopoly_core)

# --- Tests via FetchContent GoogleTest ---
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(monopoly_tests
  tests/unit/test_smoke.cpp
  tests/unit/test_matrix.cpp
  tests/unit/test_dice.cpp
)
target_link_libraries(monopoly_tests PRIVATE monopoly_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(monopoly_tests)
