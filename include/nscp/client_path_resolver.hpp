// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <config.h>

#include <boost/filesystem.hpp>
#include <map>
#include <string>

#include <nscp/boot_layout.hpp>
#include <nscp/path_defaults.hpp>

#ifdef WIN32
#include <win/shellapi.hpp>
#else
#include <unistd.h>
#endif

// Path resolution for the standalone clients (check_nrpe, check_nscp).
//
// These run without a core, and therefore without path_manager, yet they must
// resolve ${...} tokens - above all ${shared-path} / ${certificate-path} - to
// the same place the service does. A client that disagrees looks for its
// certificates in a folder the service never wrote to, and the TLS handshake
// fails.
//
// So it mirrors the service's resolution, for the inputs a client actually has:
//
//   boot.ini [paths] override  >  the binary's own lookups  >  compile defaults
//
// (the service has a further CLI --path-override layer on top; the clients have
// no such flag). Both the [layout] mode and the [paths] overrides come from the
// one boot.ini next to the executable, read once - that is what keeps a client
// in step with the service, which applies the very same two sections.
//
// This lives in one place, shared by both clients, precisely so the two cannot
// drift: before this existed each client carried its own byte-for-byte copy of
// the resolver, and one of them (the [paths] handling) was missing from both.
namespace nscp {
namespace paths {

class client_path_resolver {
  layout layout_;
  std::map<std::string, std::string> overrides_;

 public:
  // Reads boot.ini once. A missing or unreadable file means the legacy layout
  // with no overrides, which is what every installation predating these
  // settings has.
  explicit client_path_resolver(const boost::filesystem::path &boot_ini)
      : layout_(layout_from_boot_ini_file(boot_ini.string())), overrides_(path_overrides_from_boot_ini_file(boot_ini.string())) {}

  // The directory the running executable lives in.
  static boost::filesystem::path executable_dir() {
#ifdef WIN32
    return shellapi::get_module_file_name();
#else
    char buff[1024];
    const ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
    if (len == -1) return boost::filesystem::initial_path();
    buff[len] = '\0';
    return boost::filesystem::path(std::string(buff)).parent_path();
#endif
  }

  std::string get_folder(const std::string &key) const {
    // boot.ini [paths] wins, exactly as it does for the service - and it wins
    // over the binary's own lookups too, so an operator can relocate even
    // ${exe-path} if they must.
    const auto it = overrides_.find(key);
    if (it != overrides_.end()) return it->second;

    // Lookups only this binary can answer, about its own location.
    if (key == "base-path" || key == "exe-path") return executable_dir().string();
    if (key == "temp") return temp_dir();
#ifdef WIN32
    if (key == "common-appdata") return shellapi::get_special_folder_path(CSIDL_COMMON_APPDATA, executable_dir()).string();
#endif

    // Everything else comes from the table the service uses, so the two cannot
    // drift apart - including ${shared-path}, which the layout moves.
    const std::string def = default_for(key, layout_);
    if (!def.empty()) return def;

    // Last resort: the executable's directory, never an empty (and therefore
    // root-relative) path from a typo in a settings file.
    return executable_dir().string();
  }

  std::string expand_path(const std::string &file) const {
    return expand_tokens(file, [this](const std::string &key) { return get_folder(key); });
  }

  layout get_layout() const { return layout_; }

 private:
  static std::string temp_dir() {
#ifdef WIN32
    return shellapi::get_temp_path().string();
#else
    return "/tmp";
#endif
  }
};

}  // namespace paths
}  // namespace nscp
