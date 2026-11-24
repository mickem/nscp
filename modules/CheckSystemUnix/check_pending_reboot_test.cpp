// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_pending_reboot.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

using pending_reboot_check::compare_kernel_versions;
using pending_reboot_check::evaluate_kernels;
using pending_reboot_check::installed_kernel;
using pending_reboot_check::kernel_flavor;
using pending_reboot_check::kernel_verdict;
using pending_reboot_check::reboot_obj;

namespace {

// RAII scratch directory acting as a fake filesystem root.
class FakeRoot {
 public:
  FakeRoot() {
    base_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-reboot-%%%%-%%%%");
    boost::filesystem::create_directories(base_);
  }
  ~FakeRoot() {
    boost::system::error_code ec;
    boost::filesystem::remove_all(base_, ec);
  }
  FakeRoot(const FakeRoot &) = delete;
  FakeRoot &operator=(const FakeRoot &) = delete;

  std::string root() const { return base_.string(); }

  void write(const std::string &rel, const std::string &content = "") const {
    const boost::filesystem::path p = base_ / rel;
    boost::filesystem::create_directories(p.parent_path());
    std::ofstream ofs(p.string().c_str());
    ofs << content;
  }
  void mkdir(const std::string &rel) const { boost::filesystem::create_directories(base_ / rel); }
  void set_mtime(const std::string &rel, std::time_t t) const { boost::filesystem::last_write_time(base_ / rel, t); }

  // An installed kernel in the Debian/Ubuntu layout: the image in /boot and
  // the modules directory.
  void debian_kernel(const std::string &release) const {
    write("boot/vmlinuz-" + release, "image");
    mkdir("lib/modules/" + release + "/kernel");
  }
  // The RHEL 8+/Fedora layout: the image inside the modules directory (and a
  // copy in /boot).
  void rhel_kernel(const std::string &release) const {
    write("lib/modules/" + release + "/vmlinuz", "image");
    write("boot/vmlinuz-" + release, "image");
  }

 private:
  boost::filesystem::path base_;
};

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run_check(const reboot_obj &data, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_pending_reboot");
  for (const std::string &a : args) request.add_arguments(a);
  pending_reboot_check::check_pending_reboot_from(request, &response, data);
  return response.result();
}

bool has_perf(const PB::Commands::QueryResponseMessage::Response &r, const std::string &alias, long long *value = nullptr) {
  for (int i = 0; i < r.lines_size(); ++i) {
    for (int j = 0; j < r.lines(i).perf_size(); ++j) {
      const auto &p = r.lines(i).perf(j);
      if (p.alias() == alias) {
        if (value != nullptr) *value = static_cast<long long>(p.float_value().value());
        return true;
      }
    }
  }
  return false;
}

const long long kNow = 1786000000;  // 2026-08-06

}  // namespace

// --- version comparison ----------------------------------------------------------

TEST(PendingRebootKernel, CompareDebianAbiNumbersNumerically) {
  EXPECT_GT(compare_kernel_versions("6.1.0-20-amd64", "6.1.0-18-amd64"), 0);
  EXPECT_LT(compare_kernel_versions("6.1.0-9-amd64", "6.1.0-18-amd64"), 0);  // a string compare gets this wrong
  EXPECT_EQ(compare_kernel_versions("6.1.0-18-amd64", "6.1.0-18-amd64"), 0);
  EXPECT_GT(compare_kernel_versions("6.12.41+deb13-amd64", "6.12.38+deb13-amd64"), 0);
  EXPECT_GT(compare_kernel_versions("6.1.0-0.deb11.17-amd64", "5.10.0-28-amd64"), 0);
}

TEST(PendingRebootKernel, CompareUbuntuReleases) {
  EXPECT_GT(compare_kernel_versions("5.15.0-101-generic", "5.15.0-91-generic"), 0);
  EXPECT_GT(compare_kernel_versions("6.8.0-45-generic", "5.15.0-122-generic"), 0);
  EXPECT_LT(compare_kernel_versions("6.8.0-40-generic", "6.8.0-45-generic"), 0);
}

