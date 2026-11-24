// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_CHECK_PENDING_REBOOT_H
#define NSCP_CHECK_PENDING_REBOOT_H

#include <boost/optional.hpp>
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
// compares the running kernel with the newest installed one.
struct reboot_obj {
  // A distribution's "reboot required" marker file is present:
  // /run/reboot-required (Debian/Ubuntu, written by the package hooks of
  // kernels, libc, dbus, ...) or /run/reboot-needed (SUSE, written by zypper).
  bool marker;
  std::string marker_name;  // which one: "reboot-required" or "reboot-needed"
  // Packages listed in /run/reboot-required.pkgs, de-duplicated, in order.
  std::vector<std::string> packages;

  // A newer kernel of the running kernel's flavour is installed than the one
  // running: the next boot changes kernel.
  bool kernel;
  std::string running_kernel;  // uname -r
  std::string newest_kernel;   // newest installed kernel of the same flavour; empty when none was found

  // When the reboot request appeared, as epoch seconds (0 = unknown): the
  // oldest of the marker's mtime and the newest kernel image's ctime (when it
  // was written to disk; the mtime is the package build time).
  long long pending_since;
  long long now;  // epoch "now" used to derive age; captured at gather time so tests can pin it

  reboot_obj() : marker(false), kernel(false), pending_since(0), now(0) {}

  bool any() const { return marker || kernel; }
  long long signal_count() const { return (marker ? 1 : 0) + (kernel ? 1 : 0); }

  long long get_pending() const { return any() ? 1 : 0; }
  long long get_signals() const { return signal_count(); }
  long long get_marker() const { return marker ? 1 : 0; }
  long long get_kernel() const { return kernel ? 1 : 0; }
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

  std::string show() const { return get_message(); }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<reboot_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<reboot_obj, filter_obj_handler> filter_type;

// --- Kernel releases (pure, unit-tested) -------------------------------------

// Order two kernel release strings (uname -r style) the way the package
// managers order versions: split into runs of digits and runs of letters,
// numeric runs compare as numbers (of any length), alphabetic runs compare as
// text, a numeric run is newer than an alphabetic one, '~' sorts before
// everything including the end of the string, and other punctuation only
// separates. <0, 0, >0 like strcmp.
int compare_kernel_versions(const std::string &a, const std::string &b);

// The flavour of a kernel release: what a newer kernel of "the same kind"
// shares with it. Debian/Ubuntu/SUSE put it in the dash-separated tokens after
// the last numeric or dotted (ABI/release) one ("6.1.0-18-cloud-amd64" -> "cloud-amd64",
// "5.15.0-91-generic" -> "generic", "6.12.38+deb13-amd64" -> "amd64"); the RHEL
// family appends a '+variant' to the architecture ("...el9_4.x86_64+debug" ->
// "+debug") and has none otherwise ("4.18.0-513.24.1.el8_9.x86_64" -> "").
std::string kernel_flavor(const std::string &release);

struct installed_kernel {
  std::string release;  // as uname -r would print it
  long long installed;  // ctime of its image (epoch seconds), 0 when unknown

  installed_kernel() : installed(0) {}
  installed_kernel(const std::string &r, long long t) : release(r), installed(t) {}
};

// The installed kernels under `root`: /boot/vmlinuz-<release> (every
// distribution) and /lib/modules/<release>/vmlinuz (the RHEL 8+/Fedora/Arch
// layout), de-duplicated. Rescue images, *.old/*.bak copies and unversioned
// names are skipped. Empty in a container, which installs no kernel.
std::vector<installed_kernel> list_installed_kernels(const std::string &root);

// True when `root` holds the running kernel: /lib/modules/<running> exists.
// A container shares the host's kernel but not its /lib/modules, so this is
// what keeps a kernel package installed in an image from being compared with
// the host's kernel.
bool owns_running_kernel(const std::string &root, const std::string &running);

struct kernel_verdict {
  bool newer_installed;  // a newer same-flavour kernel than `running` is installed
  std::string newest;    // newest same-flavour installed release (may equal running)
  long long installed;   // its install time, 0 when unknown

  kernel_verdict() : newer_installed(false), installed(0) {}
};

// Compare the running kernel with the installed ones of the same flavour.
kernel_verdict evaluate_kernels(const std::string &running, const std::vector<installed_kernel> &installed);

// /run/reboot-required.pkgs: one package per line. Returns the names, trimmed,
// de-duplicated and stripped of anything non-printable, capped at 256 entries.
std::vector<std::string> parse_reboot_packages(const std::string &content);

// Gather every signal from a filesystem rooted at `root` ("" for the real
// one), for the given running kernel release and current time.
reboot_obj gather_pending_reboot_from(const std::string &root, const std::string &running_kernel, long long now);

// Platform reader (reboot_source_linux.cpp / reboot_source_darwin.cpp).
reboot_obj gather_pending_reboot();

// Testable core: render / threshold a pre-populated row.
void check_pending_reboot_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                               const reboot_obj &data);

// Live check.
void check_pending_reboot(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace pending_reboot_check

#endif  // NSCP_CHECK_PENDING_REBOOT_H
