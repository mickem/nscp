#include <utf8.hpp>
#include <locale>
#include <cctype>

#include <boost/lexical_cast.hpp>
#ifdef WIN32
#include <win/windows.hpp>
#endif
#ifndef WIN32
#include <stdio.h>
#include <string.h>
#endif
#ifdef __GNUC__
#include <iconv.h>
#include <errno.h>
#endif

std::wstring utf8::to_unicode(std::string const &str) {
#ifdef WIN32
  int len = static_cast<int>(str.length());
  int nChars = MultiByteToWideChar(CP_ACP, 0, str.c_str(), len, NULL, 0);
  if (nChars == 0) return L"";
  wchar_t *buffer = new wchar_t[nChars + 1];
  if (buffer == NULL) return L"";
  MultiByteToWideChar(CP_ACP, 0, str.c_str(), len, buffer, nChars);
  buffer[nChars] = 0;
  std::wstring buf(buffer, nChars);
  delete[] buffer;
  return buf;
#else
  size_t utf8Length = str.length();
  size_t outbytesLeft = utf8Length * sizeof(wchar_t);

  // Copy the instring
  char *inString = new char[str.length() + 1];
  strcpy(inString, str.c_str());

  // Create buffer for output
  char *outString = (char *)new wchar_t[utf8Length + 1];
  memset(outString, 0, sizeof(wchar_t) * (utf8Length + 1));

  char *inPointer = inString;
  char *outPointer = outString;

  iconv_t convDesc = iconv_open("WCHAR_T", "");
  iconv(convDesc, &inPointer, &utf8Length, &outPointer, &outbytesLeft);
  iconv_close(convDesc);

  std::wstring retval((wchar_t *)outString);

  // Cleanup
  delete[] inString;
  delete[] outString;

  return retval;
#endif
}

#ifdef WIN32
UINT parse_encoding(const std::string &str) {
  if (str.empty()) return CP_ACP;
  if (str == "system") return CP_ACP;
  if (str == "utf8" || str == "utf-8") return CP_UTF8;
  if (str == "utf7" || str == "utf-7") return CP_UTF7;
  if (str == "oem") return CP_OEMCP;
  return CP_ACP;
}
#endif
std::wstring utf8::from_encoding(const std::string &str, const std::string &encoding) {
#ifdef WIN32
  UINT uiEncoding = parse_encoding(encoding);
  int len = static_cast<int>(str.length());
  int nChars = MultiByteToWideChar(uiEncoding, 0, str.c_str(), len, NULL, 0);
  if (nChars == 0) return L"";
  wchar_t *buffer = new wchar_t[nChars + 1];
  if (buffer == NULL) return L"";
  MultiByteToWideChar(uiEncoding, 0, str.c_str(), len, buffer, nChars);
  buffer[nChars] = 0;
  std::wstring buf(buffer, nChars);
  delete[] buffer;
  return buf;
#else
  size_t utf8Length = str.length();
  size_t outbytesLeft = utf8Length * sizeof(wchar_t);

  // Copy the instring
  char *inString = new char[str.length() + 1];
  strcpy(inString, str.c_str());

  // Create buffer for output
  char *outString = (char *)new wchar_t[utf8Length + 1];
  memset(outString, 0, sizeof(wchar_t) * (utf8Length + 1));

  char *inPointer = inString;
  char *outPointer = outString;

  iconv_t convDesc = iconv_open("WCHAR_T", encoding.c_str());
  iconv(convDesc, &inPointer, &utf8Length, &outPointer, &outbytesLeft);
  iconv_close(convDesc);

  std::wstring retval((wchar_t *)outString);

  // Cleanup
  delete[] inString;
  delete[] outString;

  return retval;
#endif
}

std::string utf8::to_encoding(std::wstring const &str, const std::string &encoding) {
#ifdef WIN32
  UINT uiEncoding = parse_encoding(encoding);
  // figure out how many narrow characters we are going to get
  int nChars = WideCharToMultiByte(uiEncoding, 0, str.c_str(), static_cast<int>(str.length()), NULL, 0, NULL, NULL);
  if (nChars == 0) return "";

  // convert the wide string to a narrow string
  // nb: slightly naughty to write directly into the string like this
  std::string buf;
  buf.resize(nChars);
  WideCharToMultiByte(uiEncoding, 0, str.c_str(), static_cast<int>(str.length()), const_cast<char *>(buf.c_str()), nChars, NULL, NULL);
  return buf;
#else
  size_t wideSize = sizeof(wchar_t) * str.length();
  size_t outbytesLeft = wideSize + sizeof(char);  // We cannot know how many wide character there is yet

  // Copy the instring
  char *inString = (char *)new wchar_t[str.length() + 1];
  memcpy(inString, str.c_str(), wideSize + sizeof(wchar_t));

  // Create buffer for output
  char *outString = new char[outbytesLeft];
  memset(outString, 0, sizeof(char) * (outbytesLeft));

  char *inPointer = inString;
  char *outPointer = outString;

  iconv_t convDesc = iconv_open(encoding.c_str(), "WCHAR_T");
  iconv(convDesc, &inPointer, &wideSize, &outPointer, &outbytesLeft);
  iconv_close(convDesc);

  std::string retval(outString);

  // Cleanup
  delete[] inString;
  delete[] outString;

  return retval;
#endif
}

