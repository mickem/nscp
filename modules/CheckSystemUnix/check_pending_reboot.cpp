// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_pending_reboot.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <cctype>
#include <cerrno>
#include <check/duration_keyword.hpp>
#include <ctime>
#include <map>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <set>
#include <str/format.hpp>
#include <str/xtos.hpp>

namespace pending_reboot_check {

namespace {

// How many package names the reasons line spells out before summarising.
const std::size_t kReasonPackages = 5;
// Bounds on what is read from the marker's package list.
const std::size_t kMaxPackagesBytes = 64 * 1024;
const std::size_t kMaxPackages = 256;
// Bound on directory entries looked at per kernel directory.
const std::size_t kMaxDirEntries = 4096;

bool is_digit(const char c) { return c >= '0' && c <= '9'; }
bool is_alpha(const char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool is_alnum(const char c) { return is_digit(c) || is_alpha(c); }

std::string join(const std::vector<std::string> &parts, const std::string &sep) {
  std::string out;
  for (const std::string &p : parts) {
    if (!out.empty()) out += sep;
    out += p;
  }
  return out;
}

// stat() a path; false when it is not there (or not reachable).
bool stat_path(const std::string &path, struct stat &st) { return ::stat(path.c_str(), &st) == 0; }

// At most `limit` bytes of a file; empty when it cannot be read.
std::string read_bounded(const std::string &path, const std::size_t limit) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) return "";
  std::string out;
  char buf[4096];
  while (out.size() < limit) {
    const ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) break;
    out.append(buf, static_cast<std::size_t>(n));
  }
  ::close(fd);
  if (out.size() > limit) out.resize(limit);
  return out;
}

// A release name taken from a file name is used as a path component and
// rendered; only accept what a kernel release can look like.
bool plausible_release(const std::string &r) {
  if (r.empty() || r.size() > 128 || !is_digit(r[0])) return false;
  for (const char c : r) {
    if (!(is_alnum(c) || c == '.' || c == '-' || c == '_' || c == '+' || c == '~')) return false;
  }
  return true;
}

bool ends_with(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void add_kernel(std::map<std::string, long long> &found, const std::string &release, const long long ctime) {
  std::map<std::string, long long>::iterator it = found.find(release);
  if (it == found.end()) {
    found[release] = ctime;
  } else if (ctime != 0 && (it->second == 0 || ctime < it->second)) {
    it->second = ctime;
  }
}

// /lib/modules/<release>/vmlinuz under one modules directory.
void scan_modules_dir(const std::string &dir, std::map<std::string, long long> &found) {
  boost::system::error_code ec;
  boost::filesystem::directory_iterator it(dir, ec), end;
  std::size_t seen = 0;
  for (; !ec && it != end && seen < kMaxDirEntries; it.increment(ec), ++seen) {
    const std::string release = it->path().filename().string();
    if (!plausible_release(release)) continue;
    struct stat st;
    if (!stat_path(dir + "/" + release + "/vmlinuz", st) || !S_ISREG(st.st_mode)) continue;
    add_kernel(found, release, static_cast<long long>(st.st_ctime));
  }
}

}  // namespace

// --- reboot_obj ----------------------------------------------------------------

std::string reboot_obj::get_packages() const { return join(packages, ", "); }

std::string reboot_obj::get_reasons() const {
  std::vector<std::string> parts;
  if (marker) {
    std::string m = (marker_name.empty() ? std::string("reboot-required") : marker_name) + " marker";
    if (!packages.empty()) {
      std::vector<std::string> shown;
      for (std::size_t i = 0; i < packages.size() && i < kReasonPackages; ++i) shown.push_back(packages[i]);
      m += " (" + join(shown, ", ");
      if (packages.size() > kReasonPackages) m += " and " + str::xtos(packages.size() - kReasonPackages) + " more";
      m += ")";
    }
    parts.push_back(m);
  }
  if (kernel) parts.push_back("newer kernel installed (" + newest_kernel + ", running " + running_kernel + ")");
  if (parts.empty()) return "none";
  return join(parts, ", ");
}

std::string reboot_obj::get_written_s() const {
  if (pending_since == 0) return "unknown";
  return str::format::format_date(static_cast<std::time_t>(pending_since));
}

std::string reboot_obj::get_message() const {
  if (!any()) return "No reboot pending";
  std::string msg = "Reboot required: " + get_reasons();
  if (pending_since != 0) msg += " (pending since " + get_written_s() + ")";
  return msg;
}

