// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "dotnet_host.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <cstdlib>
#include <sstream>
#include <str/utf8.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = boost::filesystem;

namespace dotnet {

// --- hostfxr ABI (from dotnet/runtime src/native/corehost/hostfxr.h, MIT) -----
// Declared here rather than including the SDK header so the module builds with
// no .NET SDK present: the runtime is a run-time dependency only.
#ifdef _WIN32
typedef wchar_t char_t;
#else
typedef char char_t;
#endif
typedef void *hostfxr_handle;
struct hostfxr_initialize_parameters {
  size_t size;
  const char_t *host_path;
  const char_t *dotnet_root;
};
enum hostfxr_delegate_type {
  hdt_com_activation,
  hdt_load_in_memory_assembly,
  hdt_winrt_activation,
  hdt_com_register,
  hdt_com_unregister,
  hdt_load_assembly_and_get_function_pointer,
  hdt_get_function_pointer,
};
typedef std::int32_t (*hostfxr_initialize_for_runtime_config_fn)(const char_t *runtime_config_path, const hostfxr_initialize_parameters *parameters,
                                                                 hostfxr_handle *host_context_handle);
typedef std::int32_t (*hostfxr_get_runtime_delegate_fn)(hostfxr_handle host_context_handle, hostfxr_delegate_type type, void **delegate);
typedef std::int32_t (*hostfxr_close_fn)(hostfxr_handle host_context_handle);
typedef void (*hostfxr_error_writer_fn)(const char_t *message);
typedef hostfxr_error_writer_fn (*hostfxr_set_error_writer_fn)(hostfxr_error_writer_fn error_writer);
typedef std::int32_t (*load_assembly_and_get_function_pointer_fn)(const char_t *assembly_path, const char_t *type_name, const char_t *method_name,
                                                                  const char_t *delegate_type_name, void *reserved, void **delegate);
#define NSCP_UNMANAGEDCALLERSONLY_METHOD ((const char_t *)-1)

namespace {

std::basic_string<char_t> to_host(const std::string &utf8) {
#ifdef _WIN32
  return utf8::cvt<std::wstring>(utf8);
#else
  return utf8;
#endif
}
std::string from_host(const char_t *text) {
  if (text == nullptr) return "";
#ifdef _WIN32
  return utf8::cvt<std::string>(std::wstring(text));
#else
  return std::string(text);
#endif
}

// hostfxr reports the reason for a failure through a process-wide error writer
// callback; collect it so the caller can log something better than a hex code.
std::string g_error_text;
void error_writer(const char_t *message) {
  if (!g_error_text.empty()) g_error_text += "\n";
  g_error_text += from_host(message);
}

std::string hex(std::int32_t rc) {
  std::ostringstream ss;
  ss << "0x" << std::hex << static_cast<std::uint32_t>(rc);
  return ss.str();
}

std::string explain_rc(std::int32_t rc) {
  switch (static_cast<std::uint32_t>(rc)) {
    case 0x80008081:
      return "InvalidArgFailure";
    case 0x80008083:
      return "CoreHostLibMissingFailure (hostpolicy library not found next to the runtime)";
    case 0x80008092:
      return "InvalidConfigFile (the runtimeconfig.json could not be read)";
    case 0x80008093:
      return "AppArgNotRunnable";
    case 0x80008096:
      return "FrameworkMissingFailure (the .NET runtime version required by the runtimeconfig.json is not installed)";
    case 0x800080a1:
      return "HostApiUnsupportedVersion";
    case 0x800080a3:
      return "HostInvalidState (the runtime in this process was started in an incompatible way)";
    case 0x800080a5:
      return "CoreHostIncompatibleConfig (another runtime configuration is already active in this process)";
    default:
      return "";
  }
}

void *open_library(const fs::path &path, std::string &error) {
#ifdef _WIN32
  HMODULE h = LoadLibraryW(path.wstring().c_str());
  if (h == nullptr) error = "LoadLibrary failed with error " + std::to_string(GetLastError());
  return reinterpret_cast<void *>(h);
#else
  void *h = dlopen(path.string().c_str(), RTLD_LAZY | RTLD_LOCAL);
  if (h == nullptr) {
    const char *why = dlerror();
    error = why ? why : "dlopen failed";
  }
  return h;
#endif
}
void *find_symbol(void *library, const char *name) {
#ifdef _WIN32
  return reinterpret_cast<void *>(GetProcAddress(reinterpret_cast<HMODULE>(library), name));
#else
  return dlsym(library, name);
#endif
}

std::string getenv_utf8(const char *name) {
#ifdef _WIN32
  std::wstring wname = utf8::cvt<std::wstring>(std::string(name));
  DWORD len = GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
  if (len == 0) return "";
  std::wstring value(len, L'\0');
  len = GetEnvironmentVariableW(wname.c_str(), &value[0], len);
  value.resize(len);
  return utf8::cvt<std::string>(value);
#else
  const char *v = std::getenv(name);
  return v ? v : "";
#endif
}

void push_unique(std::vector<fs::path> &roots, const fs::path &candidate) {
  if (candidate.empty()) return;
  for (const fs::path &p : roots) {
    if (p == candidate) return;
  }
  roots.push_back(candidate);
}

// The dotnet launcher on PATH lives in the root of its install (or is a symlink
// into it, as with /usr/bin/dotnet -> /usr/lib/dotnet/dotnet).
fs::path root_from_path_launcher() {
  const std::string path = getenv_utf8("PATH");
  if (path.empty()) return fs::path();
#ifdef _WIN32
  const char separator = ';';
  const char *launcher = "dotnet.exe";
#else
  const char separator = ':';
  const char *launcher = "dotnet";
#endif
  std::vector<std::string> dirs;
  boost::split(dirs, path, boost::is_any_of(std::string(1, separator)));
  for (const std::string &dir : dirs) {
    if (dir.empty()) continue;
    try {
      fs::path candidate = fs::path(utf8::cvt<std::string>(dir)) / launcher;
      if (!fs::exists(candidate)) continue;
      boost::system::error_code ec;
      fs::path resolved = fs::canonical(candidate, ec);
      if (ec) resolved = candidate;
      return resolved.parent_path();
    } catch (const std::exception &) {
      // Unreadable PATH entry: skip it.
    }
  }
  return fs::path();
}

#ifdef _WIN32
fs::path root_from_registry() {
#if defined(_M_X64) || defined(__x86_64__)
  const wchar_t *arch = L"x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
  const wchar_t *arch = L"arm64";
#else
  const wchar_t *arch = L"x86";
#endif
  std::wstring key_path = std::wstring(L"SOFTWARE\\dotnet\\Setup\\InstalledVersions\\") + arch;
  HKEY key = nullptr;
  // The installer writes this key in the 32-bit registry view regardless of architecture.
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key_path.c_str(), 0, KEY_READ | KEY_WOW64_32KEY, &key) != ERROR_SUCCESS) return fs::path();
  wchar_t buffer[MAX_PATH * 2] = {0};
  DWORD size = sizeof(buffer) - sizeof(wchar_t);
  DWORD type = 0;
  LSTATUS rc = RegQueryValueExW(key, L"InstallLocation", nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &size);
  RegCloseKey(key);
  if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return fs::path();
  return fs::path(std::wstring(buffer));
}
#endif

}  // namespace

