// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/nscapi_helper_singleton.hpp>

// Provide the NSCAPI singleton so modern_filter.cpp can link.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();
