// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Darwin reader for the diagnostics process tables. There is none yet (it
// would be libproc's proc_pidinfo, as process_source_darwin.cpp reads for
// check_process): the sections are reported with this error rather than left
// out, so a reader can tell "not produced here" from "switched off".

#include <stdexcept>

#include "process_top.h"

namespace process_top {

table read_processes() { throw std::runtime_error("the process tables are not supported on macOS yet"); }

}  // namespace process_top
