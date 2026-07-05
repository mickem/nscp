// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// File version is a Windows PE/DLL concept; there is no equivalent on Unix, so
// the `version` filter field always evaluates to an empty string.

#include "filter.hpp"

std::string file_filter::filter_obj::get_version(parsers::where::evaluation_context) { return ""; }