TEST(PendingRebootKernel, CompareRhelReleases) {
  EXPECT_GT(compare_kernel_versions("4.18.0-513.24.1.el8_9.x86_64", "4.18.0-513.18.1.el8_9.x86_64"), 0);
  EXPECT_GT(compare_kernel_versions("4.18.0-553.el8_10.x86_64", "4.18.0-513.24.1.el8_9.x86_64"), 0);
  EXPECT_GT(compare_kernel_versions("5.14.0-427.13.1.el9_4.x86_64", "5.14.0-427.el9.x86_64"), 0);  // numeric beats alpha
  EXPECT_GT(compare_kernel_versions("5.14.0-503.11.1.el9_5.x86_64", "5.14.0-427.42.1.el9_4.x86_64"), 0);
  EXPECT_GT(compare_kernel_versions("6.11.4-301.fc41.x86_64", "6.10.12-200.fc40.x86_64"), 0);
}

TEST(PendingRebootKernel, CompareEdgeCases) {
  EXPECT_GT(compare_kernel_versions("6.1.10", "6.1.9"), 0);
  EXPECT_EQ(compare_kernel_versions("6.01.0", "6.1.0"), 0);                   // leading zeros
  EXPECT_GT(compare_kernel_versions("6.1.0.1", "6.1.0"), 0);                  // more components is newer
  EXPECT_LT(compare_kernel_versions("6.1.0~rc1", "6.1.0"), 0);                // tilde is a pre-release
  EXPECT_GT(compare_kernel_versions("99999999999999999999999.0", "9.0"), 0);  // no overflow
  EXPECT_EQ(compare_kernel_versions("", ""), 0);
  EXPECT_LT(compare_kernel_versions("", "1"), 0);
}

TEST(PendingRebootKernel, Flavors) {
  EXPECT_EQ(kernel_flavor("6.1.0-18-amd64"), "amd64");
  EXPECT_EQ(kernel_flavor("6.1.0-18-cloud-amd64"), "cloud-amd64");
  EXPECT_EQ(kernel_flavor("6.12.38+deb13-amd64"), "amd64");
  EXPECT_EQ(kernel_flavor("5.15.0-91-generic"), "generic");
  EXPECT_EQ(kernel_flavor("6.8.0-1015-aws"), "aws");
  EXPECT_EQ(kernel_flavor("6.8.0-45-generic-64k"), "generic-64k");
  EXPECT_EQ(kernel_flavor("6.4.0-150600.23.25-default"), "default");  // SUSE
  EXPECT_EQ(kernel_flavor("4.18.0-513.24.1.el8_9.x86_64"), "");
  EXPECT_EQ(kernel_flavor("5.14.0-427.13.1.el9_4.x86_64+debug"), "+debug");
  EXPECT_EQ(kernel_flavor("6.11.4-301.fc41.x86_64"), "");
  EXPECT_EQ(kernel_flavor("6.6.1"), "");
}

TEST(PendingRebootKernel, EvaluateFindsNewerSameFlavorKernel) {
  const std::vector<installed_kernel> installed = {installed_kernel("6.8.0-40-generic", 100), installed_kernel("6.8.0-45-generic", 200),
                                                   installed_kernel("6.8.0-50-lowlatency", 300)};
  const kernel_verdict v = evaluate_kernels("6.8.0-40-generic", installed);
  EXPECT_TRUE(v.newer_installed);
  EXPECT_EQ(v.newest, "6.8.0-45-generic");  // lowlatency is another flavour
  EXPECT_EQ(v.installed, 200);
}

TEST(PendingRebootKernel, EvaluateRunningNewestIsClear) {
  const std::vector<installed_kernel> installed = {installed_kernel("4.18.0-513.18.1.el8_9.x86_64", 1), installed_kernel("4.18.0-513.24.1.el8_9.x86_64", 2)};
  const kernel_verdict v = evaluate_kernels("4.18.0-513.24.1.el8_9.x86_64", installed);
  EXPECT_FALSE(v.newer_installed);
  EXPECT_EQ(v.newest, "4.18.0-513.24.1.el8_9.x86_64");
}

