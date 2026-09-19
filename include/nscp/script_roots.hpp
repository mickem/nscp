// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <vector>

#include <file_helpers.hpp>
#include <str/utils.hpp>

// The folders a configured script may be loaded from.
//
// The script modules resolve a configured name by searching a list of
// candidates, and the list ends with the value joined onto the script folder.
// That join does not stop the value climbing back out: `../foo.py` resolves to
// ${scripts}/../foo.py and loads perfectly happily from the installation
// directory. The ext-scr CLI has always held `show` and `delete` inside the
// script root for exactly this reason; the loader had no equivalent check, so
// what the CLI refused to read, the configuration could still run.
//
// Hence the same validation on the loading path. It cannot simply be
// "underneath ${scripts}", though: plenty of installations run scripts the
// agent did not ship and does not own - the monitoring-plugins package under
// its libexec directory, a vendor's plugin in /opt - so the list is extensible
// and an operator adds the folders their scripts actually live in.
//
// Containment is checked after resolving symlinks, the same way
// CheckExternalScripts::validate_sandbox does it: path_contains_file compares
// lexically, so a link planted inside an allowed folder but pointing out of it
// would otherwise pass.
namespace nscp {
namespace scripts {

class allowed_roots {
 public:
  // Add one folder. Empty values are ignored so an unset setting adds nothing.
  void add(const std::string &root) {
    if (root.empty()) return;
    roots_.push_back(resolve(root));
  }

  // Add a comma-separated list, as an operator writes it in the settings.
  void add_list(const std::string &roots) {
    for (const std::string &entry : str::utils::split_lst(roots, std::string(","))) {
      std::string trimmed = entry;
      boost::algorithm::trim(trimmed);
      add(trimmed);
    }
  }

  bool empty() const { return roots_.empty(); }

  // True when `script` lies inside one of the roots. With no roots configured
  // nothing is allowed - callers always add their own script folder first, so
  // an empty list means "not set up yet" rather than "allow everything".
  bool allows(const boost::filesystem::path &script) const {
    const boost::filesystem::path real = resolve(script.string());
    for (const boost::filesystem::path &root : roots_) {
      if (file_helpers::checks::path_contains_file(root, real)) return true;
    }
    return false;
  }

  // The roots, for the error message an operator has to act on. Naming them is
  // the difference between "not allowed" and "not allowed, add it here".
  std::string describe() const {
    std::string out;
    for (const boost::filesystem::path &root : roots_) {
      if (!out.empty()) out += ", ";
      out += root.string();
    }
    return out;
  }

 private:
  // weakly_canonical, falling back to the lexical path when resolution fails -
  // a script that does not exist yet still has to be classifiable.
  static boost::filesystem::path resolve(const std::string &path) {
    boost::system::error_code ec;
    const boost::filesystem::path canonical = boost::filesystem::weakly_canonical(boost::filesystem::path(path), ec);
    return ec ? boost::filesystem::path(path) : canonical;
  }

  std::vector<boost::filesystem::path> roots_;
};

}  // namespace scripts
}  // namespace nscp