// --- kernel releases --------------------------------------------------------------

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

std::string kernel_flavor(const std::string &release) {
  std::vector<std::string> tokens;
  std::string::size_type start = 0;
  for (;;) {
    const std::string::size_type dash = release.find('-', start);
    tokens.push_back(release.substr(start, dash == std::string::npos ? std::string::npos : dash - start));
    if (dash == std::string::npos) break;
    start = dash + 1;
  }
  std::size_t last_numeric = 0;
  for (std::size_t k = 1; k < tokens.size(); ++k) {
    // An ABI/release token is numeric ("91") or dotted ("513.24.1.el8_9.x86_64",
    // "150600.23.25"); "64k" in "generic-64k" starts with a digit but is flavour.
    const std::string &t = tokens[k];
    if (!t.empty() && is_digit(t[0]) && (t.find_first_not_of("0123456789") == std::string::npos || t.find('.') != std::string::npos)) last_numeric = k;
  }
  std::vector<std::string> rest(tokens.begin() + static_cast<std::ptrdiff_t>(last_numeric) + 1, tokens.end());
  std::string flavor = join(rest, "-");
  if (flavor.empty()) {
    const std::string::size_type dash = release.find('-');
    const std::string::size_type plus = release.rfind('+');
    if (dash != std::string::npos && plus != std::string::npos && plus > dash) flavor = release.substr(plus);
  }
  return flavor;
}

std::vector<installed_kernel> list_installed_kernels(const std::string &root) {
  std::map<std::string, long long> found;
  {
    const std::string boot = root + "/boot";
    boost::system::error_code ec;
    boost::filesystem::directory_iterator it(boot, ec), end;
    std::size_t seen = 0;
    for (; !ec && it != end && seen < kMaxDirEntries; it.increment(ec), ++seen) {
      const std::string name = it->path().filename().string();
      const std::string prefix = "vmlinuz-";
      if (name.compare(0, prefix.size(), prefix) != 0) continue;
      const std::string release = name.substr(prefix.size());
      // vmlinuz-0-rescue-<machine-id> is a rescue image, not an installed
      // kernel; *.old / *.bak are copies a tool left behind.
      if (release.find("rescue") != std::string::npos || ends_with(release, ".old") || ends_with(release, ".bak")) continue;
      if (!plausible_release(release)) continue;
      struct stat st;
      if (!stat_path(boot + "/" + name, st) || !S_ISREG(st.st_mode)) continue;
      add_kernel(found, release, static_cast<long long>(st.st_ctime));
    }
  }
  scan_modules_dir(root + "/lib/modules", found);
  scan_modules_dir(root + "/usr/lib/modules", found);

  std::vector<installed_kernel> out;
  for (const auto &kv : found) out.push_back(installed_kernel(kv.first, kv.second));
  return out;
}

bool owns_running_kernel(const std::string &root, const std::string &running) {
  if (!plausible_release(running)) return false;
  struct stat st;
  if (stat_path(root + "/lib/modules/" + running, st) && S_ISDIR(st.st_mode)) return true;
  if (stat_path(root + "/usr/lib/modules/" + running, st) && S_ISDIR(st.st_mode)) return true;
  return false;
}

kernel_verdict evaluate_kernels(const std::string &running, const std::vector<installed_kernel> &installed) {
  kernel_verdict v;
  if (running.empty()) return v;
  const std::string flavor = kernel_flavor(running);
  bool have = false;
  for (const installed_kernel &k : installed) {
    if (kernel_flavor(k.release) != flavor) continue;
    if (!have || compare_kernel_versions(k.release, v.newest) > 0) {
      v.newest = k.release;
      v.installed = k.installed;
      have = true;
    }
  }
  v.newer_installed = have && compare_kernel_versions(v.newest, running) > 0;
  return v;
}

std::vector<std::string> parse_reboot_packages(const std::string &content) {
  std::vector<std::string> out;
  std::set<std::string> seen;
  std::string::size_type start = 0;
  while (start < content.size() && out.size() < kMaxPackages) {
    std::string::size_type nl = content.find('\n', start);
    if (nl == std::string::npos) nl = content.size();
    std::string name;
    for (std::string::size_type k = start; k < nl; ++k) {
      const unsigned char c = static_cast<unsigned char>(content[k]);
      if (c > 0x20 && c < 0x7f) name += static_cast<char>(c);
    }
    if (!name.empty() && seen.insert(name).second) out.push_back(name);
    start = nl + 1;
  }
  return out;
}

