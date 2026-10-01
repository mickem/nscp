// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string/trim.hpp>
#include <boost/system/error_code.hpp>
#include <error/error.hpp>
#include <str/utf8.hpp>
#include <string>

namespace check_ad {

// FormatMessage terminates its text with CRLF, which error::lookup::last_error
// passes through verbatim. A check result is a single Nagios line (NRPE/NSCA
// keep the first line and drop or relegate the rest), so the newline must never
// reach a message - least of all mid-sentence, where it splits the text in two.
inline std::string win32_error(unsigned long code) { return boost::algorithm::trim_copy(error::lookup::last_error(code)); }

// error_code::message() is in the ANSI code page on Windows (boost renders a
// system-category code with FormatMessage and narrows it with CP_ACP), so on a
// localized install it carries bytes that are not UTF-8 - and protobuf refuses
// to serialize a result message that does, losing the whole check result.
inline std::string error_text(const boost::system::error_code &ec) { return utf8::utf8_from_native(ec.message()); }

}  // namespace check_ad
