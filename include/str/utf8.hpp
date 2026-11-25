// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <cstddef>
#include <string>

namespace utf8 {
/** Converts a std::wstring into a std::string with UTF-8 encoding. */
template <typename StringT>
StringT cvt(std::wstring const& string);

/** Converts a std::String with UTF-8 encoding into a std::wstring.	*/
template <typename StringT>
StringT cvt(std::string const& string);

/** Nop specialization for std::string. */
template <>
inline std::string cvt(std::string const& string) {
  return string;
}

/** Nop specialization for std::wstring. */
template <>
inline std::wstring cvt(std::wstring const& rc_string) {
  return rc_string;
}

std::wstring to_unicode(std::string const& str);
std::wstring from_encoding(const std::string& str, const std::string& encoding);
std::string to_encoding(std::wstring const& str, const std::string& encoding);

std::string to_system(std::wstring const& str);

std::string wstring_to_string(std::wstring const& str);
std::wstring string_to_wstring(std::string const& str);

template <>
inline std::string cvt(std::wstring const& str) {
  return wstring_to_string(str);
}

template <>
inline std::wstring cvt(std::string const& str) {
  return string_to_wstring(str);
}

inline std::string utf8_from_native(std::string const& str) { return cvt<std::string>(to_unicode(str)); }

/** True when the bytes are well-formed UTF-8 (no overlong forms, surrogates or code points past U+10FFFF). */
bool is_valid(std::string const& str);

/**
 * Returns the text as well-formed UTF-8, unchanged when it already is.
 *
 * A last line of defence for text on its way into a protobuf string field,
 * which refuses to serialize anything else and takes the whole message down
 * with it. Unlike utf8_from_native() it is safe on text that is already UTF-8,
 * so it can sit on a path where most text is: only the runs of bytes that are
 * not valid UTF-8 are touched. On Windows those are decoded from the ANSI code
 * page - where they almost always come from (error_code::message(),
 * system_error::what()) - elsewhere each byte becomes U+FFFD.
 *
 * It is not a substitute for converting OS text where it is produced: a run
 * that happens to be valid UTF-8 as well is kept as UTF-8.
 */
std::string make_valid(std::string const& str);
inline std::string to_encoding(std::string const& str, const std::string& encoding) { return to_encoding(cvt<std::wstring>(str), encoding); }

// Byte-level decoders for text read from a file or the wire, where the
// encoding is a property of the data rather than of the platform. Unlike
// from_encoding() they behave identically everywhere: they do not go through
// wchar_t (UTF-16 on Windows, UTF-32 elsewhere), iconv or the Windows code
// pages, which on Windows do not take UTF-16 or Latin-1 at all.

/** The byte-order mark a buffer starts with, if any. */
enum class byte_order_mark { none, utf8, utf16le, utf16be };
byte_order_mark detect_bom(std::string const& bytes);
/** Number of bytes the mark occupies (0, 2 or 3). */
std::size_t bom_size(byte_order_mark bom);

/**
 * Decodes UTF-16 bytes into UTF-8. A byte-order mark is not consumed (strip it
 * first, see detect_bom()). Surrogate pairs are combined and an unpaired
 * surrogate becomes U+FFFD. Returns false, leaving `out` empty, when the byte
 * count is odd.
 */
bool from_utf16(std::string const& bytes, bool little_endian, std::string& out);

/** Decodes ISO-8859-1 (Latin-1) bytes into UTF-8: every byte is the code point of the same value. */
std::string from_latin1(std::string const& bytes);
}  // namespace utf8
/*
namespace boost {
        template<>
        inline std::wstring lexical_cast<std::wstring, std::string>(const std::string& arg) {
                return utf8::cvt<std::wstring>(arg);
        }

        template<>
        inline std::string lexical_cast<std::string, std::wstring>(const std::wstring& arg) {
                return utf8::cvt<std::string>(arg);
        }
}
*/