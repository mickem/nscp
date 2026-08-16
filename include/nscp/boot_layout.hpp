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

}  // namespace paths
}  // namespace nscp