// --- version folders --------------------------------------------------------

bool parse_version(const std::string &text, version &out) {
  out = version();
  out.text = text;
  std::string numbers = text;
  const std::string::size_type dash = text.find('-');
  if (dash != std::string::npos) {
    numbers = text.substr(0, dash);
    out.prerelease = text.substr(dash + 1);
  }
  if (numbers.empty()) return false;
  std::vector<std::string> parts;
  boost::split(parts, numbers, boost::is_any_of("."));
  for (const std::string &part : parts) {
    if (part.empty()) return false;
    for (char c : part) {
      if (c < '0' || c > '9') return false;
    }
    try {
      out.parts.push_back(std::stoi(part));
    } catch (const std::exception &) {
      return false;
    }
  }
  return true;
}

bool version::operator<(const version &other) const {
  const std::size_t n = std::max(parts.size(), other.parts.size());
  for (std::size_t i = 0; i < n; ++i) {
    const int a = i < parts.size() ? parts[i] : 0;
    const int b = i < other.parts.size() ? other.parts[i] : 0;
    if (a != b) return a < b;
  }
  // Same number: a release is newer than any prerelease of it.
  if (prerelease.empty() != other.prerelease.empty()) return !prerelease.empty();
  return prerelease < other.prerelease;
}

std::string hostfxr_library_name() {
#if defined(_WIN32)
  return "hostfxr.dll";
#elif defined(__APPLE__)
  return "libhostfxr.dylib";
#else
  return "libhostfxr.so";
#endif
}

