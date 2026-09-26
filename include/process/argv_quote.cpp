// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "argv_quote.hpp"

#include <boost/filesystem/path.hpp>
#include <nscp/path_rooting.hpp>
#include <str/utf8.hpp>

#include <cstddef>

namespace process {

std::string resolve_application_path(const std::string& root_path, const std::string& argv0) {
  if (argv0.empty() || root_path.empty()) return argv0;
  const boost::filesystem::path app(argv0);
  // names_a_root() rather than is_absolute(): on Windows `C:x.exe` is
  // drive-relative and `\x.exe` is root-relative, and is_absolute() is false
  // for both, yet joining the installation directory onto either produces
  // nonsense. Same predicate the settings layer roots attachment targets with.
  if (nscp::paths::names_a_root(app)) return argv0;
  // No directory component at all: leave the system's executable search to do
  // its job (see the header).
  if (!app.has_parent_path()) return argv0;
  return (boost::filesystem::path(root_path) / app).string();
}

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

bool is_batch_target(const std::string& program) {
  std::string name = program;
  // A template token may still carry the quotes an operator wrote around a
  // path with spaces.
  while (!name.empty() && (name.front() == '"' || name.front() == '\'')) name.erase(name.begin());
  while (!name.empty() && (name.back() == '"' || name.back() == '\'')) name.pop_back();
  const auto ends_with = [&name](const std::string& suffix) {
    if (name.size() < suffix.size()) return false;
    for (std::size_t i = 0; i < suffix.size(); ++i) {
      const char c = name[name.size() - suffix.size() + i];
      const char lower = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
      if (lower != suffix[i]) return false;
    }
    return true;
  };
  return ends_with(".bat") || ends_with(".cmd");
}

}  // namespace process