std::string utf8::to_system(std::wstring const &str) {
#ifdef WIN32
  // figure out how many narrow characters we are going to get
  int nChars = WideCharToMultiByte(CP_ACP, 0, str.c_str(), static_cast<int>(str.length()), NULL, 0, NULL, NULL);
  if (nChars == 0) return "";

  // convert the wide string to a narrow string
  // nb: slightly naughty to write directly into the string like this
  std::string buf;
  buf.resize(nChars);
  WideCharToMultiByte(CP_ACP, 0, str.c_str(), static_cast<int>(str.length()), const_cast<char *>(buf.c_str()), nChars, NULL, NULL);
  return buf;
#else
  size_t wideSize = sizeof(wchar_t) * str.length();
  size_t outbytesLeft = wideSize + sizeof(char);  // We cannot know how many wide character there is yet

  // Copy the instring
  char *inString = (char *)new wchar_t[str.length() + 1];
  memcpy(inString, str.c_str(), wideSize + sizeof(wchar_t));

  // Create buffer for output
  char *outString = new char[outbytesLeft];
  memset(outString, 0, sizeof(char) * (outbytesLeft));

  char *inPointer = inString;
  char *outPointer = outString;

  iconv_t convDesc = iconv_open("UTF-8", "WCHAR_T");
  iconv(convDesc, &inPointer, &wideSize, &outPointer, &outbytesLeft);
  iconv_close(convDesc);

  std::string retval(outString);

  // Cleanup
  delete[] inString;
  delete[] outString;

  return retval;
#endif
}

std::string utf8::wstring_to_string(std::wstring const &str) {
#ifdef WIN32
  // figure out how many narrow characters we are going to get
  int nChars = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), NULL, 0, NULL, NULL);
  if (nChars == 0) return "";

  // convert the wide string to a narrow string
  // nb: slightly naughty to write directly into the string like this
  std::string buf;
  buf.resize(nChars);
  WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), const_cast<char *>(buf.c_str()), nChars, NULL, NULL);
  return buf;
#else
  size_t wideSize = sizeof(wchar_t) * str.length();
  size_t outbytesLeft = wideSize + sizeof(char);  // We cannot know how many wide character there is yet

  // Copy the instring
  char *inString = (char *)new wchar_t[str.length() + 1];
  memcpy(inString, str.c_str(), wideSize + sizeof(wchar_t));

  // Create buffer for output
  char *outString = new char[outbytesLeft];
  memset(outString, 0, sizeof(char) * (outbytesLeft));

  char *inPointer = inString;
  char *outPointer = outString;

  iconv_t convDesc = iconv_open("UTF-8", "WCHAR_T");
  iconv(convDesc, &inPointer, &wideSize, &outPointer, &outbytesLeft);
  iconv_close(convDesc);

  std::string retval(outString);

  // Cleanup
  delete[] inString;
  delete[] outString;

  return retval;
#endif
}

std::wstring utf8::string_to_wstring(std::string const &str) {
#ifdef WIN32
  int len = static_cast<int>(str.length());
  int nChars = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), len, NULL, 0);
  if (nChars == 0) return L"";
  wchar_t *buffer = new wchar_t[nChars + 1];
  if (buffer == NULL) return L"";
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), len, buffer, nChars);
  buffer[nChars] = 0;
  std::wstring buf(buffer, nChars);
  delete[] buffer;
  return buf;
#else
  size_t utf8Length = str.length();
  size_t outbytesLeft = utf8Length * sizeof(wchar_t);

  // Copy the instring
  char *inString = new char[str.length() + 1];
  strcpy(inString, str.c_str());

  // Create buffer for output
  char *outString = (char *)new wchar_t[utf8Length + 1];
  memset(outString, 0, sizeof(wchar_t) * (utf8Length + 1));

  char *inPointer = inString;
  char *outPointer = outString;

  iconv_t convDesc = iconv_open("WCHAR_T", "UTF-8");
  iconv(convDesc, &inPointer, &utf8Length, &outPointer, &outbytesLeft);
  iconv_close(convDesc);

  std::wstring retval((wchar_t *)outString);

  // Cleanup
  delete[] inString;
  delete[] outString;

  return retval;
#endif
}