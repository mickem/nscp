/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <functional>
#include <ostream>
#include <string>

namespace nsclient {
namespace web {

// CLI-driven install/uninstall/status for the web frontend bundle.
// Phase 2 of docs/design/web-bundle-installer.md.
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
