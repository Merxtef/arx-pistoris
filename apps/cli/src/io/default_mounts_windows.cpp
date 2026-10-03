// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "io/default_mounts.h"

#include <combaseapi.h>
#include <filesystem>
#include <knownfolders.h>
#include <memory>
#include <shlobj.h>
#include <string>

namespace {

class ComInitialization {
 public:
  explicit ComInitialization(HRESULT result) : initialized_(SUCCEEDED(result)) {}

  ComInitialization(const ComInitialization&) = delete;
  ComInitialization& operator=(const ComInitialization&) = delete;

  ~ComInitialization() {
    if (initialized_) CoUninitialize();
  }

 private:
  bool initialized_;
};

struct CoTaskMemDeleter {
  void operator()(void* memory) const noexcept { CoTaskMemFree(memory); }
};

}  // namespace

namespace cli {

bool defaultGameResourceRoot(std::filesystem::path& out, std::string& error) {
  out.clear();
  error.clear();
  const HRESULT initialize_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  const ComInitialization initialization(initialize_result);
  if (FAILED(initialize_result) && initialize_result != RPC_E_CHANGED_MODE) {
    error = "COM initialization failed";
    return false;
  }

  PWSTR saved_games = nullptr;
  const HRESULT result = SHGetKnownFolderPath(FOLDERID_SavedGames, KF_FLAG_DONT_VERIFY, nullptr, &saved_games);
  const std::unique_ptr<wchar_t, CoTaskMemDeleter> saved_games_owner(saved_games);
  if (FAILED(result) || !saved_games) {
    error = "Saved Games folder is unavailable";
    return false;
  }
  out = std::filesystem::path(saved_games) / L"Arx Libertatis";
  return true;
}

}  // namespace cli
