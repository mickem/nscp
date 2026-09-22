// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem.hpp>
#include <string>

// Path arithmetic shared by the ext-scr CLI and its tests. Header-only so the
// Windows-only decision below can be unit-tested on Windows, where the bug it
// fixes lives: a Linux integration test cannot see it at all.
namespace script_paths {

// weakly_canonical, falling back to the lexical path when resolution fails -
// the same treatment validate_sandbox gives both sides of its comparison.
inline boost::filesystem::path resolve(const boost::filesystem::path &path) {
  boost::system::error_code ec;
  const boost::filesystem::path resolved = boost::filesystem::weakly_canonical(path, ec);
  return ec ? path : resolved;
}

// `file` written relative to `base`, in the platform's own separators, or ""
// when it does not sit below `base`.
//
// Used to decide whether an imported script can be recorded the short, historic
// way. It is a question about where the file really is, not about what the
// layout is assumed to be: `script root` is settable and ${scripts} moves with
// a [paths] override, so the two are only related by default. Both sides go
// through resolve() so a junction, a symlink or a `..` cannot make an unrelated
// folder look like a child of the install directory.
inline std::string relative_to(const boost::filesystem::path &base, const boost::filesystem::path &file) {
  if (base.empty()) return "";
  const boost::filesystem::path canonical_base = resolve(base);
  const boost::filesystem::path canonical_file = resolve(file);
  boost::system::error_code ec;
  boost::filesystem::path rel = boost::filesystem::relative(canonical_file, canonical_base, ec);
  if (ec || rel.empty()) return "";
  // relative() happily climbs out with "..", which would be a path that only
  // resolves from this one working directory - exactly what we are avoiding.
  if (*rel.begin() == "..") return "";
  // make_preferred() mutates, so `rel` cannot be const here.
  return rel.make_preferred().string();
}

}  // namespace script_paths
