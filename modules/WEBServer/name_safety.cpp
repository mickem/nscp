#include "name_safety.hpp"

#include <cctype>

namespace name_safety {
namespace {
constexpr std::size_t kMaxSegmentLen = 128;
constexpr std::size_t kMaxNameLen = 256;

bool is_safe_segment_char(char c) {
  const auto u = static_cast<unsigned char>(c);
  if (std::isalnum(u)) return true;
  return c == '.' || c == '_' || c == '-';
}

bool is_safe_segment(const std::string& seg) {
  if (seg.empty() || seg.size() > kMaxSegmentLen) return false;
  // "." and ".." are path-traversal primitives even when each character
  // would otherwise be allowed.
  if (seg == "." || seg == "..") return false;
  for (char c : seg) {
    if (!is_safe_segment_char(c)) return false;
  }
  return true;
}
}  // namespace

bool is_safe_module_name(const std::string& name) {
  // Single segment: no separators of any kind.
  if (name.empty() || name.size() > kMaxSegmentLen) return false;
  for (char c : name) {
    if (c == '/' || c == '\\') return false;
  }
  return is_safe_segment(name);
}

bool is_safe_script_name(const std::string& name) {
  if (name.empty() || name.size() > kMaxNameLen) return false;
  // No leading separator (would resolve to filesystem root).
  if (name.front() == '/' || name.front() == '\\') return false;
  // No trailing separator (means a missing filename - e.g. "a/" - which the
  // segment loop below would otherwise miss because it exits before
  // validating the empty segment after the separator).
  if (name.back() == '/' || name.back() == '\\') return false;
  // No drive letter on Windows (e.g. "C:foo").
  if (name.size() >= 2 && name[1] == ':') return false;
  // No NULs anywhere.
  for (char c : name) {
    if (c == '\0') return false;
  }
  // Walk segments split on either separator. Each segment must be a safe
  // segment and not `.` / `..` (handled inside `is_safe_segment`). Empty
  // interior segments (e.g. "a//b") fail is_safe_segment.
  std::size_t i = 0;
  while (i < name.size()) {
    std::size_t j = i;
    while (j < name.size() && name[j] != '/' && name[j] != '\\') {
      ++j;
    }
    const std::string seg = name.substr(i, j - i);
    if (!is_safe_segment(seg)) return false;
    if (j == name.size()) break;
    i = j + 1;
  }
  return true;
}

}  // namespace name_safety
