// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The macOS software sources: installer receipts, application bundles and
// Homebrew, all read from disk in-process (no pkgutil, no brew). The mapping
// of each onto a row is in check_installed_software.cpp and the update cache
// in check_os_updates.cpp; this file only finds and reads the files.

#include <sys/stat.h>

#include <algorithm>
#include <boost/filesystem.hpp>
#include <string>
#include <vector>

#include "check_installed_software.h"
#include "check_os_updates.h"
#include "plist_value.h"

namespace fs = boost::filesystem;

namespace {

long long mtime_of(const fs::path &path) {
  struct stat st;
  if (::stat(path.string().c_str(), &st) != 0) return 0;
  return static_cast<long long>(st.st_mtime);
}

// Directory entries, sorted, skipping anything unreadable. An absent
// directory is simply empty: not every Mac has Homebrew, or anything in
// /Applications/Utilities.
std::vector<fs::path> list_dir(const fs::path &dir) {
  std::vector<fs::path> out;
  boost::system::error_code ec;
  fs::directory_iterator it(dir, ec), end;
  if (ec) return out;
  for (; it != end; it.increment(ec)) {
    if (ec) break;
    out.push_back(it->path());
  }
  std::sort(out.begin(), out.end());
  return out;
}

void read_receipts(std::vector<installed_software::software_entry> &out) {
  for (const fs::path &file : list_dir("/var/db/receipts")) {
    if (file.extension() != ".plist") continue;
    installed_software::software_entry e = installed_software::receipt_entry(plist::read_file(file.string()));
    if (!e.name.empty()) out.push_back(e);
  }
}

void read_bundles(const fs::path &dir, std::vector<installed_software::software_entry> &out) {
  for (const fs::path &bundle : list_dir(dir)) {
    if (bundle.extension() != ".app") continue;
    const plist::value info = plist::read_file((bundle / "Contents" / "Info.plist").string());
    // A bundle without a readable Info.plist is still installed software.
    out.push_back(installed_software::bundle_entry(bundle.stem().string(), info, mtime_of(bundle)));
  }
}

// A formula or cask directory holds one directory per installed version. The
// linked one - the target of <prefix>/opt/<name> for a formula - is the one in
// use; otherwise the highest.
void read_kegs(const fs::path &root, const fs::path &opt, std::vector<installed_software::software_entry> &out) {
  for (const fs::path &keg : list_dir(root)) {
    const std::vector<fs::path> versions = list_dir(keg);
    if (versions.empty()) continue;
    fs::path chosen = versions.back();
    boost::system::error_code ec;
    const fs::path linked = fs::read_symlink(opt / keg.filename(), ec);
    if (!ec) {
      for (const fs::path &v : versions) {
        if (v.filename() == linked.filename()) chosen = v;
      }
    }
    installed_software::software_entry e;
    e.manager = "homebrew";
    e.name = keg.filename().string();
    e.version = chosen.filename().string();
    e.install_date_epoch = mtime_of(chosen);
    e.install_date_str = installed_software::format_epoch_date(e.install_date_epoch);
    out.push_back(e);
  }
}

}  // namespace

installed_software::fetch_result installed_software::fetch_macos_inventory() {
  fetch_result result;
  read_receipts(result.entries);
  read_bundles("/Applications", result.entries);
  read_bundles("/Applications/Utilities", result.entries);
  // Apple silicon installs Homebrew under /opt/homebrew, Intel under
  // /usr/local.
  for (const char *prefix : {"/opt/homebrew", "/usr/local"}) {
    const fs::path base(prefix);
    read_kegs(base / "Cellar", base / "opt", result.entries);
    read_kegs(base / "Caskroom", base / "opt", result.entries);
  }
  // Every Mac has receipts; none at all means /var/db/receipts could not be
  // read, which must not be published as an empty inventory.
  result.ok = !result.entries.empty();
  return result;
}

plist::value os_updates::read_software_update_cache() { return plist::read_file("/Library/Preferences/com.apple.SoftwareUpdate.plist"); }
