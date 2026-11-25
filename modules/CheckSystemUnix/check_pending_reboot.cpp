// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_pending_reboot.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <check/duration_keyword.hpp>
#include <cstring>
#include <ctime>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <set>
#include <str/ascii.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>

namespace po = boost::program_options;

namespace pending_reboot_check {

using str::ascii::is_alnum;
using str::ascii::is_alpha;
using str::ascii::is_digit;

namespace {

// How many package names the reasons line spells out before summarising.
const std::size_t kReasonPackages = 5;
// Bounds on what is read from the marker's package list.
const std::size_t kMaxPackagesBytes = 64 * 1024;
const std::size_t kMaxPackages = 256;
const std::size_t kMaxPackageName = 128;
// Bounds on what is read while looking for kernels and container markers.
const std::size_t kMaxDirEntries = 4096;
const std::size_t kMaxProcBytes = 256 * 1024;
const std::size_t kMaxReleaseLength = 128;

// The directories a distribution installs kernel modules (and, on the RHEL
// family, Fedora and Arch, the kernel image itself) into. On a merged-/usr
// system they are the same directory.
const char *const kModuleDirs[] = {"/lib/modules", "/usr/lib/modules"};

// RPM architectures, the last dot-separated field of an RPM-style release.
const char *const kRpmArches[] = {"x86_64", "aarch64", "ppc64le", "ppc64", "s390x", "i686", "i586", "armv7hl", "riscv64", "loongarch64"};

// The outcome of stat(): there, not there (ENOENT/ENOTDIR), or could not tell.
enum class probe_state { missing, exists, error };
struct probe_result {
  probe_state state;
  struct stat st;
  int error_number;
};

probe_result probe(const std::string &path) {
  probe_result r;
  std::memset(&r.st, 0, sizeof(r.st));
  r.error_number = 0;
  if (::stat(path.c_str(), &r.st) == 0) {
    r.state = probe_state::exists;
  } else {
    r.error_number = errno;
    r.state = (r.error_number == ENOENT || r.error_number == ENOTDIR) ? probe_state::missing : probe_state::error;
  }
  return r;
}

std::string describe_error(const std::string &what, const std::string &path, const int error_number) {
  return "cannot " + what + " " + path + ": " + std::strerror(error_number);
}

// At most `limit` bytes of a file. `truncated` is set when there was more.
// Returns false, with errno in *error_number, when it could not be read.
bool read_bounded(const std::string &path, const std::size_t limit, std::string &out, bool &truncated, int *error_number) {
  out.clear();
  truncated = false;
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    if (error_number != nullptr) *error_number = errno;
    return false;
  }
  char buf[4096];
  bool ok = true;
  for (;;) {
    const ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) {
      if (error_number != nullptr) *error_number = errno;
      ok = false;
      break;
    }
    if (n == 0) break;
    out.append(buf, static_cast<std::size_t>(n));
    if (out.size() > limit) {
      out.resize(limit);
      truncated = true;
      break;
    }
  }
  ::close(fd);
  return ok;
}

std::string read_optional(const std::string &path, const std::size_t limit) {
  std::string out;
  bool truncated = false;
  if (!read_bounded(path, limit, out, truncated, nullptr)) return "";
  return out;
}

bool is_package_char(const char c) { return is_alnum(c) || c == '+' || c == '.' || c == ':' || c == '~' || c == '_' || c == '-'; }

std::string trim_ascii(const std::string &s) {
  const std::string::size_type b = s.find_first_not_of(" \t\r");
  if (b == std::string::npos) return "";
  const std::string::size_type e = s.find_last_not_of(" \t\r");
  return s.substr(b, e - b + 1);
}

// Split on one separator, keeping empty fields.
std::vector<std::string> split(const std::string &s, const char sep) {
  std::vector<std::string> out;
  std::string::size_type start = 0;
  for (;;) {
    const std::string::size_type at = s.find(sep, start);
    out.push_back(s.substr(start, at == std::string::npos ? std::string::npos : at - start));
    if (at == std::string::npos) break;
    start = at + 1;
  }
  return out;
}

