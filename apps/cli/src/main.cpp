// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "app.h"

#ifdef _WIN32
#include "io/native_path.h"

#include <cstdio>
#include <string>
#include <vector>

int wmain(int argc, wchar_t* argv[]) {
  std::vector<std::string> utf8_arguments(static_cast<std::size_t>(argc));
  std::vector<char*> utf8_argv(static_cast<std::size_t>(argc));
  for (int index = 0; index < argc; ++index) {
    if (!cli::io_detail::wideToUtf8(argv[index], utf8_arguments[static_cast<std::size_t>(index)])) {
      std::fprintf(stderr, "[CLI_INVALID_ARGUMENT_ENCODING] Command-line argument is not valid Unicode\n");
      return 1;
    }
    utf8_argv[static_cast<std::size_t>(index)] = utf8_arguments[static_cast<std::size_t>(index)].data();
  }
  return cli::runCli(argc, utf8_argv.data());
}
#else
int main(int argc, char* argv[]) { return cli::runCli(argc, argv); }
#endif
