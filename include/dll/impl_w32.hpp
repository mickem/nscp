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
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>
#include <error/error.hpp>
#include <utf8.hpp>
#include <win/windows.hpp>

namespace dll {
namespace win32 {
class impl : public boost::noncopyable {
  HMODULE handle_;
  boost::filesystem::path module_;

 public:
  explicit impl(const boost::filesystem::path &module) : handle_(nullptr), module_(module) {
    if (!boost::filesystem::is_regular_file(module_)) {
      module_ = fix_module_name(module_);
    }
  }
  static boost::filesystem::path fix_module_name(boost::filesystem::path module) {
    if (boost::filesystem::is_regular_file(module)) return module;
    /* this one (below) is wrong I think */
    boost::filesystem::path mod = module / get_extension();
    if (boost::filesystem::is_regular_file(mod)) return mod;
    mod = boost::filesystem::path(module.string() + get_extension());
    if (boost::filesystem::is_regular_file(mod)) return mod;
    return module;
  }

  static std::string get_extension() { return ".dll"; }

  static bool is_module(const std::string &file) { return boost::ends_with(file, get_extension()); }

  void load_library() {
    if (handle_ != nullptr) unload_library();
    handle_ = LoadLibrary(module_.native().c_str());
    if (handle_ == nullptr)
      throw dll_exception("Could not load library: " + utf8::cvt<std::string>(error::lookup::last_error()) + ": " + module_.filename().string());
  }
  LPVOID load_proc(const std::string &name) const {
    if (handle_ == nullptr) throw dll_exception("Failed to load process since module is not loaded: " + module_.filename().string());
    LPVOID ep = GetProcAddress(handle_, name.c_str());
    return ep;
  }

  void unload_library() {
    if (handle_ == nullptr) return;
    FreeLibrary(handle_);
    handle_ = nullptr;
  }
  bool is_loaded() const { return handle_ != nullptr; }
  boost::filesystem::path get_file() const { return module_; }
  std::string get_filename() const { return module_.filename().string(); }
  std::string get_module_name() const {
    std::string ext = ".dll";
    std::string::size_type l = ext.length();
    std::string fn = get_filename();
    if ((fn.length() > l) && (fn.substr(fn.size() - l) == ext)) return fn.substr(0, fn.size() - l);
    return fn;
  }
};
}  // namespace win32
}  // namespace dll