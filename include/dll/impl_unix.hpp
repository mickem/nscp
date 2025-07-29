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

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>
#include <error/error.hpp>
#include <file_helpers.hpp>

namespace dll {
namespace iunix {
class impl : public boost::noncopyable {
 private:
  boost::filesystem::path module_;
  void* handle_;

 public:
  impl(boost::filesystem::path module) : module_(module), handle_(NULL) {
    if (!boost::filesystem::is_regular_file(module)) {
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
    mod = mod.parent_path() / boost::filesystem::path(std::string("lib") + file_helpers::meta::get_filename(mod));
    if (boost::filesystem::is_regular_file(mod)) return mod;
    return module;
  }
  static std::string get_extension() { return ".so"; }

  static bool is_module(std::string file) { return boost::ends_with(file, get_extension()); }

  void load_library() {
    std::string dllname = module_.string();
    //                handle_ = dlopen(dllname.c_str(), RTLD_GLOBAL | RTLD_NOW);
    handle_ = dlopen(dllname.c_str(), RTLD_NOW);
    if (handle_ == NULL) throw dll_exception(std::string("Could not load library: ") + dlerror() + ": " + module_.string());
  }
  void* load_proc(std::string name) {
    if (handle_ == NULL) throw dll_exception("Failed to load process from module: " + module_.string());
    void* ep = NULL;
    ep = (void*)dlsym(handle_, name.c_str());
    return ep;
  }

  void unload_library() { dlclose(handle_); }

  bool is_loaded() const { return handle_ != NULL; }
  boost::filesystem::path get_file() const { return module_; }
  std::string get_filename() const { return file_helpers::meta::get_filename(module_); }
  std::string get_module_name() {
    std::string ext = get_extension();
    std::size_t l = ext.length();
    std::string fn = get_filename();
    if ((fn.length() > l) && (fn.substr(fn.size() - l) == ext)) return fn.substr(0, fn.size() - l);
    return fn;
  }
};
}  // namespace iunix
}  // namespace dll