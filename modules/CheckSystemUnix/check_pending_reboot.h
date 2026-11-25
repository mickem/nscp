// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_CHECK_PENDING_REBOOT_H
#define NSCP_CHECK_PENDING_REBOOT_H

#include <boost/optional.hpp>
#include <map>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

namespace pending_reboot_check {

// One aggregate row: is this machine waiting for a reboot, and why. The unix
// counterpart to the Windows check_pending_reboot. The aggregate keywords
// (pending, signals, written, age, written_s, reasons, message) mean the same
// on both platforms; the per-cause keywords are each platform's own, because
// the causes are: Windows reads servicing/Windows Update/rename/domain-join
// registry state, Linux reads the package manager's reboot marker and
// compares the running kernel with the installed ones.
struct reboot_obj {
  // A distribution's "reboot required" marker file is present:
  // /run/reboot-required (Debian/Ubuntu, written by the package hooks of
  // kernels, libc, dbus, ...) or /run/reboot-needed (SUSE, written by zypper).
  bool marker;
  std::string marker_name;  // which one: "reboot-required" or "reboot-needed"
  // Whether a marker exists could not be determined (stat failed for another
  // reason than "not there", e.g. an SELinux denial): `marker` is then unknown.
  bool marker_unknown;
  std::string marker_error;
  // Packages listed in /run/reboot-required.pkgs, de-duplicated, in order.
  std::vector<std::string> packages;

  // The next boot changes kernel: a newer kernel of the running kernel's
  // product line is installed, or the running kernel has been removed.
  bool kernel;
  // The kernel inventory could not be read (a directory that exists but cannot
  // be listed, an image that cannot be stat'ed): `kernel` is then unknown.
  bool kernel_unknown;
  std::string kernel_reason;   // the human-readable cause when kernel is set
  std::string kernel_error;    // why the inventory could not be read
  std::string running_kernel;  // uname -r
  std::string newest_kernel;   // newest installed kernel of the running kernel's line; empty when none

  // When the reboot started pending, as epoch seconds (0 = unknown): the
  // oldest of the marker's mtime and the install time of the first kernel
  // newer than the running one.
  long long pending_since;
  long long now;  // epoch "now" used to derive age; captured at gather time so tests can pin it

  reboot_obj() : marker(false), marker_unknown(false), kernel(false), kernel_unknown(false), pending_since(0), now(0) {}

  bool any() const { return marker || kernel; }
  long long signal_count() const { return (marker ? 1 : 0) + (kernel ? 1 : 0); }

  long long get_pending() const { return any() ? 1 : 0; }
  long long get_signals() const { return signal_count(); }
  boost::optional<long long> get_marker() const {
    if (marker_unknown && !marker) return boost::none;
    return marker ? 1 : 0;
  }
  boost::optional<long long> get_kernel() const {
    if (kernel_unknown && !kernel) return boost::none;
    return kernel ? 1 : 0;
  }
  std::string get_running_kernel() const { return running_kernel; }
  std::string get_newest_kernel() const { return newest_kernel; }
  std::string get_packages() const;

  boost::optional<long long> get_written() const {
    if (pending_since == 0) return boost::none;
    return pending_since;
  }
  boost::optional<long long> get_age() const {
    if (pending_since == 0) return boost::none;
    return now > pending_since ? now - pending_since : 0;
  }
  std::string get_written_s() const;
  std::string get_reasons() const;
  std::string get_message() const;
  // True when a signal that is not set could not be read: "no reboot
  // pending" is then not a claim the check can make.
  bool has_unknown() const { return (marker_unknown && !marker) || (kernel_unknown && !kernel); }
  // What could not be read, as a sentence fragment; empty when everything was.
  std::string get_unreadable() const;

  std::string show() const { return get_message(); }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<reboot_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<reboot_obj, filter_obj_handler> filter_type;

// --- Kernel releases (pure, unit-tested) -------------------------------------

// A string that can be a kernel release (uname -r) and therefore a path
// component: starts with a digit, at most 128 characters of [A-Za-z0-9._+~-].
bool plausible_release(const std::string &release);

// Order two kernel releases the way rpm orders versions: split into runs of
// digits and runs of letters, numeric runs compare as numbers (of any
// length), alphabetic runs compare as text, a numeric run is newer than an
// alphabetic one, '~' sorts before everything including the end of the
// string, and other punctuation only separates. <0, 0, >0 like strcmp.
int compare_kernel_versions(const std::string &a, const std::string &b);

// The release split on '-': "5.15.0-91-generic" -> {"5.15.0", "91", "generic"}.
std::vector<std::string> split_release(const std::string &release);

// True for the tokens of a dash-separated release that carry the build
// rather than the product line: numeric ("91"), dotted and digit-led
// ("0.deb11.17", "150600.23.25"), or letters followed by a build number
// ("arch1", "zen1") unless it is the `last` token. "64k" (a page-size
// flavour), and a last "amd64" or "v8", are not.
bool is_build_token(const std::string &token, bool last);

// The product line a release belongs to. Two releases are compared only when
// both are placed and their ids are equal: a newer kernel of another line
// (Oracle UEK, real-time, ELRepo, debug, 64k, -lowlatency, -cloud-amd64) is
// not an update of the running one. Successive builds of one line, and
// minor-release dist tags (el9 / el9_5, fc40 / fc41), share an id.
struct kernel_line {
  bool placed;
  std::string id;

