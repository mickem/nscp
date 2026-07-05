// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <nscapi/dll_defines.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <settings/client/settings_client_interface.hpp>
#include <settings/settings_interface.hpp>
#include <string>

namespace nscapi {

// Disable C4275: non dll-interface class used as base for dll-interface class
// This is safe because settings_impl_interface is a pure interface with no data members
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4275)
#endif

class NSCAPI_EXPORT settings_proxy : public settings_helper::settings_impl_interface {
  unsigned int plugin_id_;
  core_wrapper* core_;

 public:
  typedef std::shared_ptr<settings_proxy> ptr;
  settings_proxy(unsigned int plugin_id, core_wrapper* core) : plugin_id_(plugin_id), core_(core) {}

  static ptr create(unsigned int plugin_id, core_wrapper* core);

  typedef std::list<std::string> string_list;

  void register_path(std::string path, std::string title, std::string description, bool advanced, bool sample) override;
  void register_key(std::string path, std::string key, std::string type, std::string title, std::string description, std::string defValue, bool advanced,
                    bool sample, bool sensitive) override;
  void register_subkey(std::string path, std::string title, std::string description, bool advanced, bool sample) override;
  void register_tpl(std::string path, std::string title, std::string icon, std::string description, std::string fields) override;

  std::string get_string(std::string path, std::string key, std::string def) override;
  void set_string(std::string path, std::string key, std::string value) override;
  virtual int get_int(std::string path, std::string key, int def);
  virtual void set_int(std::string path, std::string key, int value);
  virtual bool get_bool(std::string path, std::string key, bool def);
  virtual void set_bool(std::string path, std::string key, bool value);
  string_list get_sections(std::string path) override;
  string_list get_keys(std::string path) override;
  std::string expand_path(std::string key) override;

  void remove_key(std::string path, std::string key) override;
  void remove_path(std::string path) override;

  void err(const char* file, int line, std::string message) override;
  void warn(const char* file, int line, std::string message) override;
  void info(const char* file, int line, std::string message) override;
  void debug(const char* file, int line, std::string message) override;
  void save(std::string context = "");
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace nscapi