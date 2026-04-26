#include <boost/lexical_cast.hpp>
#include <cctype>
#include <locale>
#include <str/utf8.hpp>
#ifdef WIN32
#include <win/windows.hpp>
#endif
#ifndef WIN32
#include <stdio.h>
#include <string.h>
#endif
#ifdef __GNUC__
#include <iconv.h>

#include <cerrno>
#include <vector>
#endif

#ifdef __GNUC__
namespace {

// Run iconv from `fromcode` to `tocode` over `in_bytes` bytes starting at
// `in_data`. Returns the converted bytes as a basic_string<CharOut>.
//
// This wraps three things the previous open-coded sites got wrong:
//   1. The return value of iconv() was ignored; on E2BIG (output buffer
//      too small) the partial output was treated as the final result.
//      Encodings like Shift-JIS / GB2312 routinely expand on conversion,
//      and a buffer sized to the input length is not enough.
//   2. The output was constructed via std::string(ptr) which requires a
//      NUL terminator. iconv does not write one, and if it filled the
//      buffer exactly the constructor read past the allocation.
//   3. Allocations used raw new[]/delete[] with no exception safety.
//
// This helper grows the buffer on E2BIG, makes one byte of forward
// progress on EILSEQ / EINVAL (skipping the offending byte) so a single
// bad input can never make us spin, and constructs the result from a
// (pointer, length) pair using `total bytes written`.
//
// The output buffer is a std::vector<CharOut> rather than std::vector<char>
// so its storage is correctly aligned for CharOut. iconv works in bytes,
// so we reinterpret the (already CharOut-aligned) buffer to char* only at
// the call boundary. The reverse direction (treating a byte buffer as
// CharOut*) would be undefined behaviour on architectures where wchar_t
// has stricter alignment than char.
template <typename CharOut>
std::basic_string<CharOut> iconv_convert(const char *tocode, const char *fromcode, const char *in_data, std::size_t in_bytes) {
  iconv_t cd = iconv_open(tocode, fromcode);
  if (cd == reinterpret_cast<iconv_t>(-1)) {
    return std::basic_string<CharOut>();
  }

  // iconv mutates the in pointer; copy so callers' buffer is left alone.
  std::vector<char> in_copy(in_data, in_data + in_bytes);
  char *in_ptr = in_copy.empty() ? nullptr : in_copy.data();
  std::size_t in_left = in_bytes;

  // Initial guess in elements: enough room for an in_bytes-sized input
  // even when the conversion expands. Encodings that expand further
  // (e.g. Shift-JIS -> UTF-8 / wchar_t) will trigger E2BIG and we'll
  // grow below. Allocating as std::vector<CharOut> guarantees the
  // storage is correctly aligned for CharOut, so the final
  // basic_string<CharOut> construction is well-defined.
  std::size_t out_capacity_elems = std::max<std::size_t>((in_bytes / sizeof(CharOut)) + 1, 4);
  std::vector<CharOut> out(out_capacity_elems);
  std::size_t total_written_bytes = 0;

  while (true) {
    char *out_ptr = reinterpret_cast<char *>(out.data()) + total_written_bytes;
    std::size_t out_left = out.size() * sizeof(CharOut) - total_written_bytes;

    const std::size_t r = iconv(cd, &in_ptr, &in_left, &out_ptr, &out_left);
    total_written_bytes = out.size() * sizeof(CharOut) - out_left;

    if (r != static_cast<std::size_t>(-1)) {
      // Successful conversion of all remaining input.
      break;
    }
    if (errno == E2BIG) {
      // Output buffer too small: grow and continue from where we left off.
      out.resize(out.size() * 2);
      continue;
    }
    if (errno == EILSEQ || errno == EINVAL) {
      // Invalid / incomplete byte sequence in input. Skip one byte to
      // make forward progress; without this the loop would spin forever
      // because in_ptr is not advanced when iconv reports an error.
      if (in_left == 0) break;
      ++in_ptr;
      --in_left;
      continue;
    }
    // Any other error: bail out with whatever we managed to convert.
    break;
  }

  iconv_close(cd);

  // Truncate any trailing partial element (iconv writes whole code units,
  // but be defensive in case of a mid-conversion abort above).
  const std::size_t total_written_elems = total_written_bytes / sizeof(CharOut);
  return std::basic_string<CharOut>(out.data(), total_written_elems);
}

}  // namespace
#endif  // __GNUC__

