#include "domain/board_factory.h"

#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace monopoly::domain {

Board loadBoardFromFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open board file: " + path);
  nlohmann::json j;
  in >> j;

  const auto& arr = j.at("squares");
  if (arr.size() != static_cast<std::size_t>(kBoardSize))
    throw std::runtime_error("board must have exactly 40 squares");

  std::array<Square, kBoardSize> squares{};
  for (const auto& e : arr) {
    Square s;
    s.position = e.at("position").get<int>();
    s.name = e.at("name").get<std::string>();
    s.type = squareTypeFromString(e.at("type").get<std::string>());
    s.group = colorGroupFromString(e.at("group").get<std::string>());
    s.price = e.at("price").get<long>();
    s.houseCost = e.at("houseCost").get<long>();
    s.mortgage = e.at("mortgage").get<long>();
    if (s.position < 0 || s.position >= kBoardSize)
      throw std::runtime_error("square position out of range");
    squares[static_cast<std::size_t>(s.position)] = s;
  }
  return Board(std::move(squares));
}

}  // namespace monopoly::domain