  kernel_line() : placed(false) {}
  explicit kernel_line(const std::string &i) : placed(true), id(i) {}
};

// RPM-style releases: <version>-<release>.<arch>[+<variant>]
// ("5.14.0-427.13.1.el9_4.x86_64+debug"). The id is the architecture, the
// variant and the alphabetic markers of the release field, with the dist tag
// reduced to its family (el, fc, amzn) and anything after its number kept as a
// marker ("el8uek" -> el + "uek"). None when the string is not RPM-style.
boost::optional<kernel_line> rpm_kernel_line(const std::string &release);

// Dash-style releases (Debian, Ubuntu, SUSE, Arch, Raspberry Pi OS): the id
// is the tokens after the last build token ("6.1.0-18-cloud-amd64" ->
// "cloud-amd64", "6.6.1-zen1-1-zen" -> "zen") plus the letters of the build
// tokens after the first ("6.6.1-arch1-1" -> "arch").
kernel_line dash_kernel_line(const std::string &release);

// rpm_kernel_line() when it applies, else dash_kernel_line(); unplaced for a
// string that is not a plausible release.
kernel_line identify_line(const std::string &release);

// --- Kernel inventory ----------------------------------------------------------

// Whether a kernel image was found for a release, and when it was written.
struct kernel_image {
  enum state_type { absent, present, unreadable };
  state_type state;
  long long installed;  // ctime of the image (epoch seconds), 0 when unknown
  std::string error;    // why it is unreadable

  kernel_image() : state(absent), installed(0) {}
  static kernel_image make_present(long long t) {
    kernel_image k;
    k.state = present;
    k.installed = t;
    return k;
  }
  static kernel_image make_unreadable(const std::string &e) {
    kernel_image k;
    k.state = unreadable;
    k.error = e;
    return k;
  }
};

// Every release with a modules directory, and its image. A release counts as
// installed only on positive evidence: a modules directory plus a kernel
// image, /lib/modules/<rel>/vmlinuz (RHEL 8+, Fedora, Arch) or
// /boot/vmlinuz-<rel> (every distribution). A stray copy in /boot
// (vmlinuz-<rel>.orig) has no modules directory, and a DKMS leftover
// directory has no image, so neither is a kernel.
struct kernel_inventory {
  bool container;     // this root is a container's: it does not own the running kernel
  std::string error;  // a modules directory exists but could not be listed
  std::map<std::string, kernel_image> kernels;

  kernel_inventory() : container(false) {}
};

struct kernel_verdict {
  bool pending;             // the next boot changes kernel
  bool unknown;             // whether it does could not be determined
  std::string reason;       // "newer kernel installed (...)" / "running kernel ... is no longer installed (...)"
  std::string error;        // why it is unknown
  std::string newest;       // newest installed kernel of the running kernel's line
  long long pending_since;  // install time of the first newer kernel, 0 when unknown

  kernel_verdict() : pending(false), unknown(false), pending_since(0) {}
};

// Decide the kernel signal from an inventory (pure).
kernel_verdict evaluate_kernels(const std::string &running, const kernel_inventory &inventory);

// The fstype of the topmost mount on "/" in /proc/self/mountinfo content;
// empty when there is none.
std::string root_fs_type(const std::string &mountinfo);

// True when NUL-separated environment content (/proc/1/environ) carries a
// non-empty container= variable, as systemd-nspawn, LXC and podman set for
// PID 1.
bool environ_names_container(const std::string &environ);

// Whether `root` is a container's filesystem: the markers container managers
// leave (/run/systemd/container, /run/.containerenv, /.dockerenv, OpenVZ's
// /proc/vz without /proc/bc), container= in PID 1's environment, or a root
// filesystem of type overlay/aufs.
bool detect_container(const std::string &root);

// Look for the image of one release under `root` (see kernel_inventory).
kernel_image find_kernel_image(const std::string &root, const std::string &release);

// List /lib/modules and /usr/lib/modules under `root` and look up each
// release's image. A missing directory is not an error; one that exists but
// cannot be listed is.
kernel_inventory read_kernel_inventory(const std::string &root);

// --- Marker ----------------------------------------------------------------------

// /run/reboot-required.pkgs: one package per line. Returns the names,
// de-duplicated, at most 256. A line that is not a package name (characters
// outside [A-Za-z0-9+.:~_-], longer than 128) is skipped, not mangled. When
// `truncated` is set the content was cut at the read limit and its last,
// possibly partial, line is dropped.
std::vector<std::string> parse_reboot_packages(const std::string &content, bool truncated = false);

// --- Gathering -------------------------------------------------------------------

// Gather every signal from a filesystem rooted at `root` ("" for the real
// one), for the given running kernel release and current time.
reboot_obj gather_pending_reboot_from(const std::string &root, const std::string &running_kernel, long long now);

// Platform reader (reboot_source_linux.cpp / reboot_source_darwin.cpp).
reboot_obj gather_pending_reboot();

// Testable core: render / threshold a pre-populated row. When the kernel
// inventory or the marker could not be read and no other signal is set the
// check is UNKNOWN, unless allow-unreadable is given.
void check_pending_reboot_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                               const reboot_obj &data);

// Live check.
void check_pending_reboot(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace pending_reboot_check

#endif  // NSCP_CHECK_PENDING_REBOOT_H
