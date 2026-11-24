// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// ASCII character classes that do not depend on the C locale. The <cctype>
// functions consult the locale, take an int that must be representable as
// unsigned char (a plain char >= 0x80 is undefined behaviour) and accept more
// than ASCII in some locales; parsers of version strings, thresholds and
// file contents want exactly the ASCII ranges.

namespace str {
namespace ascii {

inline bool is_digit(const char c) { return c >= '0' && c <= '9'; }
inline bool is_alpha(const char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool is_alnum(const char c) { return is_digit(c) || is_alpha(c); }
// Printable, excluding space: 0x21-0x7E.
inline bool is_graph(const char c) { return c > 0x20 && c < 0x7f; }

}  // namespace ascii
}  // namespace str
