#pragma once

#include <string>

// Helpers for rejecting attacker-controlled names that would otherwise reach
// the filesystem or the registry. Used by the scripts and modules controllers.
//
// The threat model: a request URL like
//   PUT /api/v2/scripts/ext/../../../Windows/System32/evil
//   POST /api/v2/modules/..%2F..%2Fwindows
// must not write outside the intended directory or register a command name
// that can drive a downstream module into traversing out.
//
// `is_safe_module_name` is strict (single segment, alphanum + `._-`). Module
// names map 1-to-1 to a registry id and a single file in module-path.
//
// `is_safe_script_name` permits forward slashes between segments because some
// installers organise scripts in subfolders, but each segment must itself be
// safe and `..` / `.` / empty / drive-letter / leading-separator are rejected.
namespace name_safety {

bool is_safe_module_name(const std::string& name);
bool is_safe_script_name(const std::string& name);

}  // namespace name_safety