TEST(PendingRebootKernel, EvaluateRhelIgnoresDebugVariant) {
  const std::vector<installed_kernel> installed = {installed_kernel("5.14.0-427.13.1.el9_4.x86_64", 1),
                                                   installed_kernel("5.14.0-503.11.1.el9_5.x86_64+debug", 2)};
  EXPECT_FALSE(evaluate_kernels("5.14.0-427.13.1.el9_4.x86_64", installed).newer_installed);
  const std::vector<installed_kernel> updated = {installed_kernel("5.14.0-427.13.1.el9_4.x86_64", 1), installed_kernel("5.14.0-503.11.1.el9_5.x86_64", 2)};
  const kernel_verdict v = evaluate_kernels("5.14.0-427.13.1.el9_4.x86_64", updated);
  EXPECT_TRUE(v.newer_installed);
  EXPECT_EQ(v.newest, "5.14.0-503.11.1.el9_5.x86_64");
}

TEST(PendingRebootKernel, EvaluateNothingInstalled) {
  const kernel_verdict v = evaluate_kernels("6.18.44-fc-v819", std::vector<installed_kernel>());
  EXPECT_FALSE(v.newer_installed);
  EXPECT_EQ(v.newest, "");
}

// --- package list ------------------------------------------------------------------

TEST(PendingRebootPackages, ParsesDedupsAndSanitises) {
  const std::vector<std::string> p =
      pending_reboot_check::parse_reboot_packages("linux-image-6.8.0-45-generic\nlibc6\n\nlinux-base\nlibc6\n  dbus \r\nbad\x01name\xff\n");
  ASSERT_EQ(p.size(), 5u);
  EXPECT_EQ(p[0], "linux-image-6.8.0-45-generic");
  EXPECT_EQ(p[1], "libc6");
  EXPECT_EQ(p[2], "linux-base");
  EXPECT_EQ(p[3], "dbus");
  EXPECT_EQ(p[4], "badname");
}

TEST(PendingRebootPackages, CapsTheList) {
  std::string content;
  for (int i = 0; i < 1000; ++i) content += "pkg" + std::to_string(i) + "\n";
  EXPECT_EQ(pending_reboot_check::parse_reboot_packages(content).size(), 256u);
}

// --- gathering from fixture trees -----------------------------------------------------

TEST(PendingRebootGather, ContainerHasNothingPending) {
  // A container: no marker, no /boot images, no /lib/modules for the host's
  // running kernel.
  FakeRoot fs;
  fs.mkdir("boot");
  const reboot_obj o = pending_reboot_check::gather_pending_reboot_from(fs.root(), "6.18.44-fc-v819", kNow);
  EXPECT_FALSE(o.any());
  EXPECT_FALSE(o.kernel);
  EXPECT_EQ(o.newest_kernel, "");
  EXPECT_EQ(o.get_message(), "No reboot pending");
  EXPECT_FALSE(o.get_written());
}

TEST(PendingRebootGather, ContainerWithKernelPackageIsNotCompared) {
  // An image that installed a kernel package (newer than the host's) but does
  // not own the running kernel: must not report the host as needing a reboot.
  FakeRoot fs;
  fs.rhel_kernel("6.20.1-100.fc43.x86_64");
  const reboot_obj o = pending_reboot_check::gather_pending_reboot_from(fs.root(), "6.18.44-200.fc43.x86_64", kNow);
  EXPECT_FALSE(o.kernel);
  EXPECT_FALSE(o.any());
}