std::vector<fs::path> default_roots(const std::string &override_root) {
  std::vector<fs::path> roots;
  push_unique(roots, override_root.empty() ? fs::path() : fs::path(override_root));
  push_unique(roots, fs::path(getenv_utf8("DOTNET_ROOT")));
  push_unique(roots, root_from_path_launcher());
#ifdef _WIN32
  push_unique(roots, root_from_registry());
  const std::string program_files = getenv_utf8("ProgramFiles");
  if (!program_files.empty()) push_unique(roots, fs::path(program_files) / "dotnet");
  const std::string local_app_data = getenv_utf8("LOCALAPPDATA");
  if (!local_app_data.empty()) push_unique(roots, fs::path(local_app_data) / "Microsoft" / "dotnet");
#else
  push_unique(roots, "/usr/lib/dotnet");
  push_unique(roots, "/usr/share/dotnet");
  push_unique(roots, "/usr/lib64/dotnet");
  push_unique(roots, "/usr/local/share/dotnet");
  push_unique(roots, "/opt/dotnet");
  push_unique(roots, "/opt/homebrew/opt/dotnet/libexec");
  const std::string home = getenv_utf8("HOME");
  if (!home.empty()) push_unique(roots, fs::path(home) / ".dotnet");
#endif
  return roots;
}

fs::path find_hostfxr_in_root(const fs::path &root) {
  boost::system::error_code ec;
  const fs::path fxr = root / "host" / "fxr";
  if (!fs::is_directory(fxr, ec)) return fs::path();
  const std::string library = hostfxr_library_name();
  bool have_best = false;
  version best;
  fs::path best_path;
  for (fs::directory_iterator it(fxr, ec), end; !ec && it != end; it.increment(ec)) {
    version candidate;
    if (!parse_version(it->path().filename().string(), candidate)) continue;
    const fs::path candidate_path = it->path() / library;
    if (!fs::is_regular_file(candidate_path, ec)) continue;
    if (!have_best || best < candidate) {
      best = candidate;
      best_path = candidate_path;
      have_best = true;
    }
  }
  return best_path;
}

hostfxr_location find_hostfxr(const std::vector<fs::path> &roots) {
  hostfxr_location result;
  for (const fs::path &root : roots) {
    result.searched.push_back(root.string());
    const fs::path library = find_hostfxr_in_root(root);
    if (!library.empty()) {
      result.library = library;
      result.root = root;
      return result;
    }
  }
  return result;
}

// --- the runtime ------------------------------------------------------------

std::shared_ptr<host> host::instance() {
  // Intentionally leaked: the runtime cannot be unloaded from a process, and the
  // module that loaded it may be unloaded and loaded again (reload), so the host
  // must outlive any one module instance.
  static std::shared_ptr<host> *singleton = new std::shared_ptr<host>(new host());
  return *singleton;
}

std::string host::take_error_text() {
  std::string text = g_error_text;
  g_error_text.clear();
  return text;
}

