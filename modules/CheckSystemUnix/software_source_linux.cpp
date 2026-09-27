// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The macOS software sources, on Linux: there are none. detect_manager()
// never picks the macOS manager here, so these only answer if called anyway,
// and they answer "nothing read" rather than an empty inventory.

#include "check_installed_software.h"
#include "check_os_updates.h"
#include "plist_value.h"

installed_software::fetch_result installed_software::fetch_macos_inventory() { return fetch_result(); }

plist::value os_updates::read_software_update_cache() { return plist::value(); }

plist::value plist::read_file(const std::string &) { return value(); }
