/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifdef WIN32

#include <win/windows.hpp>

#include <ctime>
#include <list>
#include <string>
#include <vector>
#include <algorithm>

#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <error/error.hpp>

namespace win_registry {

// ── Exception ──────────────────────────────────────────────────────────────

struct registry_exception {
  std::string reason_;
  explicit registry_exception(const std::string &reason) : reason_(reason) {}
  const std::string &reason() const { return reason_; }
};

// ── FILETIME helpers ────────────────────────────────────────────────────────

inline unsigned long long filetime_to_ull(const FILETIME &ft) {
  return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

inline long long filetime_to_epoch(unsigned long long ft_ull) {
  if (ft_ull == 0) return 0;
  return static_cast<long long>(str::format::filetime_to_time(ft_ull));
}

inline std::string filetime_to_string(unsigned long long ft_ull) {
  return str::format::format_filetime(ft_ull);
}

// ── Type name conversion ────────────────────────────────────────────────────

inline std::string type_to_string(const DWORD type) {
  switch (type) {
    case REG_NONE:          return "REG_NONE";
    case REG_SZ:            return "REG_SZ";
    case REG_EXPAND_SZ:     return "REG_EXPAND_SZ";
    case REG_BINARY:        return "REG_BINARY";
    case REG_DWORD:         return "REG_DWORD";
    case REG_DWORD_BIG_ENDIAN: return "REG_DWORD_BIG_ENDIAN";
    case REG_LINK:          return "REG_LINK";
    case REG_MULTI_SZ:      return "REG_MULTI_SZ";
    case REG_QWORD:         return "REG_QWORD";
    default:                return "REG_UNKNOWN(" + str::xtos(type) + ")";
  }
}

inline long long parse_type(const std::string &s) {
  if (s == "REG_NONE")          return REG_NONE;
  if (s == "REG_SZ")            return REG_SZ;
  if (s == "REG_EXPAND_SZ")     return REG_EXPAND_SZ;
  if (s == "REG_BINARY")        return REG_BINARY;
  if (s == "REG_DWORD")         return REG_DWORD;
  if (s == "REG_DWORD_BIG_ENDIAN") return REG_DWORD_BIG_ENDIAN;
  if (s == "REG_LINK")          return REG_LINK;
  if (s == "REG_MULTI_SZ")      return REG_MULTI_SZ;
  if (s == "REG_QWORD")         return REG_QWORD;
  try {
    return static_cast<long long>(str::stox<unsigned long>(s));
  } catch (...) {
    throw nsclient::nsclient_exception("Unknown registry type: " + s);
  }
}

// ── Hive helpers ────────────────────────────────────────────────────────────

inline HKEY parse_hive(const std::string &hive) {
  if (hive == "HKLM" || hive == "HKEY_LOCAL_MACHINE")   return HKEY_LOCAL_MACHINE;
  if (hive == "HKCU" || hive == "HKEY_CURRENT_USER")    return HKEY_CURRENT_USER;
  if (hive == "HKCR" || hive == "HKEY_CLASSES_ROOT")    return HKEY_CLASSES_ROOT;
  if (hive == "HKU"  || hive == "HKEY_USERS")           return HKEY_USERS;
  if (hive == "HKCC" || hive == "HKEY_CURRENT_CONFIG")  return HKEY_CURRENT_CONFIG;
  throw nsclient::nsclient_exception("Unknown registry hive: " + hive);
}

inline std::string hive_to_string(const HKEY hive) {
  if (hive == HKEY_LOCAL_MACHINE)   return "HKLM";
  if (hive == HKEY_CURRENT_USER)    return "HKCU";
  if (hive == HKEY_CLASSES_ROOT)    return "HKCR";
  if (hive == HKEY_USERS)           return "HKU";
  if (hive == HKEY_CURRENT_CONFIG)  return "HKCC";
  return "HKUNKNOWN";
}

// ── WOW64 view ──────────────────────────────────────────────────────────────

inline DWORD parse_view(const std::string &view) {
  if (view == "32") return KEY_WOW64_32KEY;
  if (view == "64") return KEY_WOW64_64KEY;
  return 0;
}

// ── Key path parsing ────────────────────────────────────────────────────────

struct key_path_parts {
  std::string hive_str;
  HKEY hive{};
  std::string subpath;
};

inline key_path_parts parse_key_path(const std::string &full_path) {
  key_path_parts parts;
  const auto sep = full_path.find('\\');
  parts.hive_str = (sep == std::string::npos) ? full_path : full_path.substr(0, sep);
  parts.hive = parse_hive(parts.hive_str);
  parts.subpath = (sep == std::string::npos) ? std::string() : full_path.substr(sep + 1);
  return parts;
}

// ── RAII HKEY wrapper ───────────────────────────────────────────────────────

struct raii_hkey {
  HKEY hKey;
  explicit raii_hkey(HKEY h = nullptr) : hKey(h) {}
  ~raii_hkey() {
    if (hKey != nullptr &&
        hKey != HKEY_LOCAL_MACHINE &&
        hKey != HKEY_CURRENT_USER &&
        hKey != HKEY_CLASSES_ROOT &&
        hKey != HKEY_USERS &&
        hKey != HKEY_CURRENT_CONFIG) {
      RegCloseKey(hKey);
    }
  }
  HKEY get() const { return hKey; }
  bool valid() const { return hKey != nullptr; }

  raii_hkey(const raii_hkey &) = delete;
  raii_hkey &operator=(const raii_hkey &) = delete;
};

// ── Data structs ─────────────────────────────────────────────────────────────

struct key_info {
  std::string path;        // Full path including hive prefix: HKLM\Software\...
  std::string name;        // Leaf key name
  std::string parent;      // Parent path (full, including hive)
  std::string hive;        // Hive abbreviation (HKLM, HKCU, ...)
  long long depth;         // Distance from the starting key (0 = the key itself)
  bool exists;             // Was the key successfully opened?
  long long value_count;   // Number of values in this key
  long long subkey_count;  // Number of immediate sub-keys
  unsigned long long written_ft;  // Last-write time as a FILETIME ULL (100-ns intervals from 1601-01-01)
  std::string class_name;  // Key class string (rarely set)

  key_info()
      : depth(0), exists(false), value_count(0), subkey_count(0), written_ft(0) {}

  std::string get_path()       const { return path; }
  std::string get_name()       const { return name; }
  std::string get_parent()     const { return parent; }
  std::string get_hive()       const { return hive; }
  long long   get_depth()      const { return depth; }
  long long   get_exists()     const { return exists ? 1 : 0; }
  long long   get_value_count() const { return value_count; }
  long long   get_subkey_count() const { return subkey_count; }
  long long   get_written()    const { return filetime_to_epoch(written_ft); }
  long long   get_age()        const {
    const long long w = get_written();
    return (w == 0) ? 0 : static_cast<long long>(time(nullptr)) - w;
  }
  std::string get_written_s()  const { return filetime_to_string(written_ft); }
  std::string get_class_name() const { return class_name; }

  std::string show() const { return path; }
};

struct value_info {
  std::string key;          // Full key path including hive
  std::string name;         // Value name (empty = default value)
  std::string path;         // key + "\" + display_name
  std::string hive;         // Hive abbreviation
  DWORD       type;         // REG_SZ, REG_DWORD, etc.
  std::string string_value; // Value rendered as a string
  long long   int_value;    // Numeric value (0 for non-numeric types)
  long long   size;         // Raw byte size of the value data
  bool        exists;       // Was the value successfully read?
  unsigned long long written_ft;  // Parent key's last-write time as FILETIME ULL

  value_info()
      : type(REG_NONE), int_value(0), size(0), exists(false), written_ft(0) {}

  std::string get_key()          const { return key; }
  std::string get_name()         const { return name.empty() ? "(default)" : name; }
  std::string get_path()         const { return path; }
  std::string get_hive()         const { return hive; }
  long long   get_type_i()       const { return type; }
  std::string get_type_s()       const { return type_to_string(type); }
  std::string get_string_value() const { return string_value; }
  long long   get_int_value()    const { return int_value; }
  long long   get_size()         const { return size; }
  long long   get_exists()       const { return exists ? 1 : 0; }
  long long   get_written()      const { return filetime_to_epoch(written_ft); }
  long long   get_age()          const {
    const long long w = get_written();
    return (w == 0) ? 0 : time(nullptr) - w;
  }
  std::string get_written_s()    const { return filetime_to_string(written_ft); }

  std::string show() const { return path + "=" + string_value; }

  static long long parse_type_s(const std::string &s) { return parse_type(s); }
};

// ── Internal helpers ─────────────────────────────────────────────────────────

namespace detail {

// Expand a REG_EXPAND_SZ value
inline std::string expand_env(const std::wstring &s) {
  const DWORD needed = ExpandEnvironmentStringsW(s.c_str(), nullptr, 0);
  if (needed == 0) return utf8::cvt<std::string>(s);
  std::vector<wchar_t> buf(needed + 1);
  ExpandEnvironmentStringsW(s.c_str(), buf.data(), needed);
  return utf8::cvt<std::string>(std::wstring(buf.data()));
}

// Convert a REG_MULTI_SZ double-null-terminated list to a comma-separated string
inline std::string multi_sz_to_string(const BYTE *data, const DWORD cbData) {
  std::string result;
  const auto *p = reinterpret_cast<const wchar_t *>(data);
  const auto *end = reinterpret_cast<const wchar_t *>(data + cbData);
  while (p < end && *p != L'\0') {
    if (!result.empty()) result += ", ";
    std::wstring ws(p);
    result += utf8::cvt<std::string>(ws);
    p += ws.size() + 1;
  }
  return result;
}

// Convert REG_BINARY to a hex string
inline std::string binary_to_hex(const BYTE *data, DWORD cbData) {
  static constexpr char hex_chars[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(cbData * 3);
  for (DWORD i = 0; i < cbData; ++i) {
    if (i > 0) result += ' ';
    result += hex_chars[(data[i] >> 4) & 0x0F];
    result += hex_chars[data[i] & 0x0F];
  }
  return result;
}

// Fill value_info by reading a single registry value from an already-open key
inline void fill_value(HKEY hKey, const std::string &value_name, const unsigned long long parent_written_ft, value_info &vi) {
  const std::wstring wname = utf8::cvt<std::wstring>(value_name);

  DWORD type = 0;
  DWORD cbData = 0;
  LONG ret = RegQueryValueExW(hKey, wname.c_str(), nullptr, &type, nullptr, &cbData);
  if (ret != ERROR_SUCCESS) {
    vi.exists = false;
    return;
  }

  std::vector<BYTE> buf(cbData + 4, 0);  // +4 so we can null-terminate wide strings safely
  ret = RegQueryValueExW(hKey, wname.c_str(), nullptr, &type, buf.data(), &cbData);
  if (ret != ERROR_SUCCESS) {
    vi.exists = false;
    return;
  }

  vi.exists = true;
  vi.type = type;
  vi.size = static_cast<long long>(cbData);
  vi.written_ft = parent_written_ft;

  switch (type) {
    case REG_SZ: {
      vi.string_value = utf8::cvt<std::string>(reinterpret_cast<const wchar_t *>(buf.data()));
      try { vi.int_value = str::stox<long long>(vi.string_value); } catch (...) { vi.int_value = 0; }
      break;
    }
    case REG_EXPAND_SZ: {
      vi.string_value = expand_env(reinterpret_cast<const wchar_t *>(buf.data()));
      try { vi.int_value = str::stox<long long>(vi.string_value); } catch (...) { vi.int_value = 0; }
      break;
    }
    case REG_DWORD: {
      if (cbData >= sizeof(DWORD)) {
        DWORD dw = 0;
        memcpy(&dw, buf.data(), sizeof(DWORD));
        vi.int_value = static_cast<long long>(dw);
      }
      vi.string_value = str::xtos(vi.int_value);
      break;
    }
    case REG_QWORD: {
      if (cbData >= sizeof(ULONGLONG)) {
        ULONGLONG qw = 0;
        memcpy(&qw, buf.data(), sizeof(ULONGLONG));
        vi.int_value = static_cast<long long>(qw);
      }
      vi.string_value = str::xtos(vi.int_value);
      break;
    }
    case REG_DWORD_BIG_ENDIAN: {
      if (cbData >= sizeof(DWORD)) {
        DWORD dw = 0;
        memcpy(&dw, buf.data(), sizeof(DWORD));
        // Byte-swap to little-endian
        dw = ((dw & 0xFF000000u) >> 24) |
             ((dw & 0x00FF0000u) >>  8) |
             ((dw & 0x0000FF00u) <<  8) |
             ((dw & 0x000000FFu) << 24);
        vi.int_value = static_cast<long long>(dw);
      }
      vi.string_value = str::xtos(vi.int_value);
      break;
    }
    case REG_MULTI_SZ: {
      vi.string_value = multi_sz_to_string(buf.data(), cbData);
      try { vi.int_value = str::stox<long long>(vi.string_value); } catch (...) { vi.int_value = 0; }
      break;
    }
    case REG_BINARY: {
      vi.string_value = binary_to_hex(buf.data(), cbData);
      vi.int_value = 0;
      break;
    }
    default: {
      vi.string_value = "(binary:" + str::xtos(cbData) + ")";
      vi.int_value = 0;
      break;
    }
  }
}

// Query key metadata from an already-open HKEY
inline void fill_key_metadata(HKEY hKey, key_info &ki) {
  DWORD num_subkeys = 0;
  DWORD num_values = 0;
  DWORD max_class_len = 0;
  FILETIME last_write = {};

  LONG ret = RegQueryInfoKeyW(hKey, nullptr, &max_class_len, nullptr, &num_subkeys, nullptr, nullptr, &num_values, nullptr, nullptr, nullptr, &last_write);
  if (ret != ERROR_SUCCESS) return;

  ki.subkey_count = static_cast<long long>(num_subkeys);
  ki.value_count  = static_cast<long long>(num_values);
  ki.written_ft   = filetime_to_ull(last_write);

  if (max_class_len > 0) {
    std::vector<wchar_t> cls(max_class_len + 2, L'\0');
    DWORD cls_len = max_class_len + 1;
    ret = RegQueryInfoKeyW(hKey, cls.data(), &cls_len, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (ret == ERROR_SUCCESS) ki.class_name = utf8::cvt<std::string>(std::wstring(cls.data()));
  }
}

}  // namespace detail

// ── Public API ───────────────────────────────────────────────────────────────

// Connect to a remote registry host (returns NULL for local).
// The caller must call RegCloseKey on the returned handle when done (use raii_hkey).
// If computer is empty, returns NULL and no connection is made.
inline HKEY connect_registry(const std::string &computer, HKEY hive) {
  if (computer.empty()) return nullptr;
  HKEY remote_root = nullptr;
  const std::wstring wcomputer = utf8::cvt<std::wstring>("\\\\" + computer);
  const LONG ret = RegConnectRegistryW(wcomputer.c_str(), hive, &remote_root);
  if (ret != ERROR_SUCCESS)
    throw registry_exception("Failed to connect to remote registry on " + computer + ": " + error::format::from_system(ret));
  return remote_root;
}

// Open a registry key and fill a key_info, then close the key.
// parent_path is the full path including hive (e.g. "HKLM\Software") — the parent that was iterated.
// name is the sub-key name (leaf).
// depth is the depth relative to the starting key (0 = starting key itself).
// access_flags may include KEY_WOW64_32KEY / KEY_WOW64_64KEY.
// base_hkey is the already-opened parent HKEY (may be a root hive or a connected remote hive).
inline key_info open_key(HKEY base_hkey, const std::string &subpath, const std::string &full_path,
                          const std::string &name, const std::string &parent, const std::string &hive,
                          long long depth, const DWORD access_flags) {
  key_info ki;
  ki.path   = full_path;
  ki.name   = name;
  ki.parent = parent;
  ki.hive   = hive;
  ki.depth  = depth;
  ki.exists = false;

  HKEY hKey = nullptr;
  const std::wstring wsubpath = utf8::cvt<std::wstring>(subpath);
  const LONG ret = RegOpenKeyExW(base_hkey, wsubpath.empty() ? nullptr : wsubpath.c_str(), 0,
                                  KEY_QUERY_VALUE | KEY_READ | access_flags, &hKey);
  if (ret != ERROR_SUCCESS) return ki;  // exists = false

  ki.exists = true;
  detail::fill_key_metadata(hKey, ki);
  RegCloseKey(hKey);
  return ki;
}

// Enumerate the immediate sub-keys of a given key.
// base_hkey: root hive HKEY (possibly a connected remote hive).
// parent_subpath: path relative to base_hkey to the key whose children we enumerate.
// parent_full_path: full path of the parent including hive prefix (e.g. "HKLM\Software").
// hive: hive abbreviation string.
// depth: depth to assign to the returned key_info objects.
// access_flags: KEY_WOW64_32KEY / KEY_WOW64_64KEY or 0.
inline std::vector<key_info> enum_sub_keys(HKEY base_hkey, const std::string &parent_subpath,
                                            const std::string &parent_full_path, const std::string &hive,
                                            long long depth, DWORD access_flags) {
  std::vector<key_info> result;

  HKEY hParent = nullptr;
  const std::wstring wparent = utf8::cvt<std::wstring>(parent_subpath);
  LONG ret = RegOpenKeyExW(base_hkey, wparent.empty() ? nullptr : wparent.c_str(), 0,
                            KEY_QUERY_VALUE | KEY_READ | access_flags, &hParent);
  if (ret != ERROR_SUCCESS) return result;

  // Count sub-keys
  DWORD num_subkeys = 0;
  DWORD max_name_len = 0;
  FILETIME last_write_ignored = {};
  ret = RegQueryInfoKeyW(hParent, nullptr, nullptr, nullptr, &num_subkeys, &max_name_len,
                          nullptr, nullptr, nullptr, nullptr, nullptr, &last_write_ignored);
  if (ret != ERROR_SUCCESS || num_subkeys == 0) {
    RegCloseKey(hParent);
    return result;
  }

  std::vector<wchar_t> namebuf(max_name_len + 2);
  for (DWORD i = 0; i < num_subkeys; ++i) {
    DWORD cchName = max_name_len + 1;
    FILETIME ft = {};
    ret = RegEnumKeyExW(hParent, i, namebuf.data(), &cchName, nullptr, nullptr, nullptr, &ft);
    if (ret != ERROR_SUCCESS) continue;

    const std::string child_name = utf8::cvt<std::string>(std::wstring(namebuf.data(), cchName));
    const std::string child_subpath = parent_subpath.empty() ? child_name : parent_subpath + "\\" + child_name;
    const std::string child_full_path = parent_full_path + "\\" + child_name;

    key_info ki;
    ki.path   = child_full_path;
    ki.name   = child_name;
    ki.parent = parent_full_path;
    ki.hive   = hive;
    ki.depth  = depth;
    ki.exists = true;
    ki.written_ft = filetime_to_ull(ft);

    // Open child to get value_count and subkey_count
    HKEY hChild = nullptr;
    const std::wstring wchild = utf8::cvt<std::wstring>(child_subpath);
    if (RegOpenKeyExW(base_hkey, wchild.c_str(), 0, KEY_QUERY_VALUE | KEY_READ | access_flags, &hChild) == ERROR_SUCCESS) {
      detail::fill_key_metadata(hChild, ki);
      RegCloseKey(hChild);
    }

    result.push_back(ki);
  }

  RegCloseKey(hParent);
  return result;
}

// Enumerate all values of a registry key.
// base_hkey: root hive HKEY (possibly a connected remote hive).
// key_subpath: path relative to base_hkey.
// key_full_path: full path including hive prefix.
// hive: hive abbreviation.
// access_flags: WOW64 flags or 0.
inline std::vector<value_info> enum_values(HKEY base_hkey, const std::string &key_subpath,
                                            const std::string &key_full_path, const std::string &hive,
                                            DWORD access_flags) {
  std::vector<value_info> result;

  HKEY hKey = nullptr;
  const std::wstring wsubpath = utf8::cvt<std::wstring>(key_subpath);
  LONG ret = RegOpenKeyExW(base_hkey, wsubpath.empty() ? nullptr : wsubpath.c_str(), 0,
                            KEY_QUERY_VALUE | KEY_READ | access_flags, &hKey);
  if (ret != ERROR_SUCCESS) return result;

  // Get metadata: number of values, max name length, last-write time
  DWORD num_values = 0;
  DWORD max_name_len = 0;
  FILETIME last_write = {};
  ret = RegQueryInfoKeyW(hKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          &num_values, &max_name_len, nullptr, nullptr, &last_write);
  if (ret != ERROR_SUCCESS || num_values == 0) {
    RegCloseKey(hKey);
    return result;
  }

  const unsigned long long written_ft = filetime_to_ull(last_write);
  std::vector<wchar_t> namebuf(max_name_len + 2);
  for (DWORD i = 0; i < num_values; ++i) {
    DWORD cchName = max_name_len + 1;
    ret = RegEnumValueW(hKey, i, namebuf.data(), &cchName, nullptr, nullptr, nullptr, nullptr);
    if (ret != ERROR_SUCCESS && ret != ERROR_MORE_DATA) continue;

    const std::string vname = utf8::cvt<std::string>(std::wstring(namebuf.data(), cchName));
    value_info vi;
    vi.key  = key_full_path;
    vi.name = vname;
    vi.path = key_full_path + "\\" + (vname.empty() ? "(default)" : vname);
    vi.hive = hive;
    detail::fill_value(hKey, vname, written_ft, vi);
    result.push_back(vi);
  }

  RegCloseKey(hKey);
  return result;
}

// Recursively enumerate sub-keys up to max_depth.
// depth=0 means the key itself is the starting point; children get depth=1.
// If include_root is true the starting key itself is included in the results.
inline void recursive_enum_keys(HKEY base_hkey, const std::string &subpath, const std::string &full_path,
                                 const std::string &hive, long long depth, long long max_depth,
                                 DWORD access_flags, std::vector<key_info> &out) {
  if (max_depth >= 0 && depth > max_depth) return;

  const std::vector<key_info> children = enum_sub_keys(base_hkey, subpath, full_path, hive, depth, access_flags);
  for (const key_info &child : children) {
    out.push_back(child);
    const std::string child_subpath = subpath.empty() ? child.name : subpath + "\\" + child.name;
    recursive_enum_keys(base_hkey, child_subpath, child.path, hive, depth + 1, max_depth, access_flags, out);
  }
}

// Recursively enumerate values in a key tree up to max_depth.
inline void recursive_enum_values(HKEY base_hkey, const std::string &subpath, const std::string &full_path,
                                   const std::string &hive, long long depth, long long max_depth,
                                   DWORD access_flags, std::vector<value_info> &out) {
  if (max_depth >= 0 && depth > max_depth) return;

  const std::vector<value_info> vals = enum_values(base_hkey, subpath, full_path, hive, access_flags);
  for (const value_info &v : vals) out.push_back(v);

  if (max_depth < 0 || depth < max_depth) {
    // Recurse into sub-keys
    const std::vector<key_info> children = enum_sub_keys(base_hkey, subpath, full_path, hive, depth + 1, access_flags);
    for (const key_info &child : children) {
      const std::string child_subpath = subpath.empty() ? child.name : subpath + "\\" + child.name;
      recursive_enum_values(base_hkey, child_subpath, child.path, hive, depth + 1, max_depth, access_flags, out);
    }
  }
}

}  // namespace win_registry

#endif  // WIN32