// The runs of letters in a token, concatenated: "rt7" -> "rt", "0_rc3" -> "rc".
std::string letters_of(const std::string &token) {
  std::string out;
  for (const char c : token) {
    if (is_alpha(c)) out += c;
  }
  return out;
}

bool all_digits(const std::string &s) {
  if (s.empty()) return false;
  for (const char c : s) {
    if (!is_digit(c)) return false;
  }
  return true;
}

// "el9_4" -> family "el", rest "" ; "el8uek" -> "el", "uek"; "fc41" -> "fc";
// "amzn2023" -> "amzn". False when the token is not a dist tag (a family
// immediately followed by its release number).
bool split_dist_tag(const std::string &token, std::string &family, std::string &rest) {
  static const char *const families[] = {"el", "fc", "amzn"};
  for (const char *f : families) {
    const std::string fam(f);
    if (token.size() <= fam.size() || token.compare(0, fam.size(), fam) != 0 || !is_digit(token[fam.size()])) continue;
    std::string::size_type i = fam.size();
    while (i < token.size() && (is_digit(token[i]) || token[i] == '_')) ++i;
    family = fam;
    rest = letters_of(token.substr(i));
    return true;
  }
  return false;
}

// The oldest non-zero of two times.
long long oldest(const long long a, const long long b) {
  if (a == 0) return b;
  if (b == 0) return a;
  return a < b ? a : b;
}

}  // namespace

// --- reboot_obj ----------------------------------------------------------------

std::string reboot_obj::get_packages() const { return str::format::join(packages, ", "); }

std::string reboot_obj::get_reasons() const {
  std::vector<std::string> parts;
  if (marker) {
    std::string m = (marker_name.empty() ? std::string("reboot-required") : marker_name) + " marker";
    if (!packages.empty()) {
      const std::vector<std::string> shown(packages.begin(), packages.begin() + static_cast<std::ptrdiff_t>(std::min(packages.size(), kReasonPackages)));
      m += " (" + str::format::join(shown, ", ");
      if (packages.size() > kReasonPackages) m += " and " + str::xtos(packages.size() - kReasonPackages) + " more";
      m += ")";
    }
    parts.push_back(m);
  }
  if (kernel) parts.push_back(kernel_reason);
  if (parts.empty()) return "none";
  return str::format::join(parts, ", ");
}

std::string reboot_obj::get_written_s() const {
  if (pending_since == 0) return "unknown";
  return str::format::format_date(static_cast<std::time_t>(pending_since));
}

std::string reboot_obj::get_unreadable() const {
  std::vector<std::string> parts;
  if (marker_unknown && !marker) parts.push_back("the reboot marker could not be read (" + marker_error + ")");
  if (kernel_unknown && !kernel) parts.push_back("the kernel inventory could not be read (" + kernel_error + ")");
  return str::format::join(parts, " and ");
}

std::string reboot_obj::get_message() const {
  const std::string note = get_unreadable();
  if (!any()) return note.empty() ? "No reboot pending" : "No reboot signal is set, but " + note;
  std::string msg = "Reboot required: " + get_reasons();
  if (pending_since != 0) msg += " (pending since " + get_written_s() + ")";
  if (!note.empty()) msg += "; " + note;
  return msg;
}

// --- kernel releases --------------------------------------------------------------

bool plausible_release(const std::string &r) {
  if (r.empty() || r.size() > kMaxReleaseLength || !is_digit(r[0])) return false;
  for (const char c : r) {
    if (!(is_alnum(c) || c == '.' || c == '-' || c == '_' || c == '+' || c == '~')) return false;
  }
  return true;
}

