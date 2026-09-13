// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/help_request.h"

#include "console/diagnostics.h"
#include "routes/descriptor.h"
#include "routes/registry.h"
#include "routes/types.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace {

enum class MatchStatus : std::uint8_t {
  kNone,
  kUnique,
  kAmbiguous,
};

struct TopicMatch {
  MatchStatus status = MatchStatus::kNone;
  const cli::RouteDescriptor* route = nullptr;
  const char* name = nullptr;
  const char* alternate_name = nullptr;
};

bool optionLike(std::string_view value) { return value.size() > 1 && value.front() == '-'; }

TopicMatch matchPrimaryTopic(std::string_view topic) {
  if (topic.empty()) return {};
  if (topic == "cli") return {.status = MatchStatus::kUnique, .name = "cli"};

  cli::RouteRegistryView routes = cli::routeRegistry();
  for (std::size_t index = 0; index < routes.count; ++index) {
    if (topic == routes.routes[index].name) {
      return {.status = MatchStatus::kUnique, .route = &routes.routes[index], .name = routes.routes[index].name};
    }
  }

  TopicMatch match;
  if (std::string_view("cli").starts_with(topic)) {
    match.status = MatchStatus::kUnique;
    match.name = "cli";
  }
  for (std::size_t index = 0; index < routes.count; ++index) {
    if (!std::string_view(routes.routes[index].name).starts_with(topic)) continue;
    if (match.status == MatchStatus::kUnique) {
      match.status = MatchStatus::kAmbiguous;
      match.alternate_name = routes.routes[index].name;
      return match;
    }
    match.status = MatchStatus::kUnique;
    match.route = &routes.routes[index];
    match.name = routes.routes[index].name;
  }
  return match;
}

MatchStatus matchPageTopic(std::string_view topic, const cli::RouteDescriptor* route, cli::HelpPage& page) {
  if (topic.empty()) return MatchStatus::kNone;

  if (!route) {
    if (topic == "selectors") {
      page = cli::HelpPage::kSelectors;
      return MatchStatus::kUnique;
    }
    if (topic == "formats") {
      page = cli::HelpPage::kFormats;
      return MatchStatus::kUnique;
    }

    const bool selectors = std::string_view("selectors").starts_with(topic);
    const bool formats = std::string_view("formats").starts_with(topic);
    if (selectors && formats) return MatchStatus::kAmbiguous;
    if (selectors) {
      page = cli::HelpPage::kSelectors;
      return MatchStatus::kUnique;
    }
    if (formats) {
      page = cli::HelpPage::kFormats;
      return MatchStatus::kUnique;
    }
    return MatchStatus::kNone;
  }

  if (route->kind != cli::RouteKind::kLevel) return MatchStatus::kNone;
  if (topic == "debug" || std::string_view("debug").starts_with(topic)) {
    page = cli::HelpPage::kDebug;
    return MatchStatus::kUnique;
  }
  return MatchStatus::kNone;
}

}  // namespace

namespace cli {

bool resolveHelpRequest(std::span<const char* const> arguments, HelpRequest& out) {
  HelpRequest resolved;
  if (arguments.empty()) {
    out = resolved;
    return true;
  }

  std::string_view primary = arguments.front() ? std::string_view(arguments.front()) : std::string_view();
  if (optionLike(primary)) {
    out = resolved;
    return true;
  }

  TopicMatch topic = matchPrimaryTopic(primary);
  if (topic.status == MatchStatus::kAmbiguous) {
    diagnostic(DiagnosticCode::kHelpTopicAmbiguous,
               "--help: ambiguous topic '%.*s'; use '%s' or '%s'",
               static_cast<int>(primary.size()),
               primary.data(),
               topic.name,
               topic.alternate_name);
    return false;
  }
  if (topic.status == MatchStatus::kNone) {
    out = resolved;
    return true;
  }
  resolved.route = topic.route;

  if (arguments.size() > 1) {
    std::string_view refinement = arguments[1] ? std::string_view(arguments[1]) : std::string_view();
    if (!optionLike(refinement)) {
      HelpPage page = HelpPage::kGeneral;
      MatchStatus page_status = matchPageTopic(refinement, resolved.route, page);
      if (page_status == MatchStatus::kAmbiguous) {
        diagnostic(DiagnosticCode::kHelpTopicAmbiguous,
                   "--help: ambiguous topic '%.*s'",
                   static_cast<int>(refinement.size()),
                   refinement.data());
        return false;
      }
      if (page_status == MatchStatus::kUnique) resolved.page = page;
    }
  }

  out = resolved;
  return true;
}

}  // namespace cli
