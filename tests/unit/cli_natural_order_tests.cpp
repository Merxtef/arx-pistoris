// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "base/natural_order.h"

#include <algorithm>
#include <string>
#include <vector>

TEST_SUITE("CLI natural ordering") {
  TEST_CASE("Digit runs sort by numeric value") {
    std::vector<std::string> values = {
        "level:10", "level:02", "level:2", "level:1", "level:002", "level:20", "level:3"};
    std::ranges::sort(values, cli::naturalStringLess);
    CHECK(values ==
          std::vector<std::string>{"level:1", "level:2", "level:02", "level:002", "level:3", "level:10", "level:20"});
  }

  TEST_CASE("Each digit run is compared independently") {
    std::vector<std::string> values = {
        "part10-section1", "part2-section10", "part2-section2", "part2-section1", "part1-section20"};
    std::ranges::sort(values, cli::naturalStringLess);
    CHECK(values == std::vector<std::string>{
                        "part1-section20", "part2-section1", "part2-section2", "part2-section10", "part10-section1"});
  }

  TEST_CASE("Digit runs are not limited by an integer type") {
    std::vector<std::string> values = {
        "item100000000000000000000", "item99999999999999999999", "item10000000000000000000"};
    std::ranges::sort(values, cli::naturalStringLess);
    CHECK(values == std::vector<std::string>{
                        "item10000000000000000000", "item99999999999999999999", "item100000000000000000000"});
  }

  TEST_CASE("Non-digit bytes retain lexical ordering") {
    CHECK(cli::naturalStringLess("item-2", "item.2"));
    CHECK(cli::naturalStringLess("item2", "itema"));
    CHECK(cli::naturalStringLess("item", "item0"));
    CHECK_FALSE(cli::naturalStringLess("item2", "item2"));
  }
}