// Not str's version_less() (check_installed_software): that one compares the
// non-digit characters one by one, so "427.13.1.el9" < "427.el9" ('1' < 'e')
// and the separators take part. Kernel releases need rpm's ordering, where
// separators only separate and a numeric segment beats an alphabetic one.
int compare_kernel_versions(const std::string &a, const std::string &b) {
  std::size_t i = 0, j = 0;
  for (;;) {
    while (i < a.size() && !is_alnum(a[i]) && a[i] != '~') ++i;
    while (j < b.size() && !is_alnum(b[j]) && b[j] != '~') ++j;
    const bool tilde_a = i < a.size() && a[i] == '~';
    const bool tilde_b = j < b.size() && b[j] == '~';
    if (tilde_a || tilde_b) {
      // A pre-release marker sorts before anything, the end of the string included.
      if (!tilde_a) return 1;
      if (!tilde_b) return -1;
      ++i;
      ++j;
      continue;
    }
    if (i >= a.size() || j >= b.size()) break;
    const bool digit_a = is_digit(a[i]);
    const bool digit_b = is_digit(b[j]);
    if (digit_a != digit_b) return digit_a ? 1 : -1;
    const std::size_t start_a = i, start_b = j;
    if (digit_a) {
      while (i < a.size() && is_digit(a[i])) ++i;
      while (j < b.size() && is_digit(b[j])) ++j;
      std::size_t za = start_a, zb = start_b;
      while (za + 1 < i && a[za] == '0') ++za;
      while (zb + 1 < j && b[zb] == '0') ++zb;
      const std::size_t len_a = i - za, len_b = j - zb;
      if (len_a != len_b) return len_a < len_b ? -1 : 1;
      const int c = a.compare(za, len_a, b, zb, len_b);
      if (c != 0) return c < 0 ? -1 : 1;
    } else {
      while (i < a.size() && is_alpha(a[i])) ++i;
      while (j < b.size() && is_alpha(b[j])) ++j;
      const int c = a.compare(start_a, i - start_a, b, start_b, j - start_b);
      if (c != 0) return c < 0 ? -1 : 1;
    }
  }
  const bool end_a = i >= a.size();
  const bool end_b = j >= b.size();
  if (end_a && end_b) return 0;
  return end_a ? -1 : 1;
}

std::vector<std::string> split_release(const std::string &release) { return split(release, '-'); }

bool is_build_token(const std::string &token, const bool last) {
  if (token.empty()) return false;
  if (is_digit(token[0])) return all_digits(token) || token.find('.') != std::string::npos;
  // "arch1", "zen1", "hardened1": a line name and its build number. Only when
  // another token follows - the last token is the flavour ("amd64", "v8").
  if (last) return false;
  std::string::size_type i = 0;
  while (i < token.size() && is_alpha(token[i])) ++i;
  return i > 0 && i < token.size() && all_digits(token.substr(i));
}

boost::optional<kernel_line> rpm_kernel_line(const std::string &release) {
  const std::string::size_type dash = release.find('-');
  if (dash == std::string::npos) return boost::none;
  std::string body = release.substr(dash + 1);
  std::string variant;
  const std::string::size_type plus = body.rfind('+');
  if (plus != std::string::npos) {
    variant = body.substr(plus + 1);
    body = body.substr(0, plus);
  }
  const std::string::size_type dot = body.rfind('.');
  if (dot == std::string::npos) return boost::none;
  const std::string arch = body.substr(dot + 1);
  bool known_arch = false;
  for (const char *a : kRpmArches) {
    if (arch == a) known_arch = true;
  }
  if (!known_arch) return boost::none;

  std::string family;
  std::vector<std::string> markers;
  for (const std::string &token : split(body.substr(0, dot), '.')) {
    std::string fam, rest;
    if (split_dist_tag(token, fam, rest)) {
      family = fam;
      if (!rest.empty()) markers.push_back(rest);
      continue;
    }
    const std::string letters = letters_of(token);
    if (!letters.empty()) markers.push_back(letters);
  }
  return kernel_line("rpm:" + arch + "+" + variant + ":" + family + ":" + str::format::join(markers, "."));
}

