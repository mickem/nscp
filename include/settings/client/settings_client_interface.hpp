// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <nscp/path_rooting.hpp>
#include <string>

namespace nscapi {
namespace settings_helper {

class settings_impl_interface {
 public:
  virtual ~settings_impl_interface() = default;
  typedef std::list<std::string> string_list;

  //////////////////////////////////////////////////////////////////////////
  /// Register a path with the settings module.
  /// A registered key or path will be nicely documented in some of the settings files when converted.
  ///
  /// @param path The path to register
  /// @param title The title to use
  /// @param description the description to use
  /// @param advanced advanced options will only be included if they are changed
  virtual void register_path(std::string path, std::string title, std::string description, bool advanced, bool sample) = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Register a key with the settings module.
  /// A registered key or path will be nicely documented in some of the settings files when converted.
  ///
  /// @param path The path to register
  /// @param key The key to register
  /// @param type The type of value, string, boolean or int
  /// @param title The title to use
  /// @param description the description to use
  /// @param defValue the default value
  /// @param advanced advanced options will only be included if they are changed
  virtual void register_key(std::string path, std::string key, std::string type, std::string title, std::string description, std::string defValue,
                            bool advanced, bool sample, bool sensitive) = 0;

  virtual void register_subkey(std::string path, std::string title, std::string description, bool advanced, bool sample) = 0;

  virtual void register_tpl(std::string path, std::string title, std::string icon, std::string description, std::string fields) = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Get a string value if it does not exist the default value will be returned
  ///
  /// @param path the path to look up
  /// @param key the key to lookup
  /// @param def the default value to use when no value is found
  /// @return the string value
  virtual std::string get_string(std::string path, std::string key, std::string def) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Set or update a string value
  ///
  /// @param path the path to look up
  /// @param key the key to lookup
  /// @param value the value to set
  virtual void set_string(std::string path, std::string key, std::string value) = 0;

  // Meta Functions
  //////////////////////////////////////////////////////////////////////////
  /// Get all (sub) sections (given a path).
  /// If the path is empty all root sections will be returned
  ///
  /// @param path The path to get sections from (if empty root sections will be returned)
  /// @return a list of sections
  virtual string_list get_sections(std::string path) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Get all keys for a path.
  ///
  /// @param path The path to get keys under
  /// @return a list of keys
  virtual string_list get_keys(std::string path) = 0;

  virtual std::string expand_path(std::string key) = 0;

  // expand_path, then root the answer at `default_root` if it does not name a
  // location of its own - for a setting whose consumer owns a folder and whose
  // bare names belong in it. See nscp/path_rooting.hpp for why the two are
  // separate jobs.
  //
  // Non-virtual, and composed from expand_path on purpose: every implementer of
  // this interface (including each test double) would otherwise have to grow a
  // method that could only ever be written this one way.
  std::string resolve_path(std::string key, const std::string &default_root) {
    return nscp::paths::root_path(std::move(key), default_root, [this](std::string value) { return this->expand_path(std::move(value)); });
  }

  virtual void remove_key(std::string path, std::string key) = 0;
  virtual void remove_path(std::string path) = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Log an ERROR message.
  ///
  /// @param file the file where the event happened
  /// @param line the line where the event happened
  /// @param message the message to log
  virtual void err(const char* file, int line, std::string message) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Log an WARNING message.
  ///
  /// @param file the file where the event happened
  /// @param line the line where the event happened
  /// @param message the message to log
  virtual void warn(const char* file, int line, std::string message) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Log an INFO message.
  ///
  /// @param file the file where the event happened
  /// @param line the line where the event happened
  /// @param message the message to log
  virtual void info(const char* file, int line, std::string message) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Log an DEBUG message.
  ///
  /// @param file the file where the event happened
  /// @param line the line where the event happened
  /// @param message the message to log
  virtual void debug(const char* file, int line, std::string message) = 0;
};
}  // namespace settings_helper
}  // namespace nscapi
