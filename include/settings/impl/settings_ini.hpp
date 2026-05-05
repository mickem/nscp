/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include <simpleini/simpleini.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <cerrno>
#include <cwctype>
#include <error/error.hpp>
#include <file_helpers.hpp>
#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>
#include <str/utils.hpp>
#include <tuple>
#include <vector>
#ifdef WIN32
#include <win/credentials.hpp>
#endif

#include <string>
const std::string CREDENTIAL_MARKER = "$CRED$; ";

inline std::string make_credential_alias(const std::string &path, const std::string &key) { return "NSClient++-" + path + "." + key; }

namespace settings {
class INISettings : public settings_interface_impl {
 private:
  CSimpleIni ini;
  bool is_loaded_;
  std::string filename_;
  bool use_credentials_;

 public:
  INISettings(settings_core *core, std::string alias, std::string context)
      : settings_interface_impl(core, alias, context), ini(false, false, false), is_loaded_(false), use_credentials_(false) {
    load_data();
  }

  bool supports_updates() override { return true; }

  //////////////////////////////////////////////////////////////////////////
  /// Get a string value if it does not exist exception will be thrown
  ///
  /// @param path the path to look up
  /// @param key the key to lookup
  /// @return the string value
  ///
  /// @author mickem
  op_string get_real_string(settings_core::key_path_type key) override {
    load_data();
    const wchar_t *val = ini.GetValue(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(), NULL);
    if (val == nullptr) return {};
    const auto value = utf8::cvt<std::string>(val);
    if (boost::starts_with(value, CREDENTIAL_MARKER)) {
#ifdef WIN32
      const auto alias = make_credential_alias(key.first, key.second);
      return read_credential(alias);
#else
      core_->get_logger()->error("settings", __FILE__, __LINE__, "Credentials not supported on this platform: " + key.first + "." + key.second);
#endif
    }
    return op_string(utf8::cvt<std::string>(val));
  }
  //////////////////////////////////////////////////////////////////////////
  /// Check if a key exists
  ///
  /// @param path the path to look up
  /// @param key the key to lookup
  /// @return true/false if the key exists.
  ///
  /// @author mickem
  bool has_real_key(settings_core::key_path_type key) override {
    return ini.GetValue(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str()) != nullptr;
  }

  bool has_real_path(std::string path) override { return ini.GetSectionSize(utf8::cvt<std::wstring>(path).c_str()) > 0; }

  static std::string render_comment(const boost::optional<settings_core::key_description> &desc) {
    if (!desc.has_value()) {
      return "";
    }
    std::string comment = "; ";
    if (!desc.value().title.empty()) comment += desc.value().title + " - ";
    if (!desc.value().description.empty()) comment += desc.value().description;
    str::utils::replace(comment, "\n", " ");
    return comment;
  }

  static std::string render_comment(const settings_core::path_description &desc) {
    std::string comment = "; ";
    if (!desc.title.empty()) comment += desc.title + " - ";
    if (!desc.description.empty()) comment += desc.description;
    str::utils::replace(comment, "\n", " ");
    return comment;
  }

  //////////////////////////////////////////////////////////////////////////
  /// Write a value to the resulting context.
  ///
  /// @param key The key to write to
  /// @param value The value to write
  ///
  /// @author mickem
  void set_real_value(settings_core::key_path_type key, conainer value) override {
    if (!value.is_dirty()) return;
    try {
      const auto desc = get_core()->get_registered_key(key.first, key.second);
      const std::string comment = render_comment(desc);
      ini.Delete(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str());

      if (use_credentials_ && get_core()->is_sensitive_key(key.first, key.second)) {
#ifdef WIN32
        const auto alias = make_credential_alias(key.first, key.second);
        save_credential(alias, value.get_string());
        ini.SetValue(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(),
                     utf8::cvt<std::wstring>(CREDENTIAL_MARKER + "Se credential manager: " + alias).c_str(), utf8::cvt<std::wstring>(comment).c_str());
        return;
#else
        get_logger()->warning(
            "settings", __FILE__, __LINE__,
            "Credential mapping currently only supported on windows (storing key as clerar text): " + make_skey(key.first, key.second) + " in clear text");
#endif
      }
      ini.SetValue(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(), utf8::cvt<std::wstring>(value.get_string()).c_str(),
                   utf8::cvt<std::wstring>(comment).c_str());
    } catch (settings_exception &e) {
      get_logger()->error("settings", __FILE__, __LINE__, "Failure when writing key: " + make_skey(key.first, key.second) + ": " + e.what());
    } catch (...) {
      get_logger()->error("settings", __FILE__, __LINE__, "Unknown failure when writing key: " + make_skey(key.first, key.second));
    }
  }

