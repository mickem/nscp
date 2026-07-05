// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <string>

namespace nsclient {
namespace windows_ca {

#ifdef WIN32
// Export the system "ROOT" certificate store to a PEM bundle at the given path.
// Returns the number of certificates written. On failure (open store / write
// file), `errors` is populated and the function returns 0.
unsigned int export_root_store(const std::string& path, std::list<std::string>& errors);
#endif

}  // namespace windows_ca
}  // namespace nsclient
