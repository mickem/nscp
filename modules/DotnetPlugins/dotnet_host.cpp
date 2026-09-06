// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "dotnet_host.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <str/utf8.hpp>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX  // windows.h's min/max macros would break std::max below
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// The runtime's own native hosting headers (vendored, MIT; see hostfxr/README.md).
// They carry the calling convention of every entry point: HOSTFXR_CALLTYPE for
// hostfxr's exports and CORECLR_DELEGATE_CALLTYPE for the delegates it hands
// back, which differ on 32-bit Windows.
#include "hostfxr/coreclr_delegates.h"
#include "hostfxr/hostfxr.h"

namespace fs = boost::filesystem;

namespace dotnet {

namespace {

std::basic_string<char_t> to_host(const std::string &utf8) {
#ifdef _WIN32
  return utf8::cvt<std::wstring>(utf8);
#else
  return utf8;
#endif
}
std::basic_string<char_t> to_host(const fs::path &path) {
#ifdef _WIN32
  return path.wstring();
#else
  return path.string();
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
void HOSTFXR_CALLTYPE error_writer(const char_t *message) {
  if (!g_error_text.empty()) g_error_text += "\n";
  g_error_text += from_host(message);
}

// Installs the error writer for the duration of one hostfxr call and always
// removes it again, whichever way the call returns.
class error_writer_scope {
 public:
  explicit error_writer_scope(void *set_error_writer) : set_(reinterpret_cast<hostfxr_set_error_writer_fn>(set_error_writer)) {
    g_error_text.clear();
    if (set_) set_(&error_writer);
  }
  ~error_writer_scope() {
    if (set_) set_(nullptr);
  }
  // The text hostfxr wrote, as a ": ..." suffix for an error message.
  std::string suffix() const { return g_error_text.empty() ? std::string() : ": " + g_error_text; }

 private:
  hostfxr_set_error_writer_fn set_;
};

std::string hex(std::int32_t rc) {
  std::ostringstream ss;
  ss << "0x" << std::hex << static_cast<std::uint32_t>(rc);
  return ss.str();
}

std::string explain_rc(std::int32_t rc) {
  switch (static_cast<std::uint32_t>(rc)) {
    case 0x80008081:
      return " InvalidArgFailure";
    case 0x80008083:
      return " CoreHostLibMissingFailure (hostpolicy library not found next to the runtime)";
    case 0x80008092:
      return " InvalidConfigFile (the runtimeconfig.json could not be read)";
    case 0x80008093:
      return " AppArgNotRunnable";
    case 0x80008096:
      return " FrameworkMissingFailure (the .NET runtime version required by the runtimeconfig.json is not installed)";
    case 0x800080a1:
      return " HostApiUnsupportedVersion";
    case 0x800080a3:
      return " HostInvalidState (the runtime in this process was started in an incompatible way)";
    case 0x800080a5:
      return " CoreHostIncompatibleConfig (another runtime configuration is already active in this process)";
    default:
      return "";
  }
}

void *open_library(const fs::path &path, std::string &error) {
#ifdef _WIN32
  HMODULE h = LoadLibraryW(path.wstring().c_str());
  if (h == nullptr) {
    const DWORD code = GetLastError();
    error = "LoadLibrary failed with error " + std::to_string(code);
    if (code == ERROR_BAD_EXE_FORMAT)
      error += " (the library is built for another CPU architecture than this " + std::string(architecture_name(process_architecture())) + " process)";
  }
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
void push_unique_utf8(std::vector<fs::path> &roots, const std::string &utf8) {
  if (!utf8.empty()) push_unique(roots, to_path(utf8));
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
      fs::path candidate = to_path(dir) / launcher;
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
  std::wstring key_path = L"SOFTWARE\\dotnet\\Setup\\InstalledVersions\\" + utf8::cvt<std::wstring>(std::string(architecture_name(process_architecture())));
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
#else
// dotnet-install.sh and the distribution packages record a non-default install
// root in /etc/dotnet/install_location (optionally per architecture), the same
// file the runtime's own host reads.
fs::path root_from_install_location() {
  const char *files[] = {"/etc/dotnet/install_location_", "/etc/dotnet/install_location"};
  for (const char *file : files) {
    std::string name = file;
    if (name.back() == '_') name += architecture_name(process_architecture());
    std::ifstream in(name.c_str());
    std::string line;
    if (in && std::getline(in, line)) {
      boost::trim(line);
      if (!line.empty()) return to_path(line);
    }
  }
  return fs::path();
}
#endif

}  // namespace

// --- paths ---------------------------------------------------------------------

fs::path to_path(const std::string &utf8) {
#ifdef _WIN32
  return fs::path(utf8::cvt<std::wstring>(utf8));
#else
  return fs::path(utf8);
#endif
}

std::string path_to_utf8(const fs::path &path) {
#ifdef _WIN32
  return utf8::cvt<std::string>(path.wstring());
#else
  return path.string();
#endif
}

// --- version folders -----------------------------------------------------------

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
  const std::size_t n = (std::max)(parts.size(), other.parts.size());  // parenthesized: windows.h may define max
  for (std::size_t i = 0; i < n; ++i) {
    const int a = i < parts.size() ? parts[i] : 0;
    const int b = i < other.parts.size() ? other.parts[i] : 0;
    if (a != b) return a < b;
  }
  // Same number: a release is newer than any prerelease of it.
  if (prerelease.empty() != other.prerelease.empty()) return !prerelease.empty();
  return prerelease < other.prerelease;
}

// --- architecture --------------------------------------------------------------

architecture process_architecture() {
#if defined(_M_X64) || defined(__x86_64__)
  return architecture::x64;
#elif defined(_M_ARM64) || defined(__aarch64__)
  return architecture::arm64;
#elif defined(_M_IX86) || defined(__i386__)
  return architecture::x86;
#elif defined(_M_ARM) || defined(__arm__)
  return architecture::arm;
#else
  return architecture::unknown;
#endif
}

const char *architecture_name(architecture arch) {
  switch (arch) {
    case architecture::x86:
      return "x86";
    case architecture::x64:
      return "x64";
    case architecture::arm:
      return "arm";
    case architecture::arm64:
      return "arm64";
    default:
      return "unknown";
  }
}

architecture library_architecture(const fs::path &library) {
  std::ifstream file(library.string().c_str(), std::ios::binary);
  if (!file) return architecture::unknown;
  unsigned char header[64] = {0};
  file.read(reinterpret_cast<char *>(header), sizeof(header));
  if (file.gcount() < 20) return architecture::unknown;
  auto u16 = [](const unsigned char *p, bool little_endian) -> unsigned { return little_endian ? (p[0] | (p[1] << 8)) : (p[1] | (p[0] << 8)); };
  if (header[0] == 0x7f && header[1] == 'E' && header[2] == 'L' && header[3] == 'F') {
    const bool little_endian = header[5] == 1;
    switch (u16(header + 18, little_endian)) {
      case 3:
        return architecture::x86;
      case 62:
        return architecture::x64;
      case 40:
        return architecture::arm;
      case 183:
        return architecture::arm64;
      default:
        return architecture::unknown;
    }
  }
  if (header[0] == 'M' && header[1] == 'Z') {
    const std::uint32_t pe_offset = header[0x3c] | (header[0x3d] << 8) | (header[0x3e] << 16) | (static_cast<std::uint32_t>(header[0x3f]) << 24);
    unsigned char pe[6] = {0};
    file.seekg(pe_offset, std::ios::beg);
    file.read(reinterpret_cast<char *>(pe), sizeof(pe));
    if (file.gcount() < 6 || pe[0] != 'P' || pe[1] != 'E' || pe[2] != 0 || pe[3] != 0) return architecture::unknown;
    switch (u16(pe + 4, true)) {
      case 0x014c:
        return architecture::x86;
      case 0x8664:
        return architecture::x64;
      case 0x01c4:
        return architecture::arm;
      case 0xaa64:
        return architecture::arm64;
      default:
        return architecture::unknown;
    }
  }
  return architecture::unknown;
}

// --- locating the runtime ------------------------------------------------------

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
  push_unique_utf8(roots, override_root);
  // The same sources the runtime's own host consults, in its order: the
  // architecture-specific DOTNET_ROOT_<ARCH> (and the legacy DOTNET_ROOT(x86)),
  // then DOTNET_ROOT, then the registered install location, then the platform
  // default folders.
  std::string arch_env = std::string("DOTNET_ROOT_") + architecture_name(process_architecture());
  boost::to_upper(arch_env);
  push_unique_utf8(roots, getenv_utf8(arch_env.c_str()));
  if (process_architecture() == architecture::x86) push_unique_utf8(roots, getenv_utf8("DOTNET_ROOT(x86)"));
  push_unique_utf8(roots, getenv_utf8("DOTNET_ROOT"));
#ifdef _WIN32
  push_unique(roots, root_from_registry());
  // Under WOW64 %ProgramFiles% already resolves to "Program Files (x86)" for a
  // 32-bit process; the explicit variants cover both directions anyway.
  const std::string program_files = getenv_utf8("ProgramFiles");
  if (!program_files.empty()) push_unique(roots, to_path(program_files) / "dotnet");
  const std::string program_files_native = getenv_utf8(process_architecture() == architecture::x86 ? "ProgramFiles(x86)" : "ProgramW6432");
  if (!program_files_native.empty()) push_unique(roots, to_path(program_files_native) / "dotnet");
  const std::string local_app_data = getenv_utf8("LOCALAPPDATA");
  if (!local_app_data.empty()) push_unique(roots, to_path(local_app_data) / "Microsoft" / "dotnet");
  // Last: the launcher on PATH is usually the x64 install, whatever we are.
  push_unique(roots, root_from_path_launcher());
#else
  push_unique(roots, root_from_install_location());
  push_unique(roots, "/usr/lib/dotnet");
  push_unique(roots, "/usr/share/dotnet");
  push_unique(roots, "/usr/lib64/dotnet");
  push_unique(roots, "/usr/local/share/dotnet");
  push_unique(roots, "/opt/dotnet");
  push_unique(roots, "/opt/homebrew/opt/dotnet/libexec");
  const std::string home = getenv_utf8("HOME");
  if (!home.empty()) push_unique(roots, to_path(home) / ".dotnet");
  // Last, as on Windows: the launcher on PATH may belong to another install.
  push_unique(roots, root_from_path_launcher());
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
    if (!parse_version(path_to_utf8(it->path().filename()), candidate)) continue;
    const fs::path candidate_path = it->path() / library;
    // A separate error code: one unreadable version folder must not end the
    // enumeration before the newer folders behind it are seen.
    boost::system::error_code file_ec;
    if (!fs::is_regular_file(candidate_path, file_ec)) continue;
    const architecture arch = library_architecture(candidate_path);
    if (arch != architecture::unknown && arch != process_architecture()) continue;
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
    result.searched.push_back(path_to_utf8(root));
    const fs::path library = find_hostfxr_in_root(root);
    if (!library.empty()) {
      result.library = library;
      result.root = root;
      return result;
    }
  }
  return result;
}

// --- the runtime ---------------------------------------------------------------

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
      error = "Failed to load " + path_to_utf8(location.library) + ": " + error;
      return false;
    }
  }
  auto init = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(find_symbol(library_, "hostfxr_initialize_for_runtime_config"));
  auto get_delegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(find_symbol(library_, "hostfxr_get_runtime_delegate"));
  auto close = reinterpret_cast<hostfxr_close_fn>(find_symbol(library_, "hostfxr_close"));
  set_error_writer_ = find_symbol(library_, "hostfxr_set_error_writer");
  if (init == nullptr || get_delegate == nullptr || close == nullptr) {
    error = path_to_utf8(location.library) + " does not export the hostfxr 3.0 hosting API (a .NET Core 3.0+ / .NET 5+ runtime is required)";
    return false;
  }

  const std::basic_string<char_t> config = to_host(runtimeconfig);
  const std::basic_string<char_t> root = to_host(location.root);
  hostfxr_initialize_parameters parameters;
  parameters.size = sizeof(parameters);
  parameters.host_path = nullptr;
  parameters.dotnet_root = root.c_str();

  error_writer_scope writer(set_error_writer_);
  hostfxr_handle context = nullptr;
  std::int32_t rc = init(config.c_str(), &parameters, &context);
  // 0 = Success, 1 = Success_HostAlreadyInitialized, 2 = Success_DifferentRuntimeProperties.
  if (rc < 0 || context == nullptr) {
    error = "hostfxr_initialize_for_runtime_config(" + path_to_utf8(runtimeconfig) + ") failed: " + hex(rc) + explain_rc(rc) + writer.suffix();
    return false;
  }
  void *delegate = nullptr;
  rc = get_delegate(context, hdt_load_assembly_and_get_function_pointer, &delegate);
  // The delegate stays valid after the context is closed; the runtime itself
  // stays loaded for the life of the process.
  close(context);
  if (rc < 0 || delegate == nullptr) {
    error = "hostfxr_get_runtime_delegate failed: " + hex(rc) + explain_rc(rc) + writer.suffix();
    return false;
  }
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
  error_writer_scope writer(set_error_writer_);
  auto load = reinterpret_cast<load_assembly_and_get_function_pointer_fn>(load_assembly_and_get_function_pointer_);
  const std::basic_string<char_t> assembly = to_host(assembly_path);
  const std::basic_string<char_t> type = to_host(type_name);
  const std::basic_string<char_t> method = to_host(method_name);
  void *fn = nullptr;
  const std::int32_t rc = load(assembly.c_str(), type.c_str(), method.c_str(), UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
  if (rc < 0 || fn == nullptr) {
    error = "Failed to resolve " + type_name + "." + method_name + " in " + path_to_utf8(assembly_path) + ": " + hex(rc) + explain_rc(rc) + writer.suffix();
    return nullptr;
  }
  return fn;
}

std::string host::describe() const {
  if (location_.library.empty()) return "not loaded";
  return path_to_utf8(location_.library);
}

}  // namespace dotnet