  void set_real_path(std::string path) override {
    try {
      const settings_core::path_description desc = get_core()->get_registered_path(path);
      const std::string comment = render_comment(desc);
      if (!comment.empty()) {
        ini.SetValue(utf8::cvt<std::wstring>(path).c_str(), NULL, NULL, utf8::cvt<std::wstring>(comment).c_str());
      }
    } catch (settings_exception &_e) {
      ini.SetValue(utf8::cvt<std::wstring>(path).c_str(), NULL, NULL, L"; Undocumented section");
    } catch (...) {
      get_logger()->error("settings", __FILE__, __LINE__, "Unknown failure when writing section: " + path);
    }
  }

  void remove_real_value(settings_core::key_path_type key) override {
    ini.Delete(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(), true);
  }
  void remove_real_path(std::string path) override { ini.Delete(utf8::cvt<std::wstring>(path).c_str(), NULL, true); }

  //////////////////////////////////////////////////////////////////////////
  /// Get all (sub) sections (given a path).
  /// If the path is empty all root sections will be returned
  ///
  /// @param path The path to get sections from (if empty root sections will be returned)
  /// @param list The list to append nodes to
  /// @return a list of sections
  ///
  /// @author mickem
  void get_real_sections(std::string path, string_list &list) override {
    CSimpleIni::TNamesDepend lst;
    std::string::size_type path_len = path.length();
    ini.GetAllSections(lst);
    if (path.empty()) {
      for (const CSimpleIni::Entry &e : lst) {
        std::string key = utf8::cvt<std::string>(e.pItem);
        if (key.length() > 1) {
          const std::string::size_type pos = key.find('/', 1);
          if (pos != std::string::npos) key = key.substr(0, pos);
        }
        list.push_back(key);
      }
    } else {
      for (const CSimpleIni::Entry &e : lst) {
        std::string key = utf8::cvt<std::string>(e.pItem);
        if (key.length() > path_len + 1 && key.substr(0, path_len) == path) {
          const std::string::size_type pos = key.find('/', path_len + 1);
          if (pos == std::string::npos && path_len > 1) {
            if (key[path_len] == '/' || key[path_len] == '\\') {
              key = key.substr(path_len + 1);
            } else {
              key = key.substr(path_len);
            }
          } else if (pos == std::string::npos)
            key = key.substr(path_len);
          else if (path_len > 1) {
            if (key[path_len] == '/' || key[path_len] == '\\') {
              key = key.substr(path_len + 1, pos - path_len - 1);
            } else {
              key = key.substr(path_len, pos - path_len);
            }
          } else
            key = key.substr(path_len, pos - path_len);
          list.push_back(key);
        }
      }
    }
  }
  //////////////////////////////////////////////////////////////////////////
  /// Get all keys given a path/section.
  /// If the path is empty all root sections will be returned
  ///
  /// @param path The path to get sections from (if empty root sections will be returned)
  /// @param list The list to append nodes to
  /// @return a list of sections
  ///
  /// @author mickem
  void get_real_keys(std::string path, string_list &list) override {
    load_data();
    CSimpleIni::TNamesDepend lst;
    ini.GetAllKeys(utf8::cvt<std::wstring>(path).c_str(), lst);
    for (const CSimpleIni::Entry &e : lst) {
      list.push_back(utf8::cvt<std::string>(e.pItem));
    }
  }
  //////////////////////////////////////////////////////////////////////////
  /// Save the settings store
  ///
  /// @author mickem
  void save(bool re_save_all) override {
    settings_interface_impl::save(re_save_all);

    const SI_Error rc = ini.SaveFile(get_file_name().string().c_str());
    if (rc < 0) throw_SI_error(rc, "Failed to save file");
  }

