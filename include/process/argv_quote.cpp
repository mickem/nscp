#include "argv_quote.hpp"

#include <str/utf8.hpp>

namespace process {

std::wstring quote_argv_w(const std::wstring& arg) {
  // Determine whether quoting is needed at all. An empty arg must be quoted as
  // "" so it is not lost. Space/tab/newline/quote anywhere force quoting.
  if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
    return arg;
  }

  std::wstring out;
  out.reserve(arg.size() + 4);
  out.push_back(L'"');
  for (std::size_t i = 0; i < arg.size();) {
    std::size_t backslashes = 0;
    while (i < arg.size() && arg[i] == L'\\') {
      ++backslashes;
      ++i;
    }
    if (i == arg.size()) {
      // Trailing backslashes before the closing quote: double them so the
      // closing quote is not turned into an escaped quote.
      out.append(backslashes * 2, L'\\');
      break;
    }
    if (arg[i] == L'"') {
      // Backslashes before a literal quote must be doubled, then escape the
      // quote itself with an extra backslash.
      out.append(backslashes * 2, L'\\');
      out.push_back(L'\\');
      out.push_back(L'"');
      ++i;
    } else {
      // Backslashes not adjacent to a quote pass through verbatim.
      out.append(backslashes, L'\\');
      out.push_back(arg[i]);
      ++i;
    }
  }
  out.push_back(L'"');
  return out;
}

std::wstring build_command_line_w(const std::vector<std::string>& argv) {
  std::wstring out;
  for (std::size_t i = 0; i < argv.size(); ++i) {
    if (i != 0) out.push_back(L' ');
    out.append(quote_argv_w(utf8::cvt<std::wstring>(argv[i])));
  }
  return out;
}

}  // namespace process