bool host::initialize(const hostfxr_location &location, const fs::path &runtimeconfig, std::string &error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (load_assembly_and_get_function_pointer_ != nullptr) return true;
  if (!location.found()) {
    error = "No .NET runtime found";
    return false;
  }
  if (library_ == nullptr) {
    library_ = open_library(location.library, error);
    if (library_ == nullptr) {
      error = "Failed to load " + location.library.string() + ": " + error;
      return false;
    }
  }
  auto init = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(find_symbol(library_, "hostfxr_initialize_for_runtime_config"));
  auto get_delegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(find_symbol(library_, "hostfxr_get_runtime_delegate"));
  auto close = reinterpret_cast<hostfxr_close_fn>(find_symbol(library_, "hostfxr_close"));
  auto set_error_writer = reinterpret_cast<hostfxr_set_error_writer_fn>(find_symbol(library_, "hostfxr_set_error_writer"));
  if (init == nullptr || get_delegate == nullptr || close == nullptr) {
    error = location.library.string() + " does not export the hostfxr 3.0 hosting API (a .NET Core 3.0+ / .NET 5+ runtime is required)";
    return false;
  }
  if (set_error_writer) set_error_writer(&error_writer);
  g_error_text.clear();

  const std::basic_string<char_t> config = to_host(runtimeconfig.string());
  const std::basic_string<char_t> root = to_host(location.root.string());
  hostfxr_initialize_parameters parameters;
  parameters.size = sizeof(parameters);
  parameters.host_path = nullptr;
  parameters.dotnet_root = root.c_str();

  hostfxr_handle context = nullptr;
  std::int32_t rc = init(config.c_str(), &parameters, &context);
  // 0 = Success, 1 = Success_HostAlreadyInitialized, 2 = Success_DifferentRuntimeProperties.
  if (rc < 0 || context == nullptr) {
    error = "hostfxr_initialize_for_runtime_config(" + runtimeconfig.string() + ") failed: " + hex(rc);
    const std::string reason = explain_rc(rc);
    if (!reason.empty()) error += " " + reason;
    const std::string text = take_error_text();
    if (!text.empty()) error += ": " + text;
    if (set_error_writer) set_error_writer(nullptr);
    return false;
  }
  void *delegate = nullptr;
  rc = get_delegate(context, hdt_load_assembly_and_get_function_pointer, &delegate);
  // The delegate stays valid after the context is closed; the runtime itself
  // stays loaded for the life of the process.
  close(context);
  if (rc < 0 || delegate == nullptr) {
    error = "hostfxr_get_runtime_delegate failed: " + hex(rc);
    const std::string text = take_error_text();
    if (!text.empty()) error += ": " + text;
    if (set_error_writer) set_error_writer(nullptr);
    return false;
  }
  if (set_error_writer) set_error_writer(nullptr);
  load_assembly_and_get_function_pointer_ = delegate;
  location_ = location;
  return true;
}

void *host::get_function(const fs::path &assembly_path, const std::string &type_name, const std::string &method_name, std::string &error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (load_assembly_and_get_function_pointer_ == nullptr) {
    error = "The .NET runtime is not initialized";
    return nullptr;
  }
  auto set_error_writer = reinterpret_cast<hostfxr_set_error_writer_fn>(find_symbol(library_, "hostfxr_set_error_writer"));
  if (set_error_writer) set_error_writer(&error_writer);
  g_error_text.clear();
  auto load = reinterpret_cast<load_assembly_and_get_function_pointer_fn>(load_assembly_and_get_function_pointer_);
  const std::basic_string<char_t> assembly = to_host(assembly_path.string());
  const std::basic_string<char_t> type = to_host(type_name);
  const std::basic_string<char_t> method = to_host(method_name);
  void *fn = nullptr;
  const std::int32_t rc = load(assembly.c_str(), type.c_str(), method.c_str(), NSCP_UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
  if (set_error_writer) set_error_writer(nullptr);
  if (rc < 0 || fn == nullptr) {
    error = "Failed to resolve " + type_name + "." + method_name + " in " + assembly_path.string() + ": " + hex(rc);
    const std::string text = take_error_text();
    if (!text.empty()) error += ": " + text;
    return nullptr;
  }
  return fn;
}

std::string host::describe() const {
  if (location_.library.empty()) return "not loaded";
  return location_.library.string();
}

}  // namespace dotnet
