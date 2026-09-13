// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/paths/types.h"

#include "formats/classification.h"
#include "resources/layout.h"
#include "resources/selector.h"
#include "resources/sidecar_io.h"

TEST_SUITE("CLI sidecar I/O") {
  TEST_CASE("Automatic rebasing requires a selector-qualified layout transition") {
    cli::ClassifiedPath selected_input;
    selected_input.layout = cli::ResourceLayout::kGame;
    selected_input.resource_kind = ARX_RESOURCE_KIND_MODEL;
    const cli::SidecarEndpoint selected = cli::sidecarEndpoint(selected_input, ARX_RESOURCE_KIND_MODEL);

    cli::ClassifiedPath raw_game_input;
    raw_game_input.layout = cli::ResourceLayout::kGame;
    const cli::SidecarEndpoint raw_game = cli::sidecarEndpoint(raw_game_input, ARX_RESOURCE_KIND_MODEL);

    cli::ClassifiedPath loose_input;
    loose_input.layout = cli::ResourceLayout::kLoose;
    const cli::SidecarEndpoint loose = cli::sidecarEndpoint(loose_input, ARX_RESOURCE_KIND_MODEL);

    cli::OutputTarget selected_output;
    selected_output.layout = cli::ResourceLayout::kGame;
    selected_output.selector.kind = ARX_RESOURCE_KIND_MODEL;
    const cli::SidecarEndpoint selected_target = cli::sidecarEndpoint(selected_output, ARX_RESOURCE_KIND_MODEL);

    cli::OutputTarget raw_game_output;
    raw_game_output.layout = cli::ResourceLayout::kGame;
    const cli::SidecarEndpoint raw_game_target = cli::sidecarEndpoint(raw_game_output, ARX_RESOURCE_KIND_MODEL);

    cli::OutputTarget loose_output;
    loose_output.layout = cli::ResourceLayout::kLoose;
    const cli::SidecarEndpoint loose_target = cli::sidecarEndpoint(loose_output, ARX_RESOURCE_KIND_MODEL);

    CHECK(cli::automaticSidecarRebase(selected, loose_target) == cli::SidecarRebaseDirection::kToLoose);
    CHECK(cli::automaticSidecarRebase(loose, selected_target) == cli::SidecarRebaseDirection::kToGame);
    CHECK(cli::automaticSidecarRebase(selected, selected_target) == cli::SidecarRebaseDirection::kNone);
    CHECK(cli::automaticSidecarRebase(loose, loose_target) == cli::SidecarRebaseDirection::kNone);
    CHECK(cli::automaticSidecarRebase(raw_game, loose_target) == cli::SidecarRebaseDirection::kNone);
    CHECK(cli::automaticSidecarRebase(loose, raw_game_target) == cli::SidecarRebaseDirection::kNone);
  }

  TEST_CASE("Sidecar endpoints require the route's selector kind") {
    cli::ClassifiedPath input;
    input.layout = cli::ResourceLayout::kGame;
    input.resource_kind = ARX_RESOURCE_KIND_ANIMATION;
    CHECK(cli::sidecarEndpoint(input, ARX_RESOURCE_KIND_ANIMATION).selector_addressed);
    CHECK_FALSE(cli::sidecarEndpoint(input, ARX_RESOURCE_KIND_MODEL).selector_addressed);

    cli::OutputTarget output;
    output.layout = cli::ResourceLayout::kGame;
    output.selector.kind = ARX_RESOURCE_KIND_MODEL;
    CHECK(cli::sidecarEndpoint(output, ARX_RESOURCE_KIND_MODEL).selector_addressed);
    CHECK_FALSE(cli::sidecarEndpoint(output, ARX_RESOURCE_KIND_LEVEL).selector_addressed);
  }
}
