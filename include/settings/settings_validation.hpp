// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <map>
#include <string>

namespace settings {

// What a settings store physically holds: every section it has, with the keys
// under it, as written. `source` is the context of the store the section came
// from (the main file or one of its includes), so a finding can say which file
// to edit.
struct inventory_section {
  std::string source;
  std::string path;
  std::list<std::string> keys;
  inventory_section() = default;
  inventory_section(std::string source, std::string path, std::list<std::string> keys)
      : source(std::move(source)), path(std::move(path)), keys(std::move(keys)) {}
};
typedef std::list<inventory_section> inventory;

// The part of the settings registry the validator reads: every path a loaded
// module registered, the keys it registered under it, and whether the path
// takes keys the user names (aliases, scripts, targets - a "subkey" path).
struct registry_snapshot {
  struct path_entry {
    bool any_key = false;
    std::list<std::string> keys;
  };
  std::map<std::string, path_entry> paths;
};

struct validation_finding {
  enum class kind {
    // Keys above the first [section] header: no section, so nothing reads them.
    orphan_key,
    // A section whose name does not start with '/'. Every settings path is
    // absolute, so this one is never read (#647).
    relative_path,
    // A section no loaded module uses, one typo away from one that it does.
    unknown_path,
    // A key a loaded module does not use, one typo away from one that it does.
    unknown_key,
    // A section no loaded module uses. It may belong to a module that is not
    // enabled, so on its own this is not an error.
    unused_path,
    // A key no loaded module uses, under a section that one does use. Shared
    // sections (/settings/default) carry keys for modules that may not be
    // enabled, so on its own this is not an error either.
    unused_key,
  };
  enum class severity { warning, info };

  kind type;
  severity level;
  std::string source;
  std::string path;
  std::string key;
  std::string suggestion;
  std::string message;
};
typedef std::list<validation_finding> validation_result;

// Compare what a store holds against what the loaded modules registered.
//
// Paths and keys are compared without regard to case, the way the INI and
// registry stores read them. A section or key that is unknown is only a
// warning when it is a near miss of a registered one, or does not start with
// '/', or sits above every section: those are never right. (A misplaced
// section with no keys in it is info: it configures nothing.) Anything else that
// is unknown may simply belong to a module that is not loaded, and is reported
// as info - unless `complete_registry` says every installed module registered
// its settings (`nscp settings --validate --load-all`), in which case nothing
// claims it and it is a warning too.
validation_result validate_inventory(const inventory &inv, const registry_snapshot &registry, bool complete_registry);

std::size_t count_warnings(const validation_result &result);

}  // namespace settings