kernel_line dash_kernel_line(const std::string &release) {
  const std::vector<std::string> tokens = split_release(release);
  std::size_t last_build = 0;
  for (std::size_t k = 1; k < tokens.size(); ++k) {
    if (is_build_token(tokens[k], k + 1 == tokens.size())) last_build = k;
  }
  std::vector<std::string> markers;
  for (std::size_t k = 1; k <= last_build; ++k) {
    const std::string letters = letters_of(tokens[k]);
    if (!letters.empty() && tokens[k].find('.') == std::string::npos) markers.push_back(letters);
  }
  const std::vector<std::string> flavour(tokens.begin() + static_cast<std::ptrdiff_t>(last_build) + 1, tokens.end());
  return kernel_line("deb:" + str::format::join(flavour, "-") + ":" + str::format::join(markers, "."));
}

kernel_line identify_line(const std::string &release) {
  if (!plausible_release(release)) return kernel_line();
  const boost::optional<kernel_line> rpm = rpm_kernel_line(release);
  if (rpm) return rpm.value();
  return dash_kernel_line(release);
}

// --- kernel inventory ----------------------------------------------------------------

kernel_verdict evaluate_kernels(const std::string &running, const kernel_inventory &inventory) {
  kernel_verdict v;
  if (inventory.container) return v;  // the running kernel is the host's, not this root's
  if (!inventory.error.empty()) {
    v.unknown = true;
    v.error = inventory.error;
    return v;
  }
  const kernel_line line = identify_line(running);
  if (!line.placed) return v;  // cannot tell what an update of it would be

  for (const auto &kv : inventory.kernels) {
    if (kv.second.state != kernel_image::present) continue;
    const kernel_line other = identify_line(kv.first);
    if (!other.placed || other.id != line.id) continue;
    if (v.newest.empty() || compare_kernel_versions(kv.first, v.newest) > 0) v.newest = kv.first;
  }

  const auto run = inventory.kernels.find(running);
  // An image lookup that cannot find the running kernel's own image (a
  // /boot that is not mounted, an unreadable one) cannot vouch that a newer
  // modules directory without an image is only a DKMS leftover.
  const bool lookups_reliable = run == inventory.kernels.end() || run->second.state == kernel_image::present;
  std::string newest_newer;
  std::vector<std::string> errors;
  for (const auto &kv : inventory.kernels) {
    if (kv.first == running || compare_kernel_versions(kv.first, running) <= 0) continue;
    const kernel_line other = identify_line(kv.first);
    if (!other.placed || other.id != line.id) continue;
    if (kv.second.state == kernel_image::present) {
      v.pending_since = oldest(v.pending_since, kv.second.installed);
      if (newest_newer.empty() || compare_kernel_versions(kv.first, newest_newer) > 0) newest_newer = kv.first;
    } else if (kv.second.state == kernel_image::unreadable) {
      errors.push_back(kv.second.error);
    } else if (!lookups_reliable) {
      errors.push_back(run->second.state == kernel_image::unreadable ? run->second.error
                                                                     : "no image found for the running kernel " + running + " (is /boot mounted?)");
    }
  }

  if (run == inventory.kernels.end()) {
    // The running kernel's modules are gone: the package manager removed it
    // (pacman does on every upgrade). The next boot uses another kernel.
    if (v.newest.empty()) return v;  // nothing of its line is installed: a foreign kernel (WSL, netboot)
    v.pending = true;
    v.reason = "running kernel " + running + " is no longer installed (newest installed: " + v.newest + ")";
    return v;
  }
  if (!newest_newer.empty()) {
    v.pending = true;
    v.reason = "newer kernel installed (" + newest_newer + ", running " + running + ")";
    return v;
  }
  if (!errors.empty()) {
    v.unknown = true;
    v.error = errors.front();
    v.pending_since = 0;
  }
  return v;
}