reboot_obj gather_pending_reboot_from(const std::string &root, const std::string &running_kernel, const long long now) {
  reboot_obj o;
  o.now = now;
  o.running_kernel = running_kernel;

  long long marker_written = 0;
  // Debian/Ubuntu: the package hooks touch /run/reboot-required and append
  // the triggering package to reboot-required.pkgs. /var/run is the legacy
  // spelling (a symlink to /run on every current system).
  const char *const run_dirs[] = {"/run", "/var/run"};
  for (const char *dir : run_dirs) {
    struct stat st;
    if (!stat_path(root + dir + "/reboot-required", st)) continue;
    o.marker = true;
    o.marker_name = "reboot-required";
    marker_written = static_cast<long long>(st.st_mtime);
    o.packages = parse_reboot_packages(read_bounded(root + dir + "/reboot-required.pkgs", kMaxPackagesBytes));
    break;
  }
  if (!o.marker) {
    // SUSE: zypper writes /run/reboot-needed after installing a package
    // flagged as needing a reboot (what `zypper needs-rebooting` tests).
    struct stat st;
    if (stat_path(root + "/run/reboot-needed", st)) {
      o.marker = true;
      o.marker_name = "reboot-needed";
      marker_written = static_cast<long long>(st.st_mtime);
    }
  }

  long long kernel_written = 0;
  if (owns_running_kernel(root, running_kernel)) {
    const kernel_verdict v = evaluate_kernels(running_kernel, list_installed_kernels(root));
    o.newest_kernel = v.newest;
    o.kernel = v.newer_installed;
    if (o.kernel) kernel_written = v.installed;
  }

  if (marker_written != 0 && kernel_written != 0) {
    o.pending_since = marker_written < kernel_written ? marker_written : kernel_written;
  } else {
    o.pending_since = marker_written != 0 ? marker_written : kernel_written;
  }
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
      .add_int_var("signals", &reboot_obj::get_signals, "Number of distinct pending-reboot signals currently set")
      .add_int_var("marker", parsers::where::type_bool, &reboot_obj::get_marker,
                   "1 if the distribution's reboot marker is present: /run/reboot-required (Debian/Ubuntu package hooks) or /run/reboot-needed (SUSE zypper)")
      .add_int_var("kernel", parsers::where::type_bool, &reboot_obj::get_kernel,
                   "1 if a newer kernel of the running kernel's flavour is installed than the one running (the next boot changes kernel). "
                   "Never set in a container, which does not own the running kernel");
  registry_
      .add_optional_int_var("written", parsers::where::type_date, [](auto obj) { return obj->get_written(); }, "unknown",
                            "When the reboot started pending, as epoch seconds: the oldest of the marker's modification time and the time the newer "
                            "kernel's image was written. Threshold staleness with `age` or a relative date (written < -7d); a quoted date string is "
                            "compared as text, not as a date. 'unknown' when no reboot is pending")
      .no_perf()
      .add_optional_int_var("age", type_custom_age, [](auto obj) { return obj->get_age(); }, "unknown",
                            "Seconds the reboot has been pending; threshold with durations, e.g. pending = 1 and age > 7d")
      .add_int_perf("s", "", "_age");
  registry_.add_converter(type_custom_age, &duration_keyword::parse_duration<std::shared_ptr<reboot_obj> >);
  registry_.add_string_var("written_s", &reboot_obj::get_written_s, "Signal-appearance time as a human-readable string ('unknown' when nothing is pending)")
      .add_string_var("reasons", &reboot_obj::get_reasons, "Comma-separated human-readable list of pending-reboot causes ('none' if clear)")
      .add_string_var("message", &reboot_obj::get_message,
                      "Full status sentence, e.g. 'Reboot required: newer kernel installed (6.1.0-20-amd64, running 6.1.0-18-amd64) (pending since 2026-08-16 09:00:00)'")
      .add_string_var("packages", &reboot_obj::get_packages,
                      "Comma-separated packages that asked for the reboot (/run/reboot-required.pkgs, Debian/Ubuntu); empty when none are listed")
      .add_string_var("running_kernel", &reboot_obj::get_running_kernel, "The running kernel release (uname -r)")
      .add_string_var("newest_kernel", &reboot_obj::get_newest_kernel,
                      "The newest installed kernel of the running kernel's flavour; empty when no installed kernel was found (containers)");
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

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

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
