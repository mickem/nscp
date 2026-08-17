// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <simpleini/simpleini.h>

#include <str/utf8.hpp>
#include <string>

#include "path_defaults.hpp"

// Reading `[layout] mode` out of boot.ini.
//
// The service learns the layout while booting the settings subsystem, which
// parses boot.ini anyway. The standalone clients (check_nrpe, check_nscp) have
// no settings subsystem at all, but they still have to agree about where
// ${shared-path} is - a client that disagrees looks for certificates in a
// folder the service never wrote to. So they read the one key themselves,
// which costs a header-only ini parse of a file next to the executable.
namespace nscp {
namespace paths {

// Read the layout from an already-loaded boot.ini.
inline layout layout_from_boot_ini(CSimpleIni &boot_conf, std::string *raw_value = nullptr) {
  const std::string mode = utf8::cvt<std::string>(boot_conf.GetValue(L"layout", L"mode", L""));
  if (raw_value != nullptr) *raw_value = mode;
  return parse_layout(mode);
}

// Read the layout from boot.ini at `path`. A missing or unreadable file is
// `legacy`, which is also what every installation that predates the setting
// gets: the layout only changes when an operator asks for it.
inline layout layout_from_boot_ini_file(const std::string &path, std::string *raw_value = nullptr) {
  if (raw_value != nullptr) raw_value->clear();
  CSimpleIni boot_conf;
  if (boot_conf.LoadFile(path.c_str()) < 0) return layout::legacy;
  return layout_from_boot_ini(boot_conf, raw_value);
}

// Read one key out of boot.ini's `[paths]` section, or an empty string when it
// is not set.
//
// The service applies every key in that section as a path override before it
// opens the main settings store (settings_manager_impl), so an operator can put
// ${shared-path} - and therefore the configuration, the certificates and the
// fleet identity - anywhere they like. Anything that has to agree with the
// running service about where those live has to read it too; an installer that
// resolves ${shared-path} from the layout alone will migrate into %ProgramData%
// while the service looks somewhere else entirely and finds nothing.
inline std::string path_override_from_boot_ini(CSimpleIni &boot_conf, const std::string &key) {
  return utf8::cvt<std::string>(boot_conf.GetValue(L"paths", utf8::cvt<std::wstring>(key).c_str(), L""));
}

// The same, reading boot.ini at `path`. A missing or unreadable file has no
// overrides, which is the same answer as a file without a [paths] section.
inline std::string path_override_from_boot_ini_file(const std::string &path, const std::string &key) {
  CSimpleIni boot_conf;
  if (boot_conf.LoadFile(path.c_str()) < 0) return std::string();
  return path_override_from_boot_ini(boot_conf, key);
}

// Decide the layout an installation should end up on, given the one it is on
// and what was asked for (an MSI property, a command line). Kept apart from the
// installer so the rules can be tested; the installer supplies `current` from
// boot.ini.
//
// Nothing asked for, or something we do not recognise, keeps the current
// layout: an upgrade must not move an installation that did not ask to move,
// nor move a modern one back because the property was not repeated.
inline layout resolve_requested_layout(const layout current, const std::string &requested) {
  if (requested.empty() || !is_known_layout(requested)) return current;
  const layout wanted = parse_layout(requested);
  // Going back to legacy is not something that can be honoured. It would mean
  // moving the configuration, the fleet identity and the certificates back out
  // of the protected folder, and there is no migration in that direction:
  // recording the property alone would leave the agent looking in the install
  // folder for files that are still in %ProgramData%. That is a broken
  // installation, not a reverted one, so keep what the host has.
  if (wanted == layout::legacy && current == layout::modern) return current;
  return wanted;
}

}  // namespace paths
}  // namespace nscp