std::string root_fs_type(const std::string &mountinfo) {
  std::string fstype;
  for (const std::string &line : split(mountinfo, '\n')) {
    // <id> <parent> <maj:min> <root> <mount point> <options> [optional...] - <fstype> <source> <super options>
    const std::vector<std::string> f = split(line, ' ');
    if (f.size() < 7 || f[4] != "/") continue;
    for (std::size_t k = 6; k + 1 < f.size(); ++k) {
      if (f[k] == "-") {
        fstype = f[k + 1];  // the last mount on / is the one in effect
        break;
      }
    }
  }
  return fstype;
}

bool environ_names_container(const std::string &environ) {
  const std::string key = "container=";
  for (const std::string &var : split(environ, '\0')) {
    if (var.size() > key.size() && var.compare(0, key.size(), key) == 0) return true;
  }
  return false;
}

bool detect_container(const std::string &root) {
  const char *const markers[] = {"/run/systemd/container", "/run/.containerenv", "/.dockerenv"};
  for (const char *m : markers) {
    if (probe(root + m).state == probe_state::exists) return true;
  }
  if (probe(root + "/proc/vz").state == probe_state::exists && probe(root + "/proc/bc").state != probe_state::exists) return true;
  // PID 1's environment is readable by root only; an unprivileged agent
  // relies on the markers above and the root filesystem type.
  if (environ_names_container(read_optional(root + "/proc/1/environ", kMaxProcBytes))) return true;
  const std::string fstype = root_fs_type(read_optional(root + "/proc/self/mountinfo", kMaxProcBytes));
  return fstype == "overlay" || fstype == "aufs" || fstype == "fuse.fuse-overlayfs";
}

kernel_image find_kernel_image(const std::string &root, const std::string &release) {
  std::vector<std::string> paths;
  for (const char *dir : kModuleDirs) paths.push_back(std::string(dir) + "/" + release + "/vmlinuz");
  paths.push_back("/boot/vmlinuz-" + release);
  long long installed = 0;
  bool found = false;
  std::string error;
  for (const std::string &p : paths) {
    const probe_result r = probe(root + p);
    if (r.state == probe_state::exists && S_ISREG(r.st.st_mode)) {
      found = true;
      // ctime: when the image was written to this disk. The mtime is the
      // package's build time (dpkg and rpm preserve it).
      installed = oldest(installed, static_cast<long long>(r.st.st_ctime));
    } else if (r.state == probe_state::error && error.empty()) {
      error = describe_error("read", p, r.error_number);
    }
  }
  if (found) return kernel_image::make_present(installed);
  if (!error.empty()) return kernel_image::make_unreadable(error);
  return kernel_image();
}

kernel_inventory read_kernel_inventory(const std::string &root) {
  kernel_inventory inv;
  inv.container = detect_container(root);
  if (inv.container) return inv;
  std::set<std::string> releases;
  for (const char *dir : kModuleDirs) {
    const std::string path = root + dir;
    DIR *d = ::opendir(path.c_str());
    if (d == nullptr) {
      const int e = errno;
      if (e == ENOENT || e == ENOTDIR) continue;  // no modules here: fine (containers, minimal images)
      if (inv.error.empty()) inv.error = describe_error("list", dir, e);
      continue;
    }
    std::size_t seen = 0;
    for (struct dirent *ent = ::readdir(d); ent != nullptr && seen < kMaxDirEntries; ent = ::readdir(d), ++seen) {
      const std::string name = ent->d_name;
      if (!plausible_release(name)) continue;
      const probe_result r = probe(path + "/" + name);
      if (r.state == probe_state::exists && S_ISDIR(r.st.st_mode)) releases.insert(name);
    }
    ::closedir(d);
  }
  for (const std::string &release : releases) inv.kernels[release] = find_kernel_image(root, release);
  return inv;
}

// --- marker ----------------------------------------------------------------------------

