// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// plist::read_file on macOS: CoreFoundation parses the file, XML or binary,
// and the result is copied into plain C++ values.

#include <CoreFoundation/CoreFoundation.h>

#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "plist_value.h"

namespace plist {

namespace {

std::string to_utf8(CFStringRef s) {
  if (s == nullptr) return "";
  if (const char *fast = CFStringGetCStringPtr(s, kCFStringEncodingUTF8)) return fast;
  const CFIndex length = CFStringGetLength(s);
  const CFIndex size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  std::vector<char> buffer(static_cast<std::size_t>(size));
  if (!CFStringGetCString(s, buffer.data(), size, kCFStringEncodingUTF8)) return "";
  return buffer.data();
}

// Depth-limited: a property list is data from disk, and a pathological one
// must not recurse the collector off its stack.
value convert(CFTypeRef ref, const int depth) {
  value out;
  if (ref == nullptr || depth > 32) return out;
  const CFTypeID type = CFGetTypeID(ref);
  if (type == CFStringGetTypeID()) {
    out.kind = value::string;
    out.str = to_utf8(static_cast<CFStringRef>(ref));
  } else if (type == CFBooleanGetTypeID()) {
    out.kind = value::boolean;
    out.bool_value = CFBooleanGetValue(static_cast<CFBooleanRef>(ref));
  } else if (type == CFNumberGetTypeID()) {
    const CFNumberRef number = static_cast<CFNumberRef>(ref);
    if (CFNumberIsFloatType(number)) {
      out.kind = value::real;
      CFNumberGetValue(number, kCFNumberDoubleType, &out.real_value);
    } else {
      out.kind = value::integer;
      CFNumberGetValue(number, kCFNumberLongLongType, &out.integer_value);
    }
  } else if (type == CFDateGetTypeID()) {
    out.kind = value::date;
    const double absolute = CFDateGetAbsoluteTime(static_cast<CFDateRef>(ref));
    out.integer_value = static_cast<long long>(std::floor(absolute + kCFAbsoluteTimeIntervalSince1970));
  } else if (type == CFArrayGetTypeID()) {
    out.kind = value::array;
    const CFArrayRef array = static_cast<CFArrayRef>(ref);
    const CFIndex count = CFArrayGetCount(array);
    for (CFIndex i = 0; i < count; ++i) out.items.push_back(convert(CFArrayGetValueAtIndex(array, i), depth + 1));
  } else if (type == CFDictionaryGetTypeID()) {
    out.kind = value::dict;
    const CFDictionaryRef dict = static_cast<CFDictionaryRef>(ref);
    const CFIndex count = CFDictionaryGetCount(dict);
    std::vector<const void *> keys(static_cast<std::size_t>(count)), values(static_cast<std::size_t>(count));
    CFDictionaryGetKeysAndValues(dict, keys.data(), values.data());
    for (CFIndex i = 0; i < count; ++i) {
      if (keys[i] == nullptr || CFGetTypeID(keys[i]) != CFStringGetTypeID()) continue;
      out.members[to_utf8(static_cast<CFStringRef>(keys[i]))] = convert(values[i], depth + 1);
    }
  }
  return out;
}

}  // namespace

value read_file(const std::string &path) {
  std::ifstream in(path.c_str(), std::ios::binary);
  if (!in) return value();
  const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (bytes.empty()) return value();

  CFDataRef data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(bytes.data()), static_cast<CFIndex>(bytes.size()));
  if (data == nullptr) return value();
  CFPropertyListRef list = CFPropertyListCreateWithData(kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, nullptr);
  CFRelease(data);
  if (list == nullptr) return value();
  value out = convert(list, 0);
  CFRelease(list);
  return out;
}

}  // namespace plist
