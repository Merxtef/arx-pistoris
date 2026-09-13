// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "utils/path.h"

#include <ostream>  // IWYU pragma: keep

TEST_SUITE("path utilities") {
  TEST_CASE("Decomposes filenames without allocating") {
    using namespace pistoris;

    CHECK(pathFilename("folder.with.dot/item.pie.png") == "item.pie.png");
    CHECK(pathStem("folder.with.dot/item.pie.png") == "item.pie");
    CHECK(pathExtension("folder.with.dot/item.pie.png") == ".png");
    CHECK(pathWithoutExtension("folder.with.dot/item.pie.png") == "folder.with.dot/item.pie");

    CHECK(pathFilename(R"(folder\item.png)") == "item.png");
    CHECK(pathStem("folder/item") == "item");
    CHECK(pathExtension("folder/item").empty());
    CHECK(pathWithoutExtension("folder/item") == "folder/item");

    CHECK(pathStem("folder/item.") == "item");
    CHECK(pathExtension("folder/item.") == ".");
    CHECK(pathWithoutExtension("folder/item.") == "folder/item");
  }
}
