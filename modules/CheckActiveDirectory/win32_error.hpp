// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string/trim.hpp>
#include <error/error.hpp>
#include <string>

namespace check_ad {

// FormatMessage terminates its text with CRLF, which error::lookup::last_error
// passes through verbatim. A check result is a single Nagios line (NRPE/NSCA
// keep the first line and drop or relegate the rest), so the newline must never
// reach a message - least of all mid-sentence, where it splits the text in two.
inline std::string win32_error(unsigned long code) { return boost::algorithm::trim_copy(error::lookup::last_error(code)); }

}  // namespace check_ad
