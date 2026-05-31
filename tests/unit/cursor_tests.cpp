// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "utils/cursor.h"
#include "utils/text_cursor.h"

#include <cstdint>
#include <vector>

TEST_SUITE("cursor") {
  TEST_CASE("ReadBasic") {
    uint8_t data[] = {0x01, 0x00, 0x00, 0x00};  // little-endian int32 = 1
    pistoris::ReadCursor c(data, sizeof(data));

    int32_t val = 0;
    c.read(val);
    CHECK(c);
    CHECK(val == 1);
    CHECK(c.remaining() == 0);
  }

  TEST_CASE("ReadPastEnd") {
    uint8_t data[] = {0x01, 0x02};
    pistoris::ReadCursor c(data, sizeof(data));

    int32_t val = 0;
    c.read(val);  // needs 4 bytes, only 2 available
    CHECK(!c);
    CHECK(c.error().offset == 0);
    CHECK(c.error().needed == 4);
  }

  TEST_CASE("StickyFailure") {
    uint8_t data[] = {0x01, 0x02};
    pistoris::ReadCursor c(data, sizeof(data));

    int32_t big  = 0;
    int8_t small = 0;
    c.read(big);    // fails
    c.read(small);  // skipped: cursor already failed
    CHECK(!c);
    CHECK(small == 0);
  }

  TEST_CASE("ReadArray") {
    uint8_t data[] = {
        0x01, 0x00, 0x02, 0x00, 0x03, 0x00  // three uint16_t: 1, 2, 3
    };
    pistoris::ReadCursor c(data, sizeof(data));

    std::vector<uint16_t> vals(3);
    c.readArray(vals);
    CHECK(c);
    CHECK(vals[0] == 1);
    CHECK(vals[1] == 2);
    CHECK(vals[2] == 3);
  }

  TEST_CASE("ReadArrayTruncated") {
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};  // 4 bytes
    pistoris::ReadCursor c(data, sizeof(data));

    std::vector<uint32_t> vals(2);  // needs 8 bytes
    c.readArray(vals);
    CHECK(!c);
    CHECK(c.error().kind == pistoris::CursorErrorKind::kUnexpectedEof);
  }

  TEST_CASE("Skip") {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    pistoris::ReadCursor c(data, sizeof(data));

    c.skip(2);
    CHECK(c);
    CHECK(c.remaining() == 2);

    uint8_t val = 0;
    c.read(val);
    CHECK(val == 0xCC);
  }

  TEST_CASE("SkipPastEnd") {
    uint8_t data[] = {0x01, 0x02};
    pistoris::ReadCursor c(data, sizeof(data));

    c.skip(3);
    CHECK(!c);
    CHECK(c.error().kind == pistoris::CursorErrorKind::kUnexpectedEof);
    CHECK(c.error().needed == 3);
    CHECK(c.error().offset == 0);
  }

  TEST_CASE("ReadArrayPresized") {
    uint8_t data[] = {0x01, 0x00, 0x02, 0x00};  // two uint16_t: 1, 2
    pistoris::ReadCursor c(data, sizeof(data));

    std::vector<uint16_t> vals(2);
    c.readArray(vals);
    CHECK(c);
    CHECK(vals[0] == 1);
    CHECK(vals[1] == 2);
    CHECK(c.remaining() == 0);
  }

  TEST_CASE("ReadArrayEmpty") {
    uint8_t data[] = {0x01, 0x02};
    pistoris::ReadCursor c(data, sizeof(data));

    std::vector<uint32_t> vals;
    c.readArray(vals);
    CHECK(c);
    CHECK(vals.empty());
    CHECK(c.remaining() == 2);
  }

  TEST_CASE("WriteCursorErrorInitiallyOk") {
    pistoris::WriteCursor c;
    auto err = c.error();
    CHECK(err.kind == pistoris::CursorErrorKind::kOk);
    CHECK(err.offset == 0);
    CHECK(err.needed == 0);
  }

  TEST_CASE("WriteCursorEmptyWritesAreNoops") {
    pistoris::WriteCursor c;
    std::vector<std::uint16_t> empty;
    std::uint8_t byte = 0xAA;

    c.writeArray(empty);
    c.writeN(&byte, 0);
    c.pad(0);

    CHECK(c);
    CHECK(c.size() == 0);
    CHECK(c.take().empty());
  }

  TEST_CASE("TextCursorNewlineAdvancesTokenPosition") {
    pistoris::TextCursor c(" \n\tname");

    auto tok = c.next();

    CHECK(tok.kind == pistoris::TokenKind::kString);
    CHECK(tok.text == "name");
    CHECK(tok.line == 2);
    CHECK(tok.col == 2);
  }

  TEST_CASE("TextCursorRestOfLineBlankLineReturnsEnd") {
    pistoris::TextCursor c("   \nnext");

    auto tok = c.restOfLine();

    CHECK(tok.kind == pistoris::TokenKind::kEnd);
    CHECK(tok.text.empty());
    CHECK(tok.line == 1);
    CHECK(tok.col == 4);
  }

  TEST_CASE("TextCursorRestOfLineAtEndReturnsEnd") {
    pistoris::TextCursor c("");

    auto tok = c.restOfLine();

    CHECK(tok.kind == pistoris::TokenKind::kEnd);
    CHECK(tok.text.empty());
    CHECK(tok.line == 1);
    CHECK(tok.col == 1);
  }

}  // TEST_SUITE("cursor")
