cmake_minimum_required(VERSION 3.16)
project(monopoly_advisor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic)

# --- Third-party: nlohmann/json (header-only) ---
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# --- Core library (sources added as milestones progress) ---
add_library(monopoly_core
  src/math/matrix.cpp
  src/probability/dice.cpp
  src/domain/board.cpp
  src/domain/board_factory.cpp
  src/domain/deck_factory.cpp
  src/domain/game_state.cpp
  src/probability/landing.cpp
  src/probability/transition_matrix.cpp
  src/probability/probability_engine.cpp
  src/probability/monte_carlo.cpp
  src/risk/rent_table.cpp
  src/risk/forecast.cpp
  src/risk/game_sim.cpp
  src/pricing/insurance_pricer.cpp
  src/pricing/option_chain.cpp
  src/contracts/option_book.cpp
  src/rules/rule_config.cpp
  src/rules/rule_logic.cpp
  src/engine/name_table.cpp
  src/engine/completion.cpp
  src/engine/line_reader.cpp
  src/engine/lexer.cpp
  src/engine/parser.cpp
  src/engine/help.cpp
  src/engine/formatter.cpp
  src/engine/query_service.cpp
  src/engine/executor.cpp
  src/engine/session.cpp
  src/engine/repl.cpp
)
target_include_directories(monopoly_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(monopoly_core PUBLIC cxx_std_17)
target_link_libraries(monopoly_core PUBLIC nlohmann_json::nlohmann_json)

# --- REPL executable ---
add_executable(monopoly src/app/main.cpp)
target_link_libraries(monopoly PRIVATE monopoly_core)
target_compile_definitions(monopoly PRIVATE
  MONOPOLY_DATA_DIR="${CMAKE_SOURCE_DIR}/data")

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
  tests/unit/test_board.cpp
  tests/unit/test_deck.cpp
  tests/unit/test_microstate.cpp
  tests/unit/test_landing.cpp
  tests/unit/test_transition.cpp
  tests/unit/test_probability_engine.cpp
  tests/unit/test_game_state.cpp
  tests/unit/test_rent.cpp
  tests/unit/test_pricing.cpp
  tests/unit/test_rule_config.cpp
  tests/unit/test_rule_logic.cpp
  tests/unit/test_engine_lex.cpp
  tests/unit/test_engine_parse.cpp
  tests/unit/test_completion.cpp
  tests/unit/test_options_dsl.cpp
  tests/unit/test_options_exec.cpp
  tests/unit/test_engine_exec.cpp
  tests/unit/test_engine_repl.cpp
  tests/unit/test_monte_carlo.cpp
  tests/unit/test_option_chain.cpp
  tests/unit/test_pricing_strike.cpp
  tests/unit/test_option_book.cpp
  tests/unit/test_forecast.cpp
  tests/unit/test_game_sim.cpp
)
target_link_libraries(monopoly_tests PRIVATE monopoly_core GTest::gtest_main)
target_compile_definitions(monopoly_tests PRIVATE
  BOARD_JSON_PATH="${CMAKE_SOURCE_DIR}/data/board.london.json"
  DECKS_JSON_PATH="${CMAKE_SOURCE_DIR}/data/decks.london.json"
  RULES_JSON_PATH="${CMAKE_SOURCE_DIR}/data/rules.default.json")

include(GoogleTest)
gtest_discover_tests(monopoly_tests)
