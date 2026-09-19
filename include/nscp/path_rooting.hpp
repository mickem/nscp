// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem/path.hpp>
#include <stdexcept>
#include <string>

#include <nscp/path_defaults.hpp>

// Rooting a configured path at the folder its consumer owns.
//
// Expanding a path and rooting one are different jobs, and keeping them apart
// is the point of this header. Expansion substitutes ${tokens}; it does not
// make anything absolute, and it must not, because an operator is entitled to
// point a setting anywhere on the filesystem - `/var/log/mine.log` is a
// perfectly good answer and not ours to relocate.
//
// But a value carrying neither a token nor a root only means something
// relative to *some* directory, and the one it has been relative to until now
// is the process working directory: C:\Windows\System32 for a Windows service,
// "/" under a bare init script, the package directory under the shipped systemd
// unit. An operator cannot predict which, and on the write side it means files
// the agent creates land where nobody looks for them.
//
// So the consumer - which is the only thing that knows - names the folder it
// owns, and a bare name resolves there. Only consumers that genuinely own a
// namespace should do this. A script *name* is not such a case: it is resolved
// by its provider's search list, which can try several candidates and report
// "not found". A write destination has no search and no failure mode, which is
// exactly why it has to be rooted.
//
// This lives in one header, taking the expander as a parameter, because three
// layers need identical behaviour and reach the path resolver differently: the
// core through path_manager, the settings layer through settings_core, and
// modules across the plugin ABI. Composing here rather than adding an ABI entry
// point keeps NSAPIExpandPath the only exported path call.
namespace nscp {
namespace paths {

// A ${token} named something the installation cannot resolve, or a root that is
// not one. Both are configuration or programming errors that used to be
// swallowed, and both are reported the same way.
class path_expansion_error : public std::runtime_error {
 public:
  explicit path_expansion_error(const std::string &what) : std::runtime_error(what) {}
};

// True when `path` names a location of its own, rather than something that only
// means anything relative to another directory. This is the test for "may a
// default root be joined onto this?".
//
// Deliberately NOT is_absolute(). On Windows `C:foo` is drive-relative and
// is_absolute() is false, yet the operator plainly named drive C; joining a
// root onto it would produce the nonsense `<root>/C:foo`. `\foo` is
// root-relative and equally not ours to move. A UNC `\\server\share` satisfies
// both tests anyway. Because boost parses roots per platform this is correct on
// POSIX for free: there `C:foo` really is an ordinary relative file name and
// does get rooted, which is what a unix operator means by it.
inline bool names_a_root(const boost::filesystem::path &path) { return path.has_root_name() || path.has_root_directory(); }

// Expand `file` with `expand`, then root the result at `default_root` when what
// came back does not name a location of its own.
//
// `expand` is any callable taking and returning std::string.
template <typename Expander>
std::string root_path(std::string file, const std::string &default_root, const Expander &expand) {
  const std::string expanded = expand(std::move(file));
  // An unset setting stays unset. A good number of path options use "" for "not
  // configured" and their consumers test .empty(), so handing back the root
  // directory would turn "no certificate key" into a key named after a folder.
  if (expanded.empty()) return expanded;
  // Likewise the "no path at all" sentinel: `none` must not become a file
  // called none inside the root.
  if (is_no_path(expanded)) return expanded;
  if (names_a_root(expanded)) return expanded;

  const std::string root = expand(default_root);
  if (root.empty() || !names_a_root(root)) {
    // Every call site passes a literal for the root, so this is our bug rather
    // than the operator's - and returning the value unrooted would leave it
    // resolving against the working directory, which is the failure being
    // fixed.
    throw path_expansion_error("Cannot root '" + expanded + "': the default root '" + default_root + "' does not resolve to an absolute path" +
                               (root.empty() ? "" : " (it resolves to '" + root + "')"));
  }
  return (boost::filesystem::path(root) / expanded).string();
}

}  // namespace paths
}  // namespace nscp