std::wstring utf8::to_unicode(std::string const &str) {
#ifdef WIN32
  const int len = static_cast<int>(str.length());
  const int nChars = MultiByteToWideChar(CP_ACP, 0, str.c_str(), len, nullptr, 0);
  if (nChars == 0) return L"";
  auto *buffer = new wchar_t[nChars + 1];
  MultiByteToWideChar(CP_ACP, 0, str.c_str(), len, buffer, nChars);
  buffer[nChars] = 0;
  std::wstring buf(buffer, nChars);
  delete[] buffer;
  return buf;
#else
  return iconv_convert<wchar_t>("WCHAR_T", "", str.data(), str.size());
#endif
}

#ifdef WIN32
UINT parse_encoding(const std::string &str) {
  if (str.empty()) return CP_ACP;
  if (str == "system") return CP_ACP;
  if (str == "utf8" || str == "utf-8" || str == "UTF-8" || str == "UTF8") return CP_UTF8;
  if (str == "utf7" || str == "utf-7" || str == "UTF-7" || str == "UTF7") return CP_UTF7;
  if (str == "oem") return CP_OEMCP;
  return CP_ACP;
}
#endif
std::wstring utf8::from_encoding(const std::string &str, const std::string &encoding) {
#ifdef WIN32
  const UINT uiEncoding = parse_encoding(encoding);
  const int len = static_cast<int>(str.length());
  const int nChars = MultiByteToWideChar(uiEncoding, 0, str.c_str(), len, nullptr, 0);
  if (nChars == 0) return L"";
  auto *buffer = new wchar_t[nChars + 1];
  MultiByteToWideChar(uiEncoding, 0, str.c_str(), len, buffer, nChars);
  buffer[nChars] = 0;
  std::wstring buf(buffer, nChars);
  delete[] buffer;
  return buf;
#else
  return iconv_convert<wchar_t>("WCHAR_T", encoding.c_str(), str.data(), str.size());
#endif
}

std::string utf8::to_encoding(std::wstring const &str, const std::string &encoding) {
#ifdef WIN32
  const UINT uiEncoding = parse_encoding(encoding);
  // figure out how many narrow characters we are going to get
  const int nChars = WideCharToMultiByte(uiEncoding, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0, nullptr, nullptr);
  if (nChars == 0) return "";

  // convert the wide string to a narrow string
  // nb: slightly naughty to write directly into the string like this
  std::string buf;
  buf.resize(nChars);
  WideCharToMultiByte(uiEncoding, 0, str.c_str(), static_cast<int>(str.length()), const_cast<char *>(buf.c_str()), nChars, nullptr, nullptr);
  return buf;
#else
  return iconv_convert<char>(encoding.c_str(), "WCHAR_T", reinterpret_cast<const char *>(str.data()), str.size() * sizeof(wchar_t));
#endif
}

std::string utf8::to_system(std::wstring const &str) {
#ifdef WIN32
  // figure out how many narrow characters we are going to get
  const int nChars = WideCharToMultiByte(CP_ACP, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0, nullptr, nullptr);
  if (nChars == 0) return "";

  // convert the wide string to a narrow string
  // nb: slightly naughty to write directly into the string like this
  std::string buf;
  buf.resize(nChars);
  WideCharToMultiByte(CP_ACP, 0, str.c_str(), static_cast<int>(str.length()), const_cast<char *>(buf.c_str()), nChars, nullptr, nullptr);
  return buf;
#else
  return iconv_convert<char>("UTF-8", "WCHAR_T", reinterpret_cast<const char *>(str.data()), str.size() * sizeof(wchar_t));
#endif
}

std::string utf8::wstring_to_string(std::wstring const &str) {
#ifdef WIN32
  // figure out how many narrow characters we are going to get
  const int nChars = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0, nullptr, nullptr);
  if (nChars == 0) return "";

  // convert the wide string to a narrow string
  // nb: slightly naughty to write directly into the string like this
  std::string buf;
  buf.resize(nChars);
  WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), const_cast<char *>(buf.c_str()), nChars, nullptr, nullptr);
  return buf;
#else
  return iconv_convert<char>("UTF-8", "WCHAR_T", reinterpret_cast<const char *>(str.data()), str.size() * sizeof(wchar_t));
#endif
}

std::wstring utf8::string_to_wstring(std::string const &str) {
#ifdef WIN32
  const int len = static_cast<int>(str.length());
  const int nChars = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), len, nullptr, 0);
  if (nChars == 0) return L"";
  auto *buffer = new wchar_t[nChars + 1];
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), len, buffer, nChars);
  buffer[nChars] = 0;
  std::wstring buf(buffer, nChars);
  delete[] buffer;
  return buf;
#else
  return iconv_convert<wchar_t>("WCHAR_T", "UTF-8", str.data(), str.size());
#endif
}
