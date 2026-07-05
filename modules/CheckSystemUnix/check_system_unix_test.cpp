// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/nscapi_helper_singleton.hpp>

// Provide the NSCAPI singleton used by modern_filter.cpp during test linking.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();
