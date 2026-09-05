// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Hosting the .NET runtime from native code through hostfxr, the runtime's
// official native hosting API (https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md).
// This is what replaced the old C++/CLI (/clr) build of the module: no managed
// compiler is involved on the C++ side, so it builds with any toolchain and on
// any platform the runtime supports; the runtime is located and loaded at run
// time.

#include <boost/filesystem/path.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace dotnet {

// A parsed runtime/hostfxr version folder name such as 8.0.30 or 9.0.0-preview.3.
struct version {
  std::vector<int> parts;
  std::string prerelease;
  std::string text;

  // Numeric parts first; a release beats a prerelease of the same number.
  bool operator<(const version &other) const;
};

// Parse a version folder name. Returns false when it does not look like one.
bool parse_version(const std::string &text, version &out);

// Platform file name of the hostfxr library.
std::string hostfxr_library_name();

// CPU architecture a binary is built for. A 32-bit nscp cannot load an x64
// hostfxr (LoadLibrary error 193), so candidates are checked against the
// running process before being accepted.
enum class architecture { unknown, x86, x64, arm, arm64 };
architecture process_architecture();
const char *architecture_name(architecture arch);
// Read the machine type from a PE (Windows) or ELF header. `unknown` for
// anything else (Mach-O, unreadable, stub files), which is then accepted.
architecture library_architecture(const boost::filesystem::path &library);

// Well-known places a .NET install may live, most specific first: an explicit
// override, the architecture-specific DOTNET_ROOT_<ARCH> then DOTNET_ROOT
// environment variables, the registered / default install locations, and last
// the folder the `dotnet` launcher on PATH lives in (which may be another
// architecture's install, hence last).
std::vector<boost::filesystem::path> default_roots(const std::string &override_root);

// Locate <root>/host/fxr/<newest version>/<hostfxr library> built for this
// process' architecture. Empty when absent.
boost::filesystem::path find_hostfxr_in_root(const boost::filesystem::path &root);

struct hostfxr_location {
  boost::filesystem::path library;  // full path of the hostfxr library
  boost::filesystem::path root;     // the dotnet root it was found in
  std::vector<std::string> searched;
  bool found() const { return !library.empty(); }
};

// Try the roots in order and return the first hostfxr found.
hostfxr_location find_hostfxr(const std::vector<boost::filesystem::path> &roots);

// A loaded runtime. There can be only one runtime per process, and it cannot be
// unloaded, so this is a process-wide singleton that survives module reloads.
class host {
 public:
  static std::shared_ptr<host> instance();

  // Load hostfxr from `location` and initialize the runtime for the given
  // runtimeconfig.json. Safe to call repeatedly; later calls reuse the running
  // runtime. Returns false and fills `error` when hosting fails.
  bool initialize(const hostfxr_location &location, const boost::filesystem::path &runtimeconfig, std::string &error);

  bool initialized() const { return load_assembly_and_get_function_pointer_ != nullptr; }

  // Resolve an [UnmanagedCallersOnly] static method of `type_name` in the
  // assembly at `assembly_path`. Returns NULL and fills `error` on failure.
  void *get_function(const boost::filesystem::path &assembly_path, const std::string &type_name, const std::string &method_name, std::string &error);

  // Human readable description of the runtime in use (for log lines).
  std::string describe() const;

 private:
  host() = default;
  static std::string take_error_text();

  std::mutex mutex_;
  void *library_ = nullptr;
  void *context_ = nullptr;
  void *load_assembly_and_get_function_pointer_ = nullptr;
  hostfxr_location location_;
};

}  // namespace dotnet