  //////////////////////////////////////////////////////////////////////////
  /// Re-emit the INI file with sections, and keys within each section,
  /// sorted alphabetically (issue #205). Section comments and per-key
  /// comments are preserved. The exception is the special `/modules`
  /// section, which is kept as the first section (the convention in
  /// generated configs is for `[/modules]` to come first).
  void save_sorted() override {
    settings_interface_impl::save(true);

    // Snapshot every (section, key) -> value/comment pair from the live
    // CSimpleIni then re-insert them in alphabetical order into a fresh
    // CSimpleIni. CSimpleIni's Save uses LoadOrder (insertion order), so
    // re-inserting alphabetically yields alphabetical output.
    CSimpleIni::TNamesDepend sections;
    ini.GetAllSections(sections);

    // Portable case-insensitive less-than for wchar_t strings (CSimpleIni's
    // own KeyOrder is class-template-private so we replicate the same notion
    // here).
    auto wstr_iless = [](const std::wstring &lhs, const std::wstring &rhs) {
      return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                                          [](wchar_t a, wchar_t b) { return std::towlower(a) < std::towlower(b); });
    };

    struct section_data {
      std::wstring name;
      const wchar_t *comment{};  // owned by `ini`; valid for the lifetime of this method
      std::vector<std::tuple<std::wstring, std::wstring, const wchar_t *>> entries;
    };
    std::vector<section_data> ordered;
    ordered.reserve(sections.size());
    for (const CSimpleIni::Entry &eSection : sections) {
      section_data sd;
      sd.name = eSection.pItem;
      sd.comment = eSection.pComment;
      CSimpleIni::TNamesDepend keys;
      ini.GetAllKeys(eSection.pItem, keys);
      for (const CSimpleIni::Entry &eKey : keys) {
        const wchar_t *value = ini.GetValue(eSection.pItem, eKey.pItem);
        sd.entries.emplace_back(std::wstring(eKey.pItem), std::wstring(value ? value : L""), eKey.pComment);
      }
      // Keys: alphabetical (case-insensitive to match CSimpleIni KeyOrder).
      std::sort(sd.entries.begin(), sd.entries.end(),
                [&wstr_iless](const auto &lhs, const auto &rhs) { return wstr_iless(std::get<0>(lhs), std::get<0>(rhs)); });
      ordered.push_back(std::move(sd));
    }
    // Sections: alphabetical, but pin `/modules` first (the convention of
    // generated configs is for `[/modules]` to come at the top; users
    // requesting alphabetisation in #205 explicitly noted this).
    auto section_rank = [](const std::wstring &n) -> int { return n == L"/modules" ? 0 : 1; };
    std::sort(ordered.begin(), ordered.end(), [&](const section_data &lhs, const section_data &rhs) {
      const int lr = section_rank(lhs.name);
      const int rr = section_rank(rhs.name);
      if (lr != rr) return lr < rr;
      return wstr_iless(lhs.name, rhs.name);
    });

    CSimpleIni sorted_ini(false, false, false);
    sorted_ini.SetUnicode();
    for (const auto &sd : ordered) {
      // SetValue(section, NULL, NULL, comment) registers the section header
      // (and its comment) so empty sections survive too.
      sorted_ini.SetValue(sd.name.c_str(), nullptr, nullptr, sd.comment);
      for (const auto &e : sd.entries) {
        sorted_ini.SetValue(sd.name.c_str(), std::get<0>(e).c_str(), std::get<1>(e).c_str(), std::get<2>(e));
      }
    }

    const SI_Error rc = sorted_ini.SaveFile(get_file_name().string().c_str());
    if (rc < 0) throw_SI_error(rc, "Failed to save sorted file");

    // Reload the live ini so subsequent operations see the same insertion
    // order (otherwise nOrder would still reflect the pre-sort order).
    ini.Reset();
    is_loaded_ = false;
    load_data();
  }

  error_list validate() override {
    error_list ret;
    CSimpleIni::TNamesDepend sections;
    ini.GetAllSections(sections);
    for (const CSimpleIni::Entry &ePath : sections) {
      std::string path = utf8::cvt<std::string>(ePath.pItem);
      try {
        get_core()->get_registered_path(path);
      } catch (const settings_exception &) {
        ret.push_back(std::string("Invalid path: ") + path);
      }
      CSimpleIni::TNamesDepend keys;
      ini.GetAllKeys(ePath.pItem, keys);
      for (const CSimpleIni::Entry &eKey : keys) {
        std::string key = utf8::cvt<std::string>(eKey.pItem);
        try {
          get_core()->get_registered_key(path, key);
        } catch (const settings_exception &) {
          ret.push_back(std::string("Invalid key: ") + settings::key_to_string(path, key));
        }
      }
    }

    return ret;
  }

  void real_clear_cache() override {
    is_loaded_ = false;
    load_data();
  }