std::vector<std::string> parse_reboot_packages(const std::string &content, const bool truncated) {
  std::vector<std::string> lines = split(content, '\n');
  if (truncated && !lines.empty()) lines.pop_back();
  std::vector<std::string> out;
  std::set<std::string> seen;
  for (const std::string &raw : lines) {
    if (out.size() >= kMaxPackages) break;
    const std::string name = trim_ascii(raw);
    if (name.empty() || name.size() > kMaxPackageName) continue;
    bool valid = true;
    for (const char c : name) {
      if (!is_package_char(c)) valid = false;
    }
    if (valid && seen.insert(name).second) out.push_back(name);
  }
  return out;
}

// --- gathering ------------------------------------------------------------------------

reboot_obj gather_pending_reboot_from(const std::string &root, const std::string &running_kernel, const long long now) {
  reboot_obj o;
  o.now = now;
  o.running_kernel = running_kernel;

  long long marker_written = 0;
  // Debian/Ubuntu: the package hooks touch /run/reboot-required and append
  // the triggering package to reboot-required.pkgs. /var/run is the legacy
  // spelling (a symlink to /run on every current system). SUSE: zypper
  // writes /run/reboot-needed (what `zypper needs-rebooting` tests). A probe
  // that fails for another reason than "not there" leaves the marker unknown
  // unless another location answers.
  const auto note_error = [&o](const std::string &path, const probe_result &r) {
    if (r.state != probe_state::error || o.marker_unknown) return;
    o.marker_unknown = true;
    o.marker_error = describe_error("read", path, r.error_number);
  };
  const char *const run_dirs[] = {"/run", "/var/run"};
  for (const char *dir : run_dirs) {
    const probe_result r = probe(root + dir + "/reboot-required");
    note_error(std::string(dir) + "/reboot-required", r);
    if (r.state != probe_state::exists) continue;
    o.marker = true;
    o.marker_name = "reboot-required";
    marker_written = static_cast<long long>(r.st.st_mtime);
    std::string pkgs;
    bool truncated = false;
    // The package list is detail, not a signal: an unreadable list leaves
    // `packages` empty while the marker itself is still reported.
    if (read_bounded(root + dir + "/reboot-required.pkgs", kMaxPackagesBytes, pkgs, truncated, nullptr)) o.packages = parse_reboot_packages(pkgs, truncated);
    break;
  }
  if (!o.marker) {
    const probe_result r = probe(root + "/run/reboot-needed");
    note_error("/run/reboot-needed", r);
    if (r.state == probe_state::exists) {
      o.marker = true;
      o.marker_name = "reboot-needed";
      marker_written = static_cast<long long>(r.st.st_mtime);
    }
  }

  const kernel_verdict v = evaluate_kernels(running_kernel, read_kernel_inventory(root));
  o.kernel = v.pending;
  o.kernel_unknown = v.unknown;
  o.kernel_reason = v.reason;
  o.kernel_error = v.error;
  o.newest_kernel = v.newest;

  o.pending_since = oldest(marker_written, o.kernel ? v.pending_since : 0);
  return o;
}

// --- the check -------------------------------------------------------------------

