// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The script-file operations behind `nscp lua|py list/show/delete` and the
// /api/v2/scripts/<runtime> endpoints, shared by LUAScript and PythonScript:
// each runtime keeps its scripts in ${scripts}/<folder> with its own file
// extension, and nothing else about these differs between them.

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <file_helpers.hpp>
#include <fstream>
#include <iterator>
#include <list>
#include <string>

namespace scripts {
namespace files {

namespace fs = boost::filesystem;

enum class sandbox_use { read, remove };

inline bool exists_entry(const fs::path &p) {
  boost::system::error_code ec;
  return fs::exists(fs::symlink_status(p, ec));
}

// The entry a `--script` name refers to, resolved only inside `root`
// (`${scripts}/<folder>`). Unlike the loaders' find_file() this does not try
// the name as given - relative to the working directory, or absolute - so a
// name cannot reach a file outside the scripts folder. `<folder>/foo.<ext>`
// (the form `list` and the REST listing print) and `foo.<ext>` / `foo` (the
// form a script is configured by) both resolve. `outside` is set when a name
// only resolves to something outside `root`, so the caller can say why it
// refused.
//
// Containment is decided on real paths, symlinks resolved: a lexical test
// alone lets a symlink inside the folder (or a symlinked sub-folder) reach any
// file the service account can, the hole CheckExternalScripts' sandbox closed
// the same way. A path that cannot be resolved counts as outside.
//
// - read: a regular file whose real path - the link's target, for a link - is
//   inside the folder.
// - remove: a regular file or a symlink (dangling, or to a directory, too)
//   that itself sits in the folder once any symlinked folder above it is
//   resolved. What a link points at does not matter, since removing a link
//   never touches its target; a real directory is never a candidate.
//
// The path returned is the one inside the folder, so delete removes a link,
// never its target.
inline boost::optional<fs::path> resolve_in_sandbox(const fs::path &root, const std::string &script, const std::string &extension, const sandbox_use use,
                                                    bool &outside) {
  outside = false;
  if (script.empty()) return boost::none;
  boost::system::error_code ec;
  const fs::path real_root = fs::weakly_canonical(root, ec);
  if (ec) return boost::none;
  const fs::path parent = root.parent_path();
  const std::list<fs::path> candidates = {root / script, root / (script + extension), parent / script, parent / (script + extension)};
  for (const fs::path &c : candidates) {
    const fs::path candidate = c.lexically_normal();
    if (!exists_entry(candidate)) continue;
    if (!file_helpers::checks::path_contains_file(root, candidate)) {
      outside = true;
      continue;
    }
    const bool is_link = fs::is_symlink(fs::symlink_status(candidate, ec));
    const bool is_file = fs::is_regular_file(candidate, ec);
    if (use == sandbox_use::read ? !is_file : !(is_file || is_link)) continue;
    // Where the entry itself lives, any symlinked folder above it resolved.
    const fs::path real_parent = fs::weakly_canonical(candidate.parent_path(), ec);
    if (ec || !file_helpers::checks::path_contains_file(real_root, real_parent / candidate.filename())) {
      outside = true;
      continue;
    }
    if (use == sandbox_use::read) {
      const fs::path real = fs::weakly_canonical(candidate, ec);
      if (ec || !file_helpers::checks::path_contains_file(real_root, real)) {
        outside = true;
        continue;
      }
    }
    return candidate;
  }
  return boost::none;
}

// The file a configured entry loads, found the way the loaders find it: as
// written - absolute, or relative to the working directory - then under
// ${scripts}/<folder> and ${scripts}, each with and without the extension.
// `spellings` is the entry as configured and with any ${...} expanded; each is
// also tried with `\` read as a separator, so an entry written for another
// platform or with a path variable still matches the file it names. Nothing is
// logged for a miss.
inline boost::optional<fs::path> configured_file(const fs::path &scripts, const std::string &folder, const std::string &extension,
                                                 const std::list<std::string> &spellings) {
  std::list<std::string> forms = spellings;
  for (const std::string &f : spellings) forms.push_back(boost::algorithm::replace_all_copy(f, "\\", "/"));
  for (const std::string &f : forms) {
    for (const fs::path &c :
         {fs::path(f), fs::path(f + extension), scripts / folder / f, scripts / folder / (f + extension), scripts / f, scripts / (f + extension)}) {
      if (exists_entry(c)) return c;
    }
  }
  return boost::none;
}

// The directory entry `p` names: any symlinked folder above it resolved, the
// entry itself not followed - a link is its own entry, not its target's.
inline fs::path entry_identity(const fs::path &p) {
  const fs::path abs = fs::absolute(p).lexically_normal();
  boost::system::error_code ec;
  const fs::path parent = fs::weakly_canonical(abs.parent_path(), ec);
  if (ec) return abs;
  return parent / abs.filename();
}

// Whether two spellings name the same directory entry. This used to fall back
// to fs::equivalent(), which follows symlinks: a link and the file it points
// at compared equal, so deleting a link inside the folder also dropped the
// configuration entry of its target. Windows compares without regard to
// case, which is what equivalent() was covering there.
inline bool same_entry(const fs::path &a, const fs::path &b) {
#ifdef WIN32
  fs::path ia = entry_identity(a), ib = entry_identity(b);
  return boost::algorithm::iequals(ia.make_preferred().string(), ib.make_preferred().string());
#else
  return entry_identity(a) == entry_identity(b);
#endif
}

// The script's contents, or nothing when it cannot be read - an unreadable
// file must not pass for an empty one.
inline boost::optional<std::string> read_file(const fs::path &file) {
  std::ifstream in(file.string().c_str(), std::ios::in | std::ios::binary);
  if (!in.is_open()) return boost::none;
  std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (in.bad()) return boost::none;
  return data;
}

// Whether `file`, somewhere under `dir`, sits in a folder named `lib` below
// `dir` - the shared helpers `require()` and `import` load, which are not
// scripts of their own.
inline bool in_lib_folder(const fs::path &dir, const fs::path &file) {
  const fs::path rel = file.parent_path().lexically_relative(dir);
  for (const fs::path &part : rel) {
    if (part == "lib") return true;
  }
  return false;
}

// The files under `${scripts}/<folder>` (`dir`), spelled relative to
// ${scripts} (`scripts`) - `lua/x.lua`, the spelling `add` takes straight
// back and find_file resolves from any working directory. A file reached
// through a symlink out of the folder is left absolute rather than mangled.
// Helpers in a `lib` folder below `dir` are left out unless `include_lib`.
//
// Only the path below `dir` is looked at for `lib`. The whole path used to
// be, so on a Linux package - where ${scripts} is /usr/lib/nsclient/scripts -
// every file matched and the listing was always empty.
inline std::list<std::string> list_files(const fs::path &dir, const fs::path &scripts, const bool include_lib) {
  std::list<std::string> ret;
  boost::system::error_code ec;
  if (!fs::is_directory(dir, ec)) return ret;
  const std::string rel = scripts.string();
  for (fs::recursive_directory_iterator iter(dir, ec), eod; !ec && iter != eod; iter.increment(ec)) {
    const fs::path &i = iter->path();
    if (!fs::is_regular_file(i, ec)) continue;
    if (!include_lib && in_lib_folder(dir, i)) continue;
    std::string s = i.string();
    if (boost::algorithm::starts_with(s, rel)) {
      s = s.substr(rel.size());
      if (!s.empty() && (s[0] == '\\' || s[0] == '/')) s = s.substr(1);
    }
    if (!s.empty()) ret.push_back(s);
  }
  return ret;
}

}  // namespace files
}  // namespace scripts