 private:
  void load_data() {
    if (is_loaded_) return;
    if (boost::filesystem::is_directory(get_file_name())) {
      const boost::filesystem::directory_iterator it(get_file_name());
      const boost::filesystem::directory_iterator eod;

      for (boost::filesystem::path const &p : boost::make_iterator_range(it, eod)) {
        add_child_unsafe(file_helpers::meta::get_filename(p), "ini:///" + p.string());
      }
    }
    if (!file_exists()) {
      is_loaded_ = true;
      return;
    }
    auto f = utf8::cvt<std::string>(get_file_name().string());
    ini.SetUnicode();
    get_logger()->debug("settings", __FILE__, __LINE__, "Loading: " + get_file_name().string());
    const SI_Error rc = ini.LoadFile(f.c_str());
    if (rc < 0) throw_SI_error(rc, "Failed to load file");

    get_core()->register_path(999, "/includes", "INCLUDED FILES", "Files to be included in the configuration", false, false);
    CSimpleIni::TNamesDepend lst;
    ini.GetAllKeys(L"/includes", lst);
    for (const auto & cit : lst) {
      const std::string alias = utf8::cvt<std::string>(cit.pItem);
      const std::string child = utf8::cvt<std::string>(ini.GetValue(L"/includes", cit.pItem));
      get_core()->register_key(999, "/includes", utf8::cvt<std::string>(cit.pItem), "string", "INCLUDED FILE", "Included configuration", "", true, false);
      if (!child.empty()) add_child_unsafe(alias, child);
    }
    is_loaded_ = true;
  }
  void throw_SI_error(SI_Error err, std::string msg) {
    std::string error_str = "unknown error";
    if (err == SI_NOMEM) error_str = "Out of memory";
    if (err == SI_FAIL) error_str = "General failure";
    if (err == SI_FILE) {
#ifdef WIN32
      const int saved_errno = errno;
      error_str = "I/O error: " + error::lookup::last_error();
#else
      const int saved_errno = errno;
      if (saved_errno != 0) {
        error_str = "I/O error: " + error::format::from_system(saved_errno) + " (errno=" + std::to_string(saved_errno) + ")";
        if (saved_errno == EACCES || saved_errno == EPERM || saved_errno == EROFS) {
          error_str += " - check that the user running nscp has write access to the settings file and its directory";
        }
      } else {
        error_str = "I/O error";
      }
#endif
    }
    const std::string file_hint = filename_.empty() ? get_context() : filename_;
    throw settings_exception(__FILE__, __LINE__, msg + " '" + file_hint + "': " + error_str);
  }
  boost::filesystem::path get_file_name() {
    if (filename_.empty()) {
      filename_ = get_file_from_context();
      if (!filename_.empty()) {
        if (boost::filesystem::is_regular_file(filename_)) {
        } else if (boost::filesystem::is_regular_file(filename_.substr(1))) {
          filename_ = filename_.substr(1);
        } else if (boost::filesystem::is_directory(filename_)) {
        } else if (boost::filesystem::is_directory(filename_.substr(1))) {
          filename_ = filename_.substr(1);
        } else {
          std::string tmp = core_->find_file("${exe-path}/" + filename_, "");
          if (boost::filesystem::exists(tmp)) {
            filename_ = tmp;
          } else {
            tmp = core_->find_file("${exe-path}/" + filename_.substr(1), "");
            if (boost::filesystem::exists(tmp)) {
              filename_ = tmp;
            } else {
              get_logger()->debug("settings", __FILE__, __LINE__, "Configuration file not found: " + filename_);
            }
          }
        }
      }
    }
    return utf8::cvt<std::string>(filename_);
  }
  bool file_exists() { return boost::filesystem::is_regular_file(get_file_name()); }
  std::string get_info() override { return "INI settings: (" + context_ + ", " + get_file_name().string() + ")"; }

 public:
  static bool context_exists(settings_core *core, std::string key) {
    const net::url url = net::parse(key);
    const std::string file = url.host + url.path;
    std::string tmp = core->expand_path(file);
    if (tmp.size() > 1 && tmp[0] == '/') {
      if (boost::filesystem::is_regular_file(tmp) || boost::filesystem::is_directory(tmp)) return true;
      tmp = tmp.substr(1);
    }
    return boost::filesystem::is_regular_file(tmp) || boost::filesystem::is_directory(tmp);
  }
  void ensure_exists() override { save(false); }
  std::string get_type() override { return "ini"; }

  void enable_credentials() override { use_credentials_ = true; }
};
}  // namespace settings
