// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <functional>
#include <ostream>
#include <string>

namespace nsclient {
namespace web {

// CLI-driven install/uninstall/status for the web frontend bundle.
//
// Decoupled from both the daemon core and the plugin API: the caller injects
// a path resolver (typically `[this](auto p) { return get_core()->expand_path(p); }`)
// and an output stream that the WEBServer module sets back into its protobuf
// response. Keeps the download/verify/extract logic in one place that can be
// unit-tested without standing up either side.
class web_installer {
 public:
  struct options {
    std::string version;     // Defaults to the daemon's build version.
    std::string from_path;   // If set: skip download, use this local zip.
    std::string url_base;    // GitHub releases base URL; defaults to the project's.
    bool force = false;      // Overwrite existing install without prompting.
    bool dry_run = false;    // Report intended actions, touch nothing.
  };

  using path_resolver = std::function<std::string(const std::string&)>;

  explicit web_installer(path_resolver resolve);

  // Returns 0 on success, non-zero on any failure. All user-facing text goes
  // to `out`; the caller decides whether that becomes set_response_good or
  // set_response_bad based on the return value.
  int install(const options& opts, std::ostream& out) const;
  int uninstall(bool force, std::ostream& out) const;
  int status(std::ostream& out) const;

 private:
  path_resolver resolve_;
};

}  // namespace web
}  // namespace nsclient
