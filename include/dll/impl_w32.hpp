// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>
#include <error/error.hpp>
#include <str/utf8.hpp>
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
    // LoadLibraryExW with LOAD_LIBRARY_SEARCH_DEFAULT_DIRS, not LoadLibrary.
    //
    // A plain LoadLibrary resolves the module's own dependencies through the
    // legacy search order, which includes the process's current directory and
    // then PATH. An administrator running `nscp client` or `nscp test` from a
    // user-writable folder therefore let whoever could write there decide
    // which DLL a module's imports resolved to. The DEFAULT_DIRS search set is
    // the application directory, %WINDIR%\System32 and any directory added
    // with AddDllDirectory - no CWD, no PATH - and adding the module's own
    // directory covers plugins that ship their dependencies beside themselves.
    //
    // Resolved at run time: the flags need Windows 8 (or KB2533623 on 7), and
    // the XP toolset has neither the constants nor the entry point. Where they
    // are missing the call falls back to the old behaviour, which is what that
    // platform had anyway.
    typedef HMODULE(WINAPI * load_library_ex_w_t)(LPCWSTR, HANDLE, DWORD);
    static const DWORD kSearchDefaultDirs = 0x00001000;      // LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
    static const DWORD kSearchDllLoadDir = 0x00000100;       // LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
    const HMODULE kernel = ::GetModuleHandleW(L"kernel32.dll");
    const auto load_library_ex_w = kernel == nullptr ? nullptr : reinterpret_cast<load_library_ex_w_t>(::GetProcAddress(kernel, "LoadLibraryExW"));
    const auto set_default_dll_directories =
        kernel == nullptr ? nullptr : reinterpret_cast<BOOL(WINAPI *)(DWORD)>(::GetProcAddress(kernel, "SetDefaultDllDirectories"));
    if (load_library_ex_w != nullptr && set_default_dll_directories != nullptr) {
      set_default_dll_directories(kSearchDefaultDirs);
      // The LOAD_LIBRARY_SEARCH_* flags take a fully qualified path and
      // nothing else. Refusing to resolve a relative name against the current
      // directory is the point of them, but LoadLibraryExW does not then fall
      // back to anything - it simply fails - and callers do name modules
      // relatively (`modules/CheckHelpers.dll`). So the path is completed
      // here, and the separators made native, because the same flags reject a
      // path containing forward slashes.
      boost::filesystem::path target = module_;
      try {
        if (!target.is_absolute()) target = boost::filesystem::absolute(target);
        target.make_preferred();
      } catch (const std::exception &) {
        // current_path() failed; the unqualified path will fail the load and
        // report its own error, which is better than throwing from here.
        target = module_;
      }
      handle_ = load_library_ex_w(target.native().c_str(), nullptr, kSearchDefaultDirs | kSearchDllLoadDir);
    } else {
      handle_ = LoadLibrary(module_.native().c_str());
    }
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