TEST(PendingRebootGather, UbuntuMarkerWithPackages) {
  FakeRoot fs;
  fs.debian_kernel("6.8.0-45-generic");
  fs.write("run/reboot-required", "*** System restart required ***\n");
  fs.write("run/reboot-required.pkgs", "linux-image-6.8.0-45-generic\nlibc6\nlibc6\n");
  fs.set_mtime("run/reboot-required", kNow - 3 * 86400);
  const reboot_obj o = pending_reboot_check::gather_pending_reboot_from(fs.root(), "6.8.0-45-generic", kNow);
  EXPECT_TRUE(o.marker);
  EXPECT_FALSE(o.kernel);
  EXPECT_EQ(o.get_packages(), "linux-image-6.8.0-45-generic, libc6");
  EXPECT_EQ(o.pending_since, kNow - 3 * 86400);
  EXPECT_EQ(o.get_age().value(), 3 * 86400);
  EXPECT_EQ(o.get_reasons(), "reboot-required marker (linux-image-6.8.0-45-generic, libc6)");
}

TEST(PendingRebootGather, LegacyVarRunMarker) {
  FakeRoot fs;
  fs.write("var/run/reboot-required");
  const reboot_obj o = pending_reboot_check::gather_pending_reboot_from(fs.root(), "6.1.0-18-amd64", kNow);
  EXPECT_TRUE(o.marker);
  EXPECT_EQ(o.get_packages(), "");
  EXPECT_EQ(o.get_reasons(), "reboot-required marker");
}

TEST(PendingRebootGather, SuseMarker) {
  FakeRoot fs;
  fs.write("run/reboot-needed");
  const reboot_obj o = pending_reboot_check::gather_pending_reboot_from(fs.root(), "6.4.0-150600.23.25-default", kNow);
  EXPECT_TRUE(o.marker);
  EXPECT_EQ(o.marker_name, "reboot-needed");
  EXPECT_EQ(o.get_reasons(), "reboot-needed marker");
}

TEST(PendingRebootGather, DebianNewerKernelInstalled) {
  FakeRoot fs;
  fs.debian_kernel("6.1.0-9-amd64");
  fs.debian_kernel("6.1.0-18-amd64");
  fs.write("boot/vmlinuz-6.1.0-18-amd64.old", "copy");  // ignored
  // A DKMS leftover: a modules directory without an image is not a kernel.
  fs.mkdir("lib/modules/6.1.0-25-amd64/updates/dkms");
  const long long before = static_cast<long long>(std::time(nullptr));
  const reboot_obj o = pending_reboot_check::gather_pending_reboot_from(fs.root(), "6.1.0-9-amd64", before + 10);
  EXPECT_TRUE(o.kernel);
  EXPECT_FALSE(o.marker);
  EXPECT_EQ(o.newest_kernel, "6.1.0-18-amd64");
  EXPECT_EQ(o.get_reasons(), "newer kernel installed (6.1.0-18-amd64, running 6.1.0-9-amd64)");
  // The image's ctime: when it was written, i.e. just now in the fixture.
  EXPECT_GE(o.pending_since, before - 5);
  EXPECT_LE(o.pending_since, before + 10);
}

TEST(PendingRebootGather, RhelNewerKernelInModulesLayout) {
  FakeRoot fs;
  fs.rhel_kernel("5.14.0-427.13.1.el9_4.x86_64");
  fs.write("usr/lib/modules/5.14.0-503.11.1.el9_5.x86_64/vmlinuz", "image");  // usrmerge layout
  fs.write("boot/vmlinuz-0-rescue-0123456789abcdef", "rescue");               // ignored
  const reboot_obj o = pending_reboot_check::gather_pending_reboot_from(fs.root(), "5.14.0-427.13.1.el9_4.x86_64", kNow);
  EXPECT_TRUE(o.kernel);
  EXPECT_EQ(o.newest_kernel, "5.14.0-503.11.1.el9_5.x86_64");
}

TEST(PendingRebootGather, RunningNewestKernelIsClear) {
  FakeRoot fs;
  fs.debian_kernel("5.15.0-91-generic");
  fs.debian_kernel("5.15.0-101-generic");
  const reboot_obj o = pending_reboot_check::gather_pending_reboot_from(fs.root(), "5.15.0-101-generic", kNow);
  EXPECT_FALSE(o.any());
  EXPECT_EQ(o.newest_kernel, "5.15.0-101-generic");
}

TEST(PendingRebootGather, BothSignalsTakeOldestTimestamp) {
  FakeRoot fs;
  fs.debian_kernel("6.8.0-40-generic");
  fs.debian_kernel("6.8.0-45-generic");
  fs.write("run/reboot-required");
  fs.set_mtime("run/reboot-required", 1000);
  const reboot_obj o = pending_reboot_check::gather_pending_reboot_from(fs.root(), "6.8.0-40-generic", kNow);
  EXPECT_EQ(o.signal_count(), 2);
  EXPECT_EQ(o.pending_since, 1000);
}

TEST(PendingRebootGather, ManyPackagesAreSummarised) {
  reboot_obj o;
  o.marker = true;
  o.marker_name = "reboot-required";
  for (int i = 0; i < 8; ++i) o.packages.push_back("p" + std::to_string(i));
  EXPECT_EQ(o.get_reasons(), "reboot-required marker (p0, p1, p2, p3, p4 and 3 more)");
}

// --- the check ---------------------------------------------------------------------------

TEST(PendingRebootCheck, CleanIsOk) {
  reboot_obj o;
  o.now = kNow;
  o.running_kernel = "6.8.0-45-generic";
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(o, {}, r), PB::Common::ResultCode::OK);
  EXPECT_EQ(join_lines(r), "OK: No reboot pending");
  long long v = -1;
  EXPECT_TRUE(has_perf(r, "reboot_pending", &v));
  EXPECT_EQ(v, 0);
  EXPECT_TRUE(has_perf(r, "reboot_signals", &v));
  EXPECT_EQ(v, 0);
}

TEST(PendingRebootCheck, PendingIsWarningByDefault) {
  reboot_obj o;
  o.now = kNow;
  o.kernel = true;
  o.running_kernel = "6.1.0-9-amd64";
  o.newest_kernel = "6.1.0-18-amd64";
  o.pending_since = kNow - 3600;
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(o, {}, r), PB::Common::ResultCode::WARNING);
  EXPECT_NE(join_lines(r).find("WARNING: Reboot required: newer kernel installed (6.1.0-18-amd64, running 6.1.0-9-amd64) (pending since "), std::string::npos)
      << join_lines(r);
}

TEST(PendingRebootCheck, AgeDurationThreshold) {
  reboot_obj o;
  o.now = kNow;
  o.marker = true;
  o.pending_since = kNow - 8 * 86400;
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(o, {"warn=pending = 1", "crit=pending = 1 and age > 7d"}, r), PB::Common::ResultCode::CRITICAL);
  PB::Commands::QueryResponseMessage::Response r2;
  EXPECT_EQ(run_check(o, {"warn=none", "crit=pending = 1 and age > 9d"}, r2), PB::Common::ResultCode::OK);
}

TEST(PendingRebootCheck, UnknownAgeNeverTrips) {
  reboot_obj o;
  o.now = kNow;
  o.marker = true;  // no timestamp
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(o, {"warn=none", "crit=age > 0"}, r), PB::Common::ResultCode::OK);
  PB::Commands::QueryResponseMessage::Response r2;
  EXPECT_EQ(run_check(o, {"warn=none", "crit=written = 'unknown'"}, r2), PB::Common::ResultCode::CRITICAL);
}

TEST(PendingRebootCheck, PerCauseExpressionsAndSyntax) {
  reboot_obj o;
  o.now = kNow;
  o.marker = true;
  o.marker_name = "reboot-required";
  o.packages.push_back("libc6");
  o.running_kernel = "6.1.0-18-amd64";
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(o, {"warn=none", "crit=kernel = 1", "detail-syntax=${signals} signal(s): ${reasons} [${packages}] ${running_kernel}"}, r),
            PB::Common::ResultCode::OK);
  EXPECT_EQ(join_lines(r), "OK: 1 signal(s): reboot-required marker (libc6) [libc6] 6.1.0-18-amd64");
  PB::Commands::QueryResponseMessage::Response r2;
  EXPECT_EQ(run_check(o, {"warn=none", "crit=marker = 1"}, r2), PB::Common::ResultCode::CRITICAL);
}
