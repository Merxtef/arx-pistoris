// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/name_tokens.h"

#include <doctest/doctest.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

using namespace pistoris;

namespace {

void checkTokens(const std::vector<std::string_view>& actual, std::initializer_list<std::string_view> expected) {
  REQUIRE(actual.size() == expected.size());
  auto expected_it = expected.begin();
  for (std::string_view token : actual) {
    CHECK(token.compare(*expected_it) == 0);
    ++expected_it;
  }
}

}  // namespace

TEST_CASE("double underscore detection is exact") {
  CHECK_FALSE(hasDoubleUnderscore(""));
  CHECK_FALSE(hasDoubleUnderscore("single_underscore"));
  CHECK(hasDoubleUnderscore("a__b"));
  CHECK(hasDoubleUnderscore("__a"));
  CHECK(hasDoubleUnderscore("a__"));
}

TEST_CASE("semantic strings reject reserved separators and embedded nulls") {
  CHECK(validSemanticString(""));
  CHECK(validSemanticString("single_underscore"));
  CHECK_FALSE(validSemanticString("a__b"));
  CHECK_FALSE(validSemanticString(std::string("a\0b", 3)));
  CHECK(hasEmbeddedNull(std::string("a\0b", 3)));
}

TEST_CASE("splitDoubleUnderscore preserves empty tokens") {
  std::vector<std::string_view> tokens;

  splitDoubleUnderscore("a__b", tokens);
  checkTokens(tokens, {"a", "b"});

  splitDoubleUnderscore("a__", tokens);
  checkTokens(tokens, {"a", ""});

  splitDoubleUnderscore("__a", tokens);
  checkTokens(tokens, {"", "a"});

  splitDoubleUnderscore("a____b", tokens);
  checkTokens(tokens, {"a", "", "b"});

  splitDoubleUnderscore("", tokens);
  checkTokens(tokens, {""});
}

TEST_CASE("joinDoubleUnderscore preserves token structure") {
  CHECK(joinDoubleUnderscore({"a", "b"}) == "a__b");
  CHECK(joinDoubleUnderscore({"a", ""}) == "a__");
  CHECK(joinDoubleUnderscore({"", "a"}) == "__a");
  CHECK(joinDoubleUnderscore({"a", "", "b"}) == "a____b");
  CHECK(joinDoubleUnderscore({""}) == "");
}

TEST_CASE("double underscore tokenization roundtrips") {
  std::vector<std::string_view> tokens;
  for (std::string_view text : {"", "a", "a__b", "a__", "__a", "a____b", "a__b__c__"}) {
    splitDoubleUnderscore(text, tokens);
    CHECK(joinDoubleUnderscore(tokens) == std::string(text));
  }
}
