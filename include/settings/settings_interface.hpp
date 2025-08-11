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

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <list>
#include <settings/settings_value.hpp>
#include <string>
#include <utility>

namespace settings {
class settings_exception : public std::exception {
  const char* file_;
  int line_;
  std::string error_;

 public:
  //////////////////////////////////////////////////////////////////////////
  /// Constructor takes an error message.
  /// @param file THe file in which the error occurred
  /// @param line The line in which the error occurred
  /// @param error the error message
  ///
  /// @author mickem
  settings_exception(const char* file, const int line, std::string error) noexcept : file_(file), line_(line), error_(std::move(error)) {}
  settings_exception(const settings_exception& other) noexcept : settings_exception(other.file_, other.line_, other.error_) {}
  ~settings_exception() noexcept override = default;

  //////////////////////////////////////////////////////////////////////////
  /// Retrieve the error message from the exception.
  /// @return the error message
  ///
  /// @author mickem
  const char* what() const throw() { return error_.c_str(); }
  // std::string reason() const throw() { return utf8::utf8_from_native(what()); }
  const char* file() const { return file_; }
  int line() const { return line_; }
};

class settings_interface;
typedef boost::shared_ptr<settings_interface> instance_ptr;
typedef boost::shared_ptr<settings_interface> instance_raw_ptr;

typedef std::list<std::string> error_list;

class settings_interface {
 public:
  typedef std::list<std::string> string_list;
  typedef boost::optional<std::string> op_string;
  typedef boost::optional<int> op_int;
  typedef boost::optional<bool> op_bool;

  virtual void ensure_exists() = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Empty all cached settings values and force a reload.
  /// Notice this does not save so any "active" values will be flushed and new ones read from file.
  ///
  /// @author mickem
  virtual void clear_cache() = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Get a string value if it does not exist exception will be thrown
  ///
  /// @param path the path to look up
  /// @param key the key to lookup
  /// @return the string value
  ///
  /// @author mickem
  virtual op_string get_string(std::string path, std::string key) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Get a string value if it does not exist the default value will be returned
  ///
  /// @param path the path to look up
  /// @param key the key to lookup
  /// @param def the default value to use when no value is found
  /// @return the string value
  ///
  /// @author mickem
  virtual std::string get_string(std::string path, std::string key, std::string def) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Set or update a string value
  ///
  /// @param path the path to look up
  /// @param key the key to lookup
  /// @param value the value to set
  ///
  /// @author mickem
  virtual void set_string(std::string path, std::string key, std::string value) = 0;

  virtual void remove_key(std::string path, std::string key) = 0;
  virtual void remove_path(std::string path) = 0;

  // Meta Functions
  //////////////////////////////////////////////////////////////////////////
  /// Get all (sub) sections (given a path).
  /// If the path is empty all root sections will be returned
  ///
  /// @param path The path to get sections from (if empty root sections will be returned)
  /// @return a list of sections
  ///
  /// @author mickem
  virtual string_list get_sections(std::string path) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Get all keys for a path.
  ///
  /// @param path The path to get keys under
  /// @return a list of keys
  ///
  /// @author mickem
  virtual string_list get_keys(std::string path) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Does the section exists?
  ///
  /// @param path The path of the section
  /// @return true/false
  ///
  /// @author mickem
  virtual bool has_section(std::string path) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Does the key exists?
  ///
  /// @param path The path of the section
  /// @param key The key to check
  /// @return true/false
  ///
  /// @author mickem
  virtual bool has_key(std::string path, std::string key) = 0;

  virtual void add_path(std::string path) = 0;
  // Misc Functions
  //////////////////////////////////////////////////////////////////////////
  /// Get a context.
  /// The context is an identifier for the settings store for INI/XML it is the filename.
  ///
  /// @return the context
  ///
  /// @author mickem
  virtual std::string get_context() = 0;

  // Save/Load Functions
  //////////////////////////////////////////////////////////////////////////
  /// Reload the settings store
  ///
  /// @author mickem
  virtual void reload() = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Copy the settings store to another settings store
  ///
  /// @param other the settings store to save to
  ///
  /// @author mickem
  virtual void save_to(instance_ptr other) = 0;
  virtual void save_to(std::string alias, std::string other) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Save the settings store
  ///
  /// @author mickem
  virtual void save(bool re_save_all) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Load settings from the context.
  ///
  /// @author mickem
  virtual void load() = 0;

  virtual std::string get_type() = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Validate the settings store and report all missing/invalid and superfluous keys.
  ///
  /// @author mickem
  virtual settings::error_list validate() = 0;

  virtual std::string to_string() = 0;

  virtual std::string get_info() = 0;

  static bool string_to_bool(std::string str) {
    std::string tmp = boost::to_lower_copy(str);
    return tmp == "true" || tmp == "1";
  }

  virtual std::list<boost::shared_ptr<settings_interface> > get_children() = 0;

  virtual void house_keeping() = 0;

  virtual void enable_credentials() = 0;
  virtual bool supports_updates() = 0;
};
}  // namespace settings
