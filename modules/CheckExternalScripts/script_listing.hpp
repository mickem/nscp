// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <string>
#include <vector>

// The files in the external scripts folder, as `list` prints them and as the
// `scripts.external` fact set reports them: one walk, so the two cannot
// drift.
namespace script_listing {

// Every regular file below `dir`, in the order the walk finds them.
//
// A file under `base` is spelled relative to it, which is the case on Windows
// and is what `show` and the scripts API have always been handed. When it is
// not - the normal case on unix, where ${scripts} is not below ${base-path} -
// the path is left absolute. It used to have its leading separator sliced off
// regardless, leaving a rootless `usr/lib/nsclient/scripts/x` that named no
// file at all.
//
// Scripts under a folder named exactly `lib` are skipped unless
// `include_lib`. Matched on a path *component*: a substring match on the
// whole path wrongly excluded a `libs` or `calibrate` folder.
//
// False, with `error` set, when the folder is not there or cannot be read:
// that is not an empty folder, and the caller asked what is in it.
inline bool list_files(const boost::filesystem::path &dir, const boost::filesystem::path &base, const bool include_lib, std::vector<std::string> &files,
                       std::string &error) {
  boost::system::error_code ec;
  boost::filesystem::recursive_directory_iterator iter(dir, ec), eod;
  if (ec) {
    error = "Failed to list " + dir.string() + ": " + ec.message();
    return false;
  }
  for (boost::filesystem::path const &i : boost::make_iterator_range(iter, eod)) {
    std::string s = i.string();
    if (boost::algorithm::starts_with(s, base.string())) {
      s = s.substr(base.string().size());
      if (!s.empty() && (s[0] == '\\' || s[0] == '/')) s = s.substr(1);
    }
    if (s.empty()) continue;
    bool in_lib = false;
    for (const boost::filesystem::path &part : i) {
      if (part.string() == "lib") {
        in_lib = true;
        break;
      }
    }
    if (boost::filesystem::is_regular_file(i) && (include_lib || !in_lib)) files.push_back(s);
  }
  return true;
}

}  // namespace script_listing