// age holds seconds; the converter lets `age > 7d` mean seven days instead of
// being silently read as the number 7.
static const parsers::where::value_type type_custom_age = parsers::where::type_custom_int_1;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_int_var("pending", parsers::where::type_bool, &reboot_obj::get_pending,
                        "1 if any pending-reboot signal is set (the aggregate flag most checks threshold on)")
      .add_int_var("signals", &reboot_obj::get_signals, "Number of distinct pending-reboot signals currently set");
  registry_
      .add_optional_int_var("marker", parsers::where::type_bool, [](auto obj) { return obj->get_marker(); }, "unknown",
                            "1 if the distribution's reboot marker is present: /run/reboot-required (Debian/Ubuntu package hooks) or /run/reboot-needed "
                            "(SUSE zypper). 'unknown' when it could not be looked up (see allow-unreadable)")
      .add_optional_int_var("kernel", parsers::where::type_bool, [](auto obj) { return obj->get_kernel(); }, "unknown",
                            "1 if the next boot changes kernel: a newer kernel of the running kernel's product line is installed, or the running "
                            "kernel has been removed (Arch). Never set in a container, which does not own the running kernel. 'unknown' when the "
                            "kernel inventory could not be read (see allow-unreadable)")
      .add_optional_int_var("written", parsers::where::type_date, [](auto obj) { return obj->get_written(); }, "unknown",
                            "When the reboot started pending, as epoch seconds: the oldest of the marker's modification time and the install time of "
                            "the first kernel newer than the running one. Threshold staleness with `age` or a relative date (written < -7d); a quoted "
                            "date string is compared as text, not as a date. 'unknown' when no timestamped signal is set")
      .no_perf()
      .add_optional_int_var("age", type_custom_age, [](auto obj) { return obj->get_age(); }, "unknown",
                            "Seconds the reboot has been pending; threshold with durations, e.g. pending = 1 and age > 7d")
      .add_int_perf("s", "", "_age");
  registry_.add_converter(type_custom_age, &duration_keyword::parse_duration<std::shared_ptr<reboot_obj> >);
  registry_.add_string_var("written_s", &reboot_obj::get_written_s, "Signal-appearance time as a human-readable string ('unknown' when not known)")
      .add_string_var("reasons", &reboot_obj::get_reasons, "Comma-separated human-readable list of pending-reboot causes ('none' if clear)")
      .add_string_var("message", &reboot_obj::get_message,
                      "Full status sentence, e.g. 'Reboot required: newer kernel installed (6.1.0-20-amd64, running 6.1.0-18-amd64) (pending since 2026-08-16 09:00:00)'")
      .add_string_var("packages", &reboot_obj::get_packages,
                      "Comma-separated packages that asked for the reboot (/run/reboot-required.pkgs, Debian/Ubuntu); empty when none are listed")
      .add_string_var("running_kernel", &reboot_obj::get_running_kernel, "The running kernel release (uname -r)")
      .add_string_var("newest_kernel", &reboot_obj::get_newest_kernel,
                      "The newest installed kernel of the running kernel's product line; empty when none was found (containers)");
  // clang-format on
}

void check_pending_reboot_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                               const reboot_obj &data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  // Same contract as the Windows check: WARNING when any reboot is pending,
  // no critical. There is always exactly one row, so the empty state never
  // applies.
  filter_helper.add_options("pending = 1", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_short_threshold("pending", modern_filter::shorthand::above, modern_filter::shorthand::unit_plain, "0", "");
  filter_helper.add_syntax("${status}: ${list}", "${message}", "reboot", "", "%(status): No reboot pending");
  filter_helper.set_default_perf_config("extra(pending;signals)");

  bool allow_unreadable = false;
  // clang-format off
  filter_helper.get_desc().add_options()
    ("allow-unreadable", po::value<bool>(&allow_unreadable)->implicit_value(true)->default_value(false),
     "Linux: report the signals that could be read when another could not (a modules directory or /boot the agent may not read, a "
     "marker it may not stat). The unreadable signal (`kernel`, `marker`) is then 'unknown' and never matches a threshold. Without "
     "it, an unreadable signal makes the check UNKNOWN unless another signal already says a reboot is pending.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  // "No reboot pending" would be a claim the check cannot make: refuse
  // unless the operator opted in. A signal that did fire is still reported.
  if (data.has_unknown() && !data.any() && !allow_unreadable) {
    return nscapi::protobuf::functions::set_response_bad(
        *response, "Cannot tell whether a reboot is pending: " + data.get_unreadable() + "; run the agent with read access to it or pass allow-unreadable");
  }

  const std::shared_ptr<reboot_obj> record(new reboot_obj(data));
  filter.match(record);

  filter_helper.post_process(filter);
}

void check_pending_reboot(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    check_pending_reboot_from(request, response, gather_pending_reboot());
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to read pending-reboot state: " + std::string(e.what()));
  }
}

}  // namespace pending_reboot_check
