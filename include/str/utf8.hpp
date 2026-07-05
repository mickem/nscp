// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
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
inline std::string to_encoding(std::string const& str, const std::string& encoding) { return to_encoding(cvt<std::wstring>(str), encoding); }
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