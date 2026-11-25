// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_pending_reboot.h"

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

using namespace pending_reboot_check;
using namespace std::string_literals;

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
    for (const std::string &p : restore_) ::chmod(p.c_str(), 0755);
    boost::filesystem::remove_all(base_, ec);
  }
  FakeRoot(const FakeRoot &) = delete;
  FakeRoot &operator=(const FakeRoot &) = delete;

  std::string root() const { return base_.string(); }

  void write(const std::string &rel, const std::string &content = "") const {
    const boost::filesystem::path p = base_ / rel;
    boost::filesystem::create_directories(p.parent_path());
    std::ofstream ofs(p.string().c_str(), std::ios::binary);
    ofs << content;
  }
  void mkdir(const std::string &rel) const { boost::filesystem::create_directories(base_ / rel); }
  void set_mtime(const std::string &rel, std::time_t t) const { boost::filesystem::last_write_time(base_ / rel, t); }
  // A path that resolves to itself: every lookup through it fails with ELOOP,
  // which is how a test running as root gets a "cannot read" error that a
  // permission bit would not give it.
  void loop(const std::string &rel) const {
    boost::filesystem::create_directories((base_ / rel).parent_path());
    boost::filesystem::create_symlink(base_ / rel, base_ / rel);
  }
  void chmod(const std::string &rel, mode_t mode) {
    const std::string p = (base_ / rel).string();
    ::chmod(p.c_str(), mode);
    restore_.push_back(p);
  }

  // The Debian/Ubuntu layout: the image in /boot plus the modules directory.
  void debian_kernel(const std::string &release) const {
    write("boot/vmlinuz-" + release, "image");
    mkdir("lib/modules/" + release + "/kernel");
  }
  // The RHEL 8+/Fedora layout: the image inside the modules directory, and a copy in /boot.
  void rhel_kernel(const std::string &release) const {
    write("lib/modules/" + release + "/vmlinuz", "image");
    write("boot/vmlinuz-" + release, "image");
  }
  // The Arch layout: the image inside /usr/lib/modules; /boot holds an
  // unversioned copy named after the package.
  void arch_kernel(const std::string &release, const std::string &package = "linux") const {
    write("usr/lib/modules/" + release + "/vmlinuz", "image");
    write("usr/lib/modules/" + release + "/pkgbase", package + "\n");
    write("boot/vmlinuz-" + package, "image");
  }

 private:
  boost::filesystem::path base_;
  std::vector<std::string> restore_;
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
  check_pending_reboot_from(request, &response, data);
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

bool same_line(const std::string &a, const std::string &b) {
  const kernel_line la = identify_line(a);
  const kernel_line lb = identify_line(b);
  return la.placed && lb.placed && la.id == lb.id;
}

// An inventory of present kernels with the given install times.
kernel_inventory inventory_of(const std::vector<std::pair<std::string, long long> > &kernels) {
  kernel_inventory inv;
  for (const auto &k : kernels) inv.kernels[k.first] = kernel_image::make_present(k.second);
  return inv;
}

const long long kNow = 1786000000;  // 2026-08-06

}  // namespace

// --- plausible_release ---------------------------------------------------------------

TEST(PendingRebootRelease, Plausible) {
  EXPECT_TRUE(plausible_release("6.1.0-18-amd64"));
  EXPECT_TRUE(plausible_release("5.14.0-427.13.1.el9_4.x86_64+debug"));
  EXPECT_TRUE(plausible_release("6.1.0~rc1"));
  EXPECT_FALSE(plausible_release(""));
  EXPECT_FALSE(plausible_release("linux"));  // Arch's /boot/vmlinuz-linux
  EXPECT_FALSE(plausible_release(".."));
  EXPECT_FALSE(plausible_release("6.1/../../etc"));
  EXPECT_FALSE(plausible_release("6.1 0"));
  EXPECT_FALSE(plausible_release("6.1\0x"s));
  EXPECT_FALSE(plausible_release("6.1\xff"));
  EXPECT_TRUE(plausible_release("6" + std::string(127, '1')));
  EXPECT_FALSE(plausible_release("6" + std::string(128, '1')));
}

// --- compare_kernel_versions --------------------------------------------------------

TEST(PendingRebootCompare, DebianAbiNumbersNumerically) {
  EXPECT_GT(compare_kernel_versions("6.1.0-20-amd64", "6.1.0-18-amd64"), 0);
  EXPECT_LT(compare_kernel_versions("6.1.0-9-amd64", "6.1.0-18-amd64"), 0);  // a string compare gets this wrong
  EXPECT_EQ(compare_kernel_versions("6.1.0-18-amd64", "6.1.0-18-amd64"), 0);
  EXPECT_GT(compare_kernel_versions("6.12.41+deb13-amd64", "6.12.38+deb13-amd64"), 0);
  EXPECT_GT(compare_kernel_versions("6.1.0-0.deb11.17-amd64", "5.10.0-28-amd64"), 0);
}

TEST(PendingRebootCompare, UbuntuReleases) {
  EXPECT_GT(compare_kernel_versions("5.15.0-101-generic", "5.15.0-91-generic"), 0);
  EXPECT_GT(compare_kernel_versions("6.8.0-45-generic", "5.15.0-122-generic"), 0);
  EXPECT_LT(compare_kernel_versions("6.8.0-40-generic", "6.8.0-45-generic"), 0);
}

TEST(PendingRebootCompare, RhelReleases) {
  EXPECT_GT(compare_kernel_versions("4.18.0-513.24.1.el8_9.x86_64", "4.18.0-513.18.1.el8_9.x86_64"), 0);
  EXPECT_GT(compare_kernel_versions("4.18.0-553.el8_10.x86_64", "4.18.0-513.24.1.el8_9.x86_64"), 0);
  EXPECT_GT(compare_kernel_versions("5.14.0-427.13.1.el9_4.x86_64", "5.14.0-427.el9.x86_64"), 0);  // numeric beats alpha
  EXPECT_GT(compare_kernel_versions("5.14.0-503.11.1.el9_5.x86_64", "5.14.0-427.42.1.el9_4.x86_64"), 0);
  EXPECT_GT(compare_kernel_versions("6.11.4-301.fc41.x86_64", "6.10.12-200.fc40.x86_64"), 0);
}

TEST(PendingRebootCompare, ArchReleases) {
  EXPECT_GT(compare_kernel_versions("6.6.2-arch1-1", "6.6.1-arch1-1"), 0);
  EXPECT_GT(compare_kernel_versions("6.6.1-arch2-1", "6.6.1-arch1-1"), 0);
  EXPECT_GT(compare_kernel_versions("6.6.1-arch1-2", "6.6.1-arch1-1"), 0);
}

TEST(PendingRebootCompare, EdgeAndAdversarialInput) {
  EXPECT_GT(compare_kernel_versions("6.1.10", "6.1.9"), 0);
  EXPECT_EQ(compare_kernel_versions("6.01.0", "6.1.0"), 0);                   // leading zeros
  EXPECT_EQ(compare_kernel_versions("6.000", "6.0"), 0);                      // all zeros
  EXPECT_GT(compare_kernel_versions("6.1.0.1", "6.1.0"), 0);                  // more components is newer
  EXPECT_LT(compare_kernel_versions("6.1.0~rc1", "6.1.0"), 0);                // tilde is a pre-release
  EXPECT_LT(compare_kernel_versions("6.1.0~~", "6.1.0~"), 0);                 // and stacks
  EXPECT_GT(compare_kernel_versions("99999999999999999999999.0", "9.0"), 0);  // no overflow
  EXPECT_EQ(compare_kernel_versions("", ""), 0);
  EXPECT_LT(compare_kernel_versions("", "1"), 0);
  EXPECT_EQ(compare_kernel_versions("...", "---"), 0);  // separators only
  EXPECT_EQ(compare_kernel_versions("6.1", "6-1"), 0);  // separators only separate
  EXPECT_EQ(compare_kernel_versions(std::string(10000, '9'), std::string(10000, '9')), 0);
  // Antisymmetric on a mixed set.
  const std::vector<std::string> v = {"6.1.0-18-amd64", "6.1.0-9-amd64", "5.14.0-427.el9.x86_64", "5.14.0-427.13.1.el9_4.x86_64", "6.1~rc1", "a", "", "~"};
  for (const std::string &a : v) {
    for (const std::string &b : v) EXPECT_EQ(compare_kernel_versions(a, b), -compare_kernel_versions(b, a)) << a << " vs " << b;
  }
}

// --- split_release / is_build_token ------------------------------------------------

TEST(PendingRebootTokens, SplitRelease) {
  EXPECT_EQ(split_release("5.15.0-91-generic"), (std::vector<std::string>{"5.15.0", "91", "generic"}));
  EXPECT_EQ(split_release("6.6.1"), (std::vector<std::string>{"6.6.1"}));
  EXPECT_EQ(split_release(""), (std::vector<std::string>{""}));
  EXPECT_EQ(split_release("6.1--x-"), (std::vector<std::string>{"6.1", "", "x", ""}));
}

TEST(PendingRebootTokens, BuildTokens) {
  EXPECT_TRUE(is_build_token("91", false));
  EXPECT_TRUE(is_build_token("91", true));
  EXPECT_TRUE(is_build_token("0.deb11.17", false));
  EXPECT_TRUE(is_build_token("150600.23.25", false));
  EXPECT_TRUE(is_build_token("arch1", false));
  EXPECT_TRUE(is_build_token("hardened1", false));
  EXPECT_FALSE(is_build_token("arch1", true));  // last: a flavour
  EXPECT_FALSE(is_build_token("amd64", true));
  EXPECT_FALSE(is_build_token("64k", false));
  EXPECT_FALSE(is_build_token("generic", false));
  EXPECT_FALSE(is_build_token("", false));
  EXPECT_FALSE(is_build_token("a1b", false));
}

// --- product lines ---------------------------------------------------------------------

TEST(PendingRebootLine, RpmStockBuildsShareALine) {
  EXPECT_TRUE(same_line("5.14.0-427.13.1.el9_4.x86_64", "5.14.0-503.11.1.el9_5.x86_64"));
  EXPECT_TRUE(same_line("5.14.0-427.el9.x86_64", "5.14.0-427.13.1.el9_4.x86_64"));
  EXPECT_TRUE(same_line("4.18.0-513.24.1.el8_9.x86_64", "4.18.0-553.el8_10.x86_64"));
  EXPECT_TRUE(same_line("6.10.12-200.fc40.x86_64", "6.11.4-301.fc41.x86_64"));
  EXPECT_TRUE(same_line("6.1.79-99.167.amzn2023.x86_64", "6.1.90-99.173.amzn2023.x86_64"));
}

TEST(PendingRebootLine, RpmProductLinesAreSeparate) {
  const std::string stock = "5.14.0-427.13.1.el9_4.x86_64";
  EXPECT_FALSE(same_line(stock, "5.15.0-208.159.3.el9uek.x86_64"));   // Oracle UEK
  EXPECT_FALSE(same_line(stock, "5.14.0-427.13.1.el9_4.x86_64+rt"));  // RHEL 9 real-time
  EXPECT_FALSE(same_line(stock, "5.14.0-427.13.1.el9_4.x86_64+debug"));
  EXPECT_FALSE(same_line(stock, "5.14.0-427.13.1.el9_4.aarch64+64k"));
  EXPECT_FALSE(same_line(stock, "6.6.1-1.el9.elrepo.x86_64"));                               // ELRepo kernel-ml
  EXPECT_FALSE(same_line(stock, "5.14.0-427.13.1.el9_4.aarch64"));                           // another architecture
  EXPECT_FALSE(same_line("4.18.0-513.24.1.el8_9.x86_64", "4.18.0-513.rt7.307.el8.x86_64"));  // RHEL 8 real-time
  EXPECT_FALSE(same_line("6.11.4-301.fc41.x86_64", "5.14.0-427.el9.x86_64"));
  // ...while each product line still updates within itself.
  EXPECT_TRUE(same_line("5.15.0-208.159.3.el8uek.x86_64", "5.15.0-209.161.7.el8uek.x86_64"));
  EXPECT_TRUE(same_line("4.18.0-513.rt7.307.el8.x86_64", "4.18.0-553.rt7.342.el8_10.x86_64"));
  EXPECT_TRUE(same_line("6.6.1-1.el9.elrepo.x86_64", "6.7.2-1.el9.elrepo.x86_64"));
  EXPECT_TRUE(same_line("5.14.0-427.13.1.el9_4.x86_64+debug", "5.14.0-503.11.1.el9_5.x86_64+debug"));
}

TEST(PendingRebootLine, RpmDetection) {
  EXPECT_TRUE(rpm_kernel_line("5.14.0-427.el9.x86_64"));
  EXPECT_FALSE(rpm_kernel_line("6.1.0-18-amd64"));              // Debian
  EXPECT_FALSE(rpm_kernel_line("6.4.0-150600.23.25-default"));  // SUSE: last field is not an arch
  EXPECT_FALSE(rpm_kernel_line("6.6.1"));
  EXPECT_FALSE(rpm_kernel_line("6.6.1-x86_64"));  // no release field
  EXPECT_FALSE(rpm_kernel_line("6.6.1-1.el9.sparc"));
  EXPECT_FALSE(rpm_kernel_line("-+.."));
}

TEST(PendingRebootLine, DashFlavours) {
  EXPECT_EQ(dash_kernel_line("6.1.0-18-amd64").id, dash_kernel_line("6.1.0-20-amd64").id);
  EXPECT_EQ(dash_kernel_line("6.12.38+deb13-amd64").id, dash_kernel_line("6.12.41+deb13-amd64").id);
  EXPECT_EQ(dash_kernel_line("5.15.0-91-generic").id, dash_kernel_line("6.8.0-45-generic").id);      // HWE
  EXPECT_EQ(dash_kernel_line("6.1.0-0.deb11.17-amd64").id, dash_kernel_line("5.10.0-28-amd64").id);  // backports
  EXPECT_NE(dash_kernel_line("6.1.0-18-amd64").id, dash_kernel_line("6.1.0-18-cloud-amd64").id);
  EXPECT_NE(dash_kernel_line("6.1.0-18-amd64").id, dash_kernel_line("6.1.0-18-rt-amd64").id);
  EXPECT_NE(dash_kernel_line("6.8.0-45-generic").id, dash_kernel_line("6.8.0-45-lowlatency").id);
  EXPECT_NE(dash_kernel_line("6.8.0-45-generic").id, dash_kernel_line("6.8.0-45-generic-64k").id);
  EXPECT_EQ(dash_kernel_line("6.4.0-150600.23.25-default").id, dash_kernel_line("6.4.0-150600.23.30-default").id);
  EXPECT_EQ(dash_kernel_line("6.6.31+rpt-rpi-v8").id, dash_kernel_line("6.6.51+rpt-rpi-v8").id);
}

TEST(PendingRebootLine, ArchPackagesAreSeparate) {
  EXPECT_EQ(dash_kernel_line("6.6.1-arch1-1").id, dash_kernel_line("6.6.2-arch2-1").id);
  EXPECT_EQ(dash_kernel_line("6.1.62-1-lts").id, dash_kernel_line("6.1.63-1-lts").id);
  EXPECT_NE(dash_kernel_line("6.6.1-arch1-1").id, dash_kernel_line("6.1.62-1-lts").id);
  EXPECT_NE(dash_kernel_line("6.6.1-arch1-1").id, dash_kernel_line("6.6.1-zen1-1-zen").id);
  EXPECT_NE(dash_kernel_line("6.6.1-arch1-1").id, dash_kernel_line("6.6.1-hardened1-1-hardened").id);
}

TEST(PendingRebootLine, UnplacedReleases) {
  EXPECT_FALSE(identify_line("").placed);
  EXPECT_FALSE(identify_line("linux").placed);
  EXPECT_FALSE(identify_line("6.1/x").placed);
  EXPECT_TRUE(identify_line("6.6.1").placed);
}

// --- evaluate_kernels (pure) ----------------------------------------------------------------

TEST(PendingRebootEvaluate, NewerSameLineKernelFires) {
  const kernel_verdict v = evaluate_kernels("6.8.0-40-generic", inventory_of({{"6.8.0-40-generic", 100}, {"6.8.0-45-generic", 200}}));
  EXPECT_TRUE(v.pending);
  EXPECT_FALSE(v.unknown);
  EXPECT_EQ(v.newest, "6.8.0-45-generic");
  EXPECT_EQ(v.reason, "newer kernel installed (6.8.0-45-generic, running 6.8.0-40-generic)");
  EXPECT_EQ(v.pending_since, 200);
}

TEST(PendingRebootEvaluate, RunningNewestIsClear) {
  const kernel_verdict v = evaluate_kernels("6.8.0-45-generic", inventory_of({{"6.8.0-40-generic", 100}, {"6.8.0-45-generic", 200}}));
  EXPECT_FALSE(v.pending);
  EXPECT_FALSE(v.unknown);
  EXPECT_EQ(v.newest, "6.8.0-45-generic");
}

TEST(PendingRebootEvaluate, ForeignRunningKernelIsClear) {
  // WSL2 / a netbooted kernel: not installed here, and nothing of its line is.
  const kernel_verdict v = evaluate_kernels("5.15.153.1-microsoft-standard-WSL2", inventory_of({{"6.8.0-45-generic", 1}}));
  EXPECT_FALSE(v.pending);
  EXPECT_EQ(v.newest, "");
}

TEST(PendingRebootEvaluate, ContainerIsClear) {
  kernel_inventory inv = inventory_of({{"6.20.1-100.fc43.x86_64", 1}});
  inv.container = true;
  const kernel_verdict v = evaluate_kernels("6.18.44-200.fc43.x86_64", inv);
  EXPECT_FALSE(v.pending);
  EXPECT_FALSE(v.unknown);
}

// Review point 4: pacman deletes the running kernel's modules on upgrade.
TEST(PendingRebootRegression, ArchRemovedRunningKernelFires) {
  const kernel_verdict v = evaluate_kernels("6.6.1-arch1-1", inventory_of({{"6.6.3-arch1-1", 300}, {"6.1.62-1-lts", 50}}));
  EXPECT_TRUE(v.pending);
  EXPECT_EQ(v.newest, "6.6.3-arch1-1");  // the lts kernel is another line
  EXPECT_EQ(v.reason, "running kernel 6.6.1-arch1-1 is no longer installed (newest installed: 6.6.3-arch1-1)");
  EXPECT_EQ(v.pending_since, 300);
}

// Review point 5: age must count from the first newer kernel, not the newest.
TEST(PendingRebootRegression, AgeCountsFromFirstNewerKernel) {
  const kernel_verdict v =
      evaluate_kernels("5.14.0-427.13.1.el9_4.x86_64",
                       inventory_of({{"5.14.0-427.13.1.el9_4.x86_64", 100}, {"5.14.0-427.42.1.el9_4.x86_64", 1000}, {"5.14.0-503.11.1.el9_5.x86_64", 9000}}));
  EXPECT_TRUE(v.pending);
  EXPECT_EQ(v.newest, "5.14.0-503.11.1.el9_5.x86_64");
  EXPECT_EQ(v.pending_since, 1000);
}

TEST(PendingRebootRegression, AgeIgnoresOlderKernelsAndOtherLines) {
  const kernel_verdict v = evaluate_kernels(
      "6.8.0-45-generic", inventory_of({{"6.8.0-40-generic", 10}, {"6.8.0-45-generic", 20}, {"6.8.0-50-lowlatency", 30}, {"6.8.0-49-generic", 40}}));
  EXPECT_EQ(v.pending_since, 40);
}

// Review point 7: a host pinned to a lower-versioned product line stays quiet...
TEST(PendingRebootRegression, PinnedProductLinesAreQuiet) {
  struct pin {
    const char *running;
    const char *other;
  };
  const pin cases[] = {
      {"5.14.0-427.13.1.el9_4.x86_64", "5.15.0-208.159.3.el9uek.x86_64"},    // stock pinned, newer UEK installed
      {"5.15.0-208.159.3.el9uek.x86_64", "5.15.0-427.13.1.el9_4.x86_64"},    // UEK pinned, stock "newer"
      {"4.18.0-513.24.1.el8_9.x86_64", "4.18.0-553.rt7.342.el8_10.x86_64"},  // real-time installed
      {"4.18.0-513.rt7.307.el8.x86_64", "4.18.0-553.el8_10.x86_64"},         // real-time pinned
      {"5.14.0-427.13.1.el9_4.x86_64", "5.14.0-503.11.1.el9_5.x86_64+rt"},
      {"5.14.0-427.13.1.el9_4.x86_64", "6.6.1-1.el9.elrepo.x86_64"},  // ELRepo installed
      {"6.6.1-1.el9.elrepo.x86_64", "6.7.0-503.el9.x86_64"},          // ELRepo pinned
      {"5.14.0-427.13.1.el9_4.x86_64", "5.14.0-503.11.1.el9_5.x86_64+debug"},
      {"5.14.0-427.13.1.el9_4.aarch64", "5.14.0-503.11.1.el9_5.aarch64+64k"},
      {"6.8.0-40-generic", "6.8.0-50-lowlatency"},
      {"6.1.0-18-amd64", "6.1.0-25-cloud-amd64"},
  };
  for (const pin &c : cases) {
    const kernel_verdict v = evaluate_kernels(c.running, inventory_of({{c.running, 1}, {c.other, 2}}));
    EXPECT_FALSE(v.pending) << c.running << " with " << c.other;
    EXPECT_EQ(v.newest, c.running) << c.running;
  }
}

// ...and a real update within each line still fires.
TEST(PendingRebootRegression, UpdatesWithinEachLineFire) {
  struct update {
    const char *running;
    const char *newer;
  };
  const update cases[] = {
      {"5.14.0-427.13.1.el9_4.x86_64", "5.14.0-503.11.1.el9_5.x86_64"},
      {"5.15.0-208.159.3.el9uek.x86_64", "5.15.0-209.161.7.el9uek.x86_64"},
      {"4.18.0-513.rt7.307.el8.x86_64", "4.18.0-553.rt7.342.el8_10.x86_64"},
      {"5.14.0-427.13.1.el9_4.x86_64+rt", "5.14.0-503.11.1.el9_5.x86_64+rt"},
      {"6.6.1-1.el9.elrepo.x86_64", "6.7.2-1.el9.elrepo.x86_64"},
      {"5.14.0-427.13.1.el9_4.aarch64+64k", "5.14.0-503.11.1.el9_5.aarch64+64k"},
      {"6.10.12-200.fc40.x86_64", "6.11.4-301.fc41.x86_64"},
      {"6.8.0-40-generic", "6.8.0-45-generic"},
      {"6.1.0-18-cloud-amd64", "6.1.0-25-cloud-amd64"},
      {"6.4.0-150600.23.25-default", "6.4.0-150600.23.30-default"},
  };
  for (const update &c : cases) {
    const kernel_verdict v = evaluate_kernels(c.running, inventory_of({{c.running, 1}, {c.newer, 2}}));
    EXPECT_TRUE(v.pending) << c.running << " -> " << c.newer;
    EXPECT_EQ(v.newest, c.newer);
  }
}

// Review point 8: what could not be read is unknown, never clear.
TEST(PendingRebootRegression, UnlistableModulesDirIsUnknown) {
  kernel_inventory inv;
  inv.error = "cannot list /lib/modules: Permission denied";
  const kernel_verdict v = evaluate_kernels("6.1.0-18-amd64", inv);
  EXPECT_FALSE(v.pending);
  EXPECT_TRUE(v.unknown);
  EXPECT_EQ(v.error, "cannot list /lib/modules: Permission denied");
}

TEST(PendingRebootRegression, UnreadableNewerImageIsUnknown) {
  kernel_inventory inv;
  inv.kernels["6.1.0-18-amd64"] = kernel_image::make_unreadable("cannot read /boot/vmlinuz-6.1.0-18-amd64: Permission denied");
  inv.kernels["6.1.0-25-amd64"] = kernel_image::make_unreadable("cannot read /boot/vmlinuz-6.1.0-25-amd64: Permission denied");
  const kernel_verdict v = evaluate_kernels("6.1.0-18-amd64", inv);
  EXPECT_TRUE(v.unknown);
  EXPECT_FALSE(v.pending);
  EXPECT_NE(v.error.find("Permission denied"), std::string::npos);
}

TEST(PendingRebootRegression, UnreadableImagesWithoutNewerCandidatesAreClear) {
  // Nothing newer has even a modules directory: no image can change that.
  kernel_inventory inv;
  inv.kernels["6.1.0-18-amd64"] = kernel_image::make_unreadable("cannot read /boot/vmlinuz-6.1.0-18-amd64: Permission denied");
  inv.kernels["6.1.0-9-amd64"] = kernel_image::make_unreadable("cannot read /boot/vmlinuz-6.1.0-9-amd64: Permission denied");
  const kernel_verdict v = evaluate_kernels("6.1.0-18-amd64", inv);
  EXPECT_FALSE(v.unknown);
  EXPECT_FALSE(v.pending);
}

TEST(PendingRebootRegression, MissingRunningImageMakesNewerDirectoryUnknown) {
  // /boot not mounted: neither the running nor the newer image is found, so
  // the newer modules directory cannot be written off as a DKMS leftover.
  kernel_inventory inv;
  inv.kernels["6.1.0-18-amd64"] = kernel_image();
  inv.kernels["6.1.0-25-amd64"] = kernel_image();
  const kernel_verdict v = evaluate_kernels("6.1.0-18-amd64", inv);
  EXPECT_TRUE(v.unknown);
  EXPECT_NE(v.error.find("is /boot mounted?"), std::string::npos);
}

TEST(PendingRebootRegression, NewerImageWinsOverUnknowns) {
  kernel_inventory inv = inventory_of({{"6.1.0-18-amd64", 1}, {"6.1.0-25-amd64", 5}});
  inv.kernels["6.1.0-30-amd64"] = kernel_image::make_unreadable("cannot read ...");
  const kernel_verdict v = evaluate_kernels("6.1.0-18-amd64", inv);
  EXPECT_TRUE(v.pending);
  EXPECT_FALSE(v.unknown);
}

// --- container detection ----------------------------------------------------------------------

TEST(PendingRebootContainer, RootFsType) {
  const std::string host =
      "22 1 8:2 / / rw,relatime shared:1 - ext4 /dev/sda2 rw\n"
      "23 22 0:21 / /proc rw,nosuid shared:12 - proc proc rw\n";
  EXPECT_EQ(root_fs_type(host), "ext4");
  EXPECT_EQ(root_fs_type("1 0 0:1 / / rw - overlay overlay rw,lowerdir=/a\n"), "overlay");
  // No optional fields; two mounts on / - the last one is in effect.
  EXPECT_EQ(root_fs_type("1 0 8:1 / / rw - ext4 /dev/sda1 rw\n9 1 0:5 / / rw master:3 - overlay overlay rw\n"), "overlay");
  EXPECT_EQ(root_fs_type(""), "");
  EXPECT_EQ(root_fs_type("garbage\n1 2 3\n- - - - / - -\n"), "");
  EXPECT_EQ(root_fs_type("1 0 8:1 / / rw shared:1 -"), "");  // separator without a type
}

TEST(PendingRebootContainer, EnvironNamesContainer) {
  EXPECT_TRUE(environ_names_container("PATH=/bin\0container=lxc\0"s));
  EXPECT_TRUE(environ_names_container("container=podman"));
  EXPECT_FALSE(environ_names_container("PATH=/bin\0container=\0"s));  // empty
  EXPECT_FALSE(environ_names_container("xcontainer=lxc\0"s));
  EXPECT_FALSE(environ_names_container(""));
}

TEST(PendingRebootContainer, Markers) {
  const char *const markers[] = {"run/systemd/container", "run/.containerenv", ".dockerenv"};
  for (const char *m : markers) {
    FakeRoot fs;
    fs.write(m, "docker\n");
    EXPECT_TRUE(detect_container(fs.root())) << m;
  }
  {
    FakeRoot fs;
    EXPECT_FALSE(detect_container(fs.root()));
  }
  {
    FakeRoot fs;
    fs.mkdir("proc/vz");
    EXPECT_TRUE(detect_container(fs.root()));  // OpenVZ guest
    fs.mkdir("proc/bc");
    EXPECT_FALSE(detect_container(fs.root()));  // OpenVZ host
  }
  {
    FakeRoot fs;
    fs.write("proc/1/environ", "HOME=/\0container=lxc\0"s);
    EXPECT_TRUE(detect_container(fs.root()));
  }
  {
    FakeRoot fs;
    fs.write("proc/self/mountinfo", "1 0 0:1 / / rw - overlay overlay rw\n");
    EXPECT_TRUE(detect_container(fs.root()));
  }
}

// --- inventory from fixture trees ------------------------------------------------------------

TEST(PendingRebootInventory, ImageNeedsPositiveEvidence) {
  FakeRoot fs;
  fs.debian_kernel("6.1.0-18-amd64");
  fs.mkdir("lib/modules/6.1.0-25-amd64/updates/dkms");  // DKMS leftover: no image
  fs.write("boot/vmlinuz-6.1.0-30-amd64", "image");     // image without modules
  const kernel_inventory inv = read_kernel_inventory(fs.root());
  ASSERT_EQ(inv.kernels.size(), 2u);
  EXPECT_EQ(inv.kernels.at("6.1.0-18-amd64").state, kernel_image::present);
  EXPECT_EQ(inv.kernels.at("6.1.0-25-amd64").state, kernel_image::absent);
}

TEST(PendingRebootInventory, FindImageLayouts) {
  FakeRoot fs;
  fs.rhel_kernel("5.14.0-427.el9.x86_64");
  fs.arch_kernel("6.6.3-arch1-1");
  fs.debian_kernel("6.1.0-18-amd64");
  EXPECT_EQ(find_kernel_image(fs.root(), "5.14.0-427.el9.x86_64").state, kernel_image::present);
  EXPECT_EQ(find_kernel_image(fs.root(), "6.6.3-arch1-1").state, kernel_image::present);
  EXPECT_EQ(find_kernel_image(fs.root(), "6.1.0-18-amd64").state, kernel_image::present);
  EXPECT_EQ(find_kernel_image(fs.root(), "6.1.0-99-amd64").state, kernel_image::absent);
  const long long before = static_cast<long long>(std::time(nullptr));
  EXPECT_GE(find_kernel_image(fs.root(), "6.1.0-18-amd64").installed, before - 60);
}

TEST(PendingRebootInventory, ModulesDirsAreMergedAndBounded) {
  FakeRoot fs;
  fs.rhel_kernel("5.14.0-427.el9.x86_64");
  fs.write("usr/lib/modules/5.14.0-503.el9.x86_64/vmlinuz", "image");
  fs.write("lib/modules/not-a-release/vmlinuz", "x");  // ignored
  fs.write("lib/modules/6.0.0-1-amd64", "a file, not a directory");
  const kernel_inventory inv = read_kernel_inventory(fs.root());
  EXPECT_EQ(inv.kernels.size(), 2u);
  EXPECT_TRUE(inv.error.empty());
}

TEST(PendingRebootInventory, UnlistableModulesDirIsAnError) {
  FakeRoot fs;
  fs.loop("lib/modules");  // exists, but every lookup through it fails (ELOOP)
  const kernel_inventory inv = read_kernel_inventory(fs.root());
  EXPECT_NE(inv.error.find("cannot list /lib/modules"), std::string::npos) << inv.error;
}

TEST(PendingRebootInventory, UnreadableBootIsUnreadableNotAbsent) {
  FakeRoot fs;
  fs.mkdir("lib/modules/6.1.0-18-amd64");
  fs.loop("boot");
  const kernel_image k = find_kernel_image(fs.root(), "6.1.0-18-amd64");
  EXPECT_EQ(k.state, kernel_image::unreadable);
  EXPECT_NE(k.error.find("/boot/vmlinuz-6.1.0-18-amd64"), std::string::npos) << k.error;
}

TEST(PendingRebootInventory, PermissionDeniedBoot) {
  // The literal mode-0700 /boot case; root bypasses the mode, so this half
  // only runs unprivileged (the ELOOP tests above cover the logic as root).
  if (::geteuid() == 0) GTEST_SKIP() << "root ignores directory modes";
  FakeRoot fs;
  fs.debian_kernel("6.1.0-18-amd64");
  fs.debian_kernel("6.1.0-25-amd64");
  fs.chmod("boot", 0);
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "6.1.0-18-amd64", kNow);
  EXPECT_TRUE(o.kernel_unknown);
  EXPECT_FALSE(o.get_kernel());
}

// --- package list ------------------------------------------------------------------

TEST(PendingRebootPackages, ParsesAndDedups) {
  const std::vector<std::string> p = parse_reboot_packages("linux-image-6.8.0-45-generic\nlibc6\n\nlinux-base\nlibc6\n  dbus \r\nlibssl3t64:amd64\n");
  EXPECT_EQ(p, (std::vector<std::string>{"linux-image-6.8.0-45-generic", "libc6", "linux-base", "dbus", "libssl3t64:amd64"}));
}

TEST(PendingRebootPackages, InvalidLinesAreSkippedNotMangled) {
  const std::vector<std::string> p = parse_reboot_packages(
      "ok1\nbad\x01name\nbad\xff"
      "name\nbad name\n$(reboot)\n" +
      std::string(200, 'a') + "\nok2");
  EXPECT_EQ(p, (std::vector<std::string>{"ok1", "ok2"}));
}

TEST(PendingRebootPackages, TruncatedContentDropsThePartialLine) {
  EXPECT_EQ(parse_reboot_packages("libc6\nlinux-ima", true), (std::vector<std::string>{"libc6"}));
  EXPECT_EQ(parse_reboot_packages("libc6\nlinux-ima", false), (std::vector<std::string>{"libc6", "linux-ima"}));
  EXPECT_TRUE(parse_reboot_packages("", true).empty());
}

TEST(PendingRebootPackages, CapsTheList) {
  std::string content;
  for (int i = 0; i < 1000; ++i) content += "pkg" + std::to_string(i) + "\n";
  EXPECT_EQ(parse_reboot_packages(content).size(), 256u);
  EXPECT_TRUE(parse_reboot_packages(std::string(1000, '\n')).empty());
}

// --- gathering from fixture trees -----------------------------------------------------

TEST(PendingRebootGather, ContainerHasNothingPending) {
  FakeRoot fs;
  fs.write("run/systemd/container", "docker\n");
  fs.mkdir("boot");
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "6.18.44-fc-v819", kNow);
  EXPECT_FALSE(o.any());
  EXPECT_EQ(o.get_kernel().value(), 0);
  EXPECT_EQ(o.newest_kernel, "");
  EXPECT_EQ(o.get_message(), "No reboot pending");
}

TEST(PendingRebootGather, ContainerImageWithKernelPackageIsNotCompared) {
  // An image that installed a kernel package of the host's line: detected as
  // a container, so neither "newer installed" nor "running removed" fires.
  FakeRoot fs;
  fs.write(".dockerenv");
  fs.rhel_kernel("6.20.1-100.fc43.x86_64");
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "6.18.44-200.fc43.x86_64", kNow);
  EXPECT_FALSE(o.any());
}

TEST(PendingRebootGather, UbuntuMarkerWithPackages) {
  FakeRoot fs;
  fs.debian_kernel("6.8.0-45-generic");
  fs.write("run/reboot-required", "*** System restart required ***\n");
  fs.write("run/reboot-required.pkgs", "linux-image-6.8.0-45-generic\nlibc6\nlibc6\n");
  fs.set_mtime("run/reboot-required", kNow - 3 * 86400);
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "6.8.0-45-generic", kNow);
  EXPECT_TRUE(o.marker);
  EXPECT_FALSE(o.kernel);
  EXPECT_EQ(o.get_packages(), "linux-image-6.8.0-45-generic, libc6");
  EXPECT_EQ(o.pending_since, kNow - 3 * 86400);
  EXPECT_EQ(o.get_age().value(), 3 * 86400);
  EXPECT_EQ(o.get_reasons(), "reboot-required marker (linux-image-6.8.0-45-generic, libc6)");
}

TEST(PendingRebootGather, LegacyVarRunAndSuseMarkers) {
  {
    FakeRoot fs;
    fs.write("var/run/reboot-required");
    const reboot_obj o = gather_pending_reboot_from(fs.root(), "6.1.0-18-amd64", kNow);
    EXPECT_TRUE(o.marker);
    EXPECT_EQ(o.get_reasons(), "reboot-required marker");
  }
  {
    FakeRoot fs;
    fs.write("run/reboot-needed");
    const reboot_obj o = gather_pending_reboot_from(fs.root(), "6.4.0-150600.23.25-default", kNow);
    EXPECT_EQ(o.marker_name, "reboot-needed");
    EXPECT_EQ(o.get_reasons(), "reboot-needed marker");
  }
}

TEST(PendingRebootGather, UnreadableMarkerIsUnknown) {
  FakeRoot fs;
  fs.loop("run");
  fs.rhel_kernel("5.14.0-427.el9.x86_64");
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "5.14.0-427.el9.x86_64", kNow);
  EXPECT_FALSE(o.marker);
  EXPECT_TRUE(o.marker_unknown);
  EXPECT_FALSE(o.get_marker());
  EXPECT_TRUE(o.has_unknown());
  EXPECT_NE(o.get_unreadable().find("/run/reboot-required"), std::string::npos) << o.get_unreadable();
}

TEST(PendingRebootGather, DebianNewerKernelInstalled) {
  FakeRoot fs;
  fs.debian_kernel("6.1.0-9-amd64");
  fs.debian_kernel("6.1.0-18-amd64");
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "6.1.0-9-amd64", kNow);
  EXPECT_TRUE(o.kernel);
  EXPECT_EQ(o.newest_kernel, "6.1.0-18-amd64");
  EXPECT_EQ(o.get_reasons(), "newer kernel installed (6.1.0-18-amd64, running 6.1.0-9-amd64)");
  EXPECT_GT(o.pending_since, 0);
}

TEST(PendingRebootGather, RhelNewerKernelInModulesLayout) {
  FakeRoot fs;
  fs.rhel_kernel("5.14.0-427.13.1.el9_4.x86_64");
  fs.write("usr/lib/modules/5.14.0-503.11.1.el9_5.x86_64/vmlinuz", "image");
  fs.write("boot/vmlinuz-0-rescue-0123456789abcdef", "rescue");
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "5.14.0-427.13.1.el9_4.x86_64", kNow);
  EXPECT_TRUE(o.kernel);
  EXPECT_EQ(o.newest_kernel, "5.14.0-503.11.1.el9_5.x86_64");
}

TEST(PendingRebootGather, RhelNeedsNoBoot) {
  // The RHEL/Fedora image lives in the world-readable modules directory, so an
  // agent that may not read /boot still gets an answer.
  FakeRoot fs;
  fs.write("lib/modules/5.14.0-427.el9.x86_64/vmlinuz", "image");
  fs.write("lib/modules/5.14.0-503.el9.x86_64/vmlinuz", "image");
  fs.loop("boot");
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "5.14.0-427.el9.x86_64", kNow);
  EXPECT_TRUE(o.kernel);
  EXPECT_FALSE(o.kernel_unknown);
}

// Review point 4, end to end on an Arch layout.
TEST(PendingRebootRegression, ArchLayoutAfterUpgrade) {
  FakeRoot fs;
  fs.arch_kernel("6.6.3-arch1-1");  // the running 6.6.1-arch1-1 was removed by pacman
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "6.6.1-arch1-1", kNow);
  EXPECT_TRUE(o.kernel);
  EXPECT_EQ(o.get_reasons(), "running kernel 6.6.1-arch1-1 is no longer installed (newest installed: 6.6.3-arch1-1)");
  EXPECT_GT(o.pending_since, 0);
}

TEST(PendingRebootRegression, ArchLayoutUpToDate) {
  FakeRoot fs;
  fs.arch_kernel("6.6.3-arch1-1");
  fs.arch_kernel("6.1.62-1-lts", "linux-lts");
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "6.6.3-arch1-1", kNow);
  EXPECT_FALSE(o.any());
  EXPECT_EQ(o.newest_kernel, "6.6.3-arch1-1");
}

// Review point 6: a stray copy in /boot is not an installed kernel.
TEST(PendingRebootRegression, StrayBootCopiesAreNotKernels) {
  const char *const strays[] = {".orig", ".old", ".bak", ".rpmsave", "~", ".sig", ".efi.signed"};
  for (const char *suffix : strays) {
    FakeRoot fs;
    fs.rhel_kernel("5.14.0-427.13.1.el9_4.x86_64");
    fs.write(std::string("boot/vmlinuz-5.14.0-427.13.1.el9_4.x86_64") + suffix, "copy");
    const reboot_obj o = gather_pending_reboot_from(fs.root(), "5.14.0-427.13.1.el9_4.x86_64", kNow);
    EXPECT_FALSE(o.any()) << suffix;
    EXPECT_FALSE(o.kernel_unknown) << suffix;
  }
}

// Review point 8, end to end: the marker still reports, the kernel is unknown.
TEST(PendingRebootRegression, MarkerStillReportedWhenKernelsUnreadable) {
  FakeRoot fs;
  fs.loop("lib/modules");
  fs.write("run/reboot-required");
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "6.1.0-18-amd64", kNow);
  EXPECT_TRUE(o.marker);
  EXPECT_TRUE(o.kernel_unknown);
  EXPECT_FALSE(o.get_kernel());
  EXPECT_NE(o.get_message().find("; the kernel inventory could not be read (cannot list /lib/modules"), std::string::npos) << o.get_message();
}

TEST(PendingRebootGather, BothSignalsTakeOldestTimestamp) {
  FakeRoot fs;
  fs.debian_kernel("6.8.0-40-generic");
  fs.debian_kernel("6.8.0-45-generic");
  fs.write("run/reboot-required");
  fs.set_mtime("run/reboot-required", 1000);
  const reboot_obj o = gather_pending_reboot_from(fs.root(), "6.8.0-40-generic", kNow);
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
  o.kernel_reason = "newer kernel installed (6.1.0-18-amd64, running 6.1.0-9-amd64)";
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

// The unreadable-inventory contract.
TEST(PendingRebootCheck, UnreadableKernelsWithNothingElseIsUnknown) {
  reboot_obj o;
  o.now = kNow;
  o.kernel_unknown = true;
  o.kernel_error = "cannot read /boot/vmlinuz-6.1.0-25-amd64: Permission denied";
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(o, {}, r), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(join_lines(r),
            "Cannot tell whether a reboot is pending: the kernel inventory could not be read (cannot read /boot/vmlinuz-6.1.0-25-amd64: Permission "
            "denied); run the agent with read access to it or pass allow-unreadable");
}

TEST(PendingRebootCheck, AllowUnreadableReportsTheRest) {
  reboot_obj o;
  o.now = kNow;
  o.kernel_unknown = true;
  o.kernel_error = "cannot list /lib/modules: Permission denied";
  // The REST spelling of a valued boolean.
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(o, {"allow-unreadable=true", "detail-syntax=${message} kernel=${kernel}"}, r), PB::Common::ResultCode::OK);
  EXPECT_EQ(join_lines(r),
            "OK: No reboot signal is set, but the kernel inventory could not be read (cannot list /lib/modules: Permission denied) kernel=unknown");
  // An unknown kernel never trips a numeric threshold, and can be tested for.
  PB::Commands::QueryResponseMessage::Response r2;
  EXPECT_EQ(run_check(o, {"allow-unreadable", "crit=kernel = 1 or kernel = 0"}, r2), PB::Common::ResultCode::OK);
  PB::Commands::QueryResponseMessage::Response r3;
  EXPECT_EQ(run_check(o, {"allow-unreadable=true", "crit=kernel = 'unknown'"}, r3), PB::Common::ResultCode::CRITICAL);
}

TEST(PendingRebootCheck, SignalStillWarnsWhenKernelsUnreadable) {
  reboot_obj o;
  o.now = kNow;
  o.marker = true;
  o.marker_name = "reboot-required";
  o.kernel_unknown = true;
  o.kernel_error = "cannot list /lib/modules: Permission denied";
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(o, {}, r), PB::Common::ResultCode::WARNING);
  EXPECT_EQ(join_lines(r),
            "WARNING: Reboot required: reboot-required marker; the kernel inventory could not be read (cannot list /lib/modules: Permission denied)");
}
