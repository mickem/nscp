// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "path_manager.hpp"

#include <config.h>
#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <memory>
#include <nsclient/logger/logger.hpp>
#include <nscp/boot_layout.hpp>
#include <nscp/layout_migration.hpp>

class MockLogger : public nsclient::logging::log_interface {
 public:
  void trace(const std::string &module, const char *file, const int line, const std::string &message) override {}
  void debug(const std::string &module, const char *file, const int line, const std::string &message) override {}
  void info(const std::string &module, const char *file, const int line, const std::string &message) override {}
  void warning(const std::string &module, const char *file, const int line, const std::string &message) override {}
  void error(const std::string &module, const char *file, const int line, const std::string &message) override {}
  void critical(const std::string &module, const char *file, const int line, const std::string &message) override {}

  bool should_trace() const override { return false; }
  bool should_debug() const override { return false; }
  bool should_info() const override { return false; }
  bool should_warning() const override { return false; }
  bool should_error() const override { return false; }
  bool should_critical() const override { return false; }
};

class PathManagerTest : public ::testing::Test {
 protected:
  nsclient::logging::log_client_accessor log_instance_;
  nsclient::core::path_manager *pm;

  void SetUp() override {
    log_instance_ = std::make_shared<MockLogger>();
    pm = new nsclient::core::path_manager(log_instance_);
  }

  void TearDown() override { delete pm; }
};

TEST_F(PathManagerTest, ExpandPathEmpty) { EXPECT_EQ(pm->expand_path(""), ""); }

TEST_F(PathManagerTest, ExpandPathNoVariables) {
  std::string path = "/usr/local/bin";
  EXPECT_EQ(pm->expand_path(path), path);
}

TEST_F(PathManagerTest, GetFolderUnknownKey) {
  std::string key = "unknown-key";
  std::string result = pm->getFolder(key);
  EXPECT_FALSE(result.empty());
}

TEST_F(PathManagerTest, ExpandPathWithVariables) {
  std::string input = "${base-path}/config";
  std::string output = pm->expand_path(input);
  EXPECT_NE(output, input);
  EXPECT_TRUE(output.find("config") != std::string::npos);
}

TEST_F(PathManagerTest, GetFolderKeys) {
  const char *keys[] = {"certificate-path", "module-path", "web-path",    "scripts",  "log-path",  "cache-folder", "crash-folder",
                        "base-path",        "temp",        "shared-path", "exe-path", "data-path", "ca-path",      "fleet-folder"};

  for (const auto &key : keys) {
    EXPECT_FALSE(pm->getFolder(key).empty()) << "Failed for key: " << key;
  }
}

// --- layout migration --------------------------------------------------------
// The mover the CLI and (later) the MSI both call. It classifies what travels,
// what belongs to the package, and what is regenerated - getting that wrong
// either strands the agent's identity or takes shipped files out of the
// installer's hands, so the rules are pinned here rather than exercised only
// through a real migration.

class LayoutMigrationTest : public ::testing::Test {
 protected:
  boost::filesystem::path root_, from_, to_;

  void SetUp() override {
    root_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-migrate-%%%%-%%%%");
    from_ = root_ / "install";
    to_ = root_ / "shared";
    boost::filesystem::create_directories(from_ / "security");
    boost::filesystem::create_directories(to_);
  }
  void TearDown() override {
    boost::system::error_code ignored;
    boost::filesystem::remove_all(root_, ignored);
  }

  void write(const boost::filesystem::path &path, const std::string &content) {
    boost::filesystem::create_directories(path.parent_path());
    std::ofstream out(path.string().c_str());
    out << content;
  }
  std::string read(const boost::filesystem::path &path) {
    std::ifstream in(path.string().c_str());
    std::string content;
    std::getline(in, content);
    return content;
  }
  bool exists(const boost::filesystem::path &path) {
    boost::system::error_code ignored;
    return boost::filesystem::exists(path, ignored);
  }
  // The action recorded for `name`, or `absent` when the step is missing.
  nscp::paths::migration_action action_for(const nscp::paths::migration_report &report, const std::string &name) {
    for (const nscp::paths::migration_step &step : report.steps) {
      if (step.name == name) return step.action;
    }
    return nscp::paths::migration_action::absent;
  }
};

TEST_F(LayoutMigrationTest, MovesPerMachineStateAndLeavesTheProgramAlone) {
  write(from_ / "nsclient.ini", "[/settings/default]");
  write(from_ / "security" / "certificate.pem", "CERT");
  write(from_ / "security" / "certificate_key.pem", "KEY");
  write(from_ / "security" / "agent-state.json", "IDENTITY");
  write(from_ / "security" / "nrpe_dh_2048.pem", "DH");
  write(from_ / "security" / "windows-ca.pem", "STALE-TRUST");
  write(from_ / "security" / "an-admins-own-ca.pem", "CUSTOM");
  write(from_ / "fleet" / "fleet.ini", "MANAGED");
  write(from_ / "cache" / "bundle.zip", "CACHED");

  const nscp::paths::migration_report report = nscp::paths::apply_migration(from_.string(), to_.string());
  ASSERT_TRUE(report.ok());

  EXPECT_EQ(read(to_ / "nsclient.ini"), "[/settings/default]");
  EXPECT_EQ(read(to_ / "security" / "certificate.pem"), "CERT");
  EXPECT_EQ(read(to_ / "security" / "certificate_key.pem"), "KEY");
  EXPECT_EQ(read(to_ / "security" / "agent-state.json"), "IDENTITY");
  EXPECT_EQ(read(to_ / "fleet" / "fleet.ini"), "MANAGED");
  EXPECT_EQ(read(to_ / "cache" / "bundle.zip"), "CACHED");
  EXPECT_FALSE(exists(from_ / "nsclient.ini"));

  // Shipped with the package: moving it takes it out of the installer's hands.
  EXPECT_EQ(read(from_ / "security" / "nrpe_dh_2048.pem"), "DH");
  EXPECT_FALSE(exists(to_ / "security" / "nrpe_dh_2048.pem"));
  EXPECT_EQ(action_for(report, "security/nrpe_dh_2048.pem"), nscp::paths::migration_action::kept);

  // Re-exported at every start, and a stale trust bundle decides which CAs the
  // agent trusts - so it is dropped rather than carried across.
  EXPECT_FALSE(exists(from_ / "security" / "windows-ca.pem"));
  EXPECT_FALSE(exists(to_ / "security" / "windows-ca.pem"));
  EXPECT_EQ(action_for(report, "security/windows-ca.pem"), nscp::paths::migration_action::dropped);

  // Anything an admin put there themselves travels: stranding a trust anchor
  // breaks TLS quietly, so the rule is move-unless-known-otherwise.
  EXPECT_EQ(read(to_ / "security" / "an-admins-own-ca.pem"), "CUSTOM");
}

TEST_F(LayoutMigrationTest, NeverOverwritesWhatIsAlreadyAtTheDestination) {
  write(from_ / "security" / "agent-state.json", "OLD-IDENTITY");
  write(to_ / "security" / "agent-state.json", "LIVE-IDENTITY");

  const nscp::paths::migration_report report = nscp::paths::apply_migration(from_.string(), to_.string());
  EXPECT_TRUE(report.ok()) << "an occupied destination is a normal outcome, not a failure";
  EXPECT_EQ(read(to_ / "security" / "agent-state.json"), "LIVE-IDENTITY");
  EXPECT_EQ(action_for(report, "security/agent-state.json"), nscp::paths::migration_action::blocked);
}

TEST_F(LayoutMigrationTest, IsIdempotent) {
  write(from_ / "nsclient.ini", "CONFIG");
  write(from_ / "security" / "agent-state.json", "IDENTITY");
  ASSERT_TRUE(nscp::paths::apply_migration(from_.string(), to_.string()).ok());
  // A second run - a repeated command, or a resumed half-finished migration.
  EXPECT_TRUE(nscp::paths::apply_migration(from_.string(), to_.string()).ok());
  EXPECT_EQ(read(to_ / "nsclient.ini"), "CONFIG");
  EXPECT_EQ(read(to_ / "security" / "agent-state.json"), "IDENTITY");
}

TEST_F(LayoutMigrationTest, DryRunChangesNothing) {
  write(from_ / "nsclient.ini", "CONFIG");
  write(from_ / "security" / "windows-ca.pem", "STALE");

  const nscp::paths::migration_report plan = nscp::paths::plan_migration(from_.string(), to_.string());
  EXPECT_TRUE(plan.ok());
  EXPECT_EQ(action_for(plan, "nsclient.ini"), nscp::paths::migration_action::moved);
  EXPECT_EQ(action_for(plan, "security/windows-ca.pem"), nscp::paths::migration_action::dropped);
  // ...but nothing actually happened, including the drop.
  EXPECT_EQ(read(from_ / "nsclient.ini"), "CONFIG");
  EXPECT_EQ(read(from_ / "security" / "windows-ca.pem"), "STALE");
  EXPECT_FALSE(exists(to_ / "nsclient.ini"));
}

TEST_F(LayoutMigrationTest, DryRunWorksBeforeTheDestinationExists) {
  // The preview has to work before the caller has created and secured the
  // destination, or it cannot be used to decide whether to go ahead.
  write(from_ / "nsclient.ini", "CONFIG");
  const boost::filesystem::path missing = root_ / "not-created-yet";
  const nscp::paths::migration_report plan = nscp::paths::plan_migration(from_.string(), missing.string());
  EXPECT_TRUE(plan.ok());
  EXPECT_EQ(action_for(plan, "nsclient.ini"), nscp::paths::migration_action::moved);
}

TEST_F(LayoutMigrationTest, RefusesToMoveIntoADestinationNobodyPrepared) {
  // Creating it here would put the configuration and the private key somewhere
  // that has not been locked down yet.
  write(from_ / "nsclient.ini", "CONFIG");
  const boost::filesystem::path missing = root_ / "not-created-yet";
  const nscp::paths::migration_report report = nscp::paths::apply_migration(from_.string(), missing.string());
  EXPECT_FALSE(report.ok());
  EXPECT_EQ(read(from_ / "nsclient.ini"), "CONFIG");
}

TEST_F(LayoutMigrationTest, KeepsBootIniWithTheExecutable) {
  write(from_ / "boot.ini", "[layout]");
  const nscp::paths::migration_report report = nscp::paths::apply_migration(from_.string(), to_.string());
  ASSERT_TRUE(report.ok());
  // boot.ini is what tells the agent where the shared folder is, so it cannot
  // live inside it.
  EXPECT_EQ(read(from_ / "boot.ini"), "[layout]");
  EXPECT_FALSE(exists(to_ / "boot.ini"));
  EXPECT_EQ(action_for(report, "boot.ini"), nscp::paths::migration_action::kept);
}

TEST_F(LayoutMigrationTest, FailsRatherThanReportingAnEmptyMigrationItCouldNotRead) {
  // Every individual check treats an unreadable entry as "nothing there", so an
  // inaccessible source folder would otherwise produce a clean report listing
  // no work - and the caller would switch the layout having moved nothing.
  const boost::filesystem::path missing = root_ / "no-such-install";
  const nscp::paths::migration_report report = nscp::paths::apply_migration(missing.string(), to_.string());
  EXPECT_FALSE(report.ok());
  EXPECT_TRUE(report.has(nscp::paths::migration_action::failed));
}

TEST_F(LayoutMigrationTest, SameFolderIsANoOp) {
  write(from_ / "nsclient.ini", "CONFIG");
  const nscp::paths::migration_report report = nscp::paths::apply_migration(from_.string(), from_.string());
  EXPECT_TRUE(report.ok());
  EXPECT_EQ(read(from_ / "nsclient.ini"), "CONFIG");
}

// --- the table and expander shared with the standalone clients ---------------
// check_nrpe / check_nscp cannot link path_manager, so they resolve tokens from
// the same header. These pin the contract both sides rely on.

TEST(PathDefaults, ExpandsATokenThatIsNotAtTheStartOfTheString) {
  // The clients' previous hand-rolled expander extracted the key with
  // `substr(pstart + 1, pend - 2)`, which is only correct for a leading token.
  // A path like this yielded the key "etc}/x.pem", which resolved to the
  // executable's directory - a plausible-looking wrong answer, never an error.
  const auto resolve = [](const std::string &key) { return key == "etc" ? std::string("/etc/nsclient") : std::string("<unknown:" + key + ">"); };
  EXPECT_EQ(nscp::paths::expand_tokens("${etc}/x.pem", resolve), "/etc/nsclient/x.pem");
  EXPECT_EQ(nscp::paths::expand_tokens("/opt/nscp/${etc}/x.pem", resolve), "/opt/nscp//etc/nsclient/x.pem");
  EXPECT_EQ(nscp::paths::expand_tokens("cfg-${etc}-suffix", resolve), "cfg-/etc/nsclient-suffix");
}

TEST(PathDefaults, ExpandsChainedTokensAndSurvivesCycles) {
  const auto resolve = [](const std::string &key) {
    if (key == "a") return std::string("${b}/one");
    if (key == "b") return std::string("/root");
    if (key == "loop") return std::string("${loop}");
    return std::string();
  };
  EXPECT_EQ(nscp::paths::expand_tokens("${a}/two", resolve), "/root/one/two");
  // A self-referential token must terminate rather than spin to the limit.
  EXPECT_EQ(nscp::paths::expand_tokens("${loop}", resolve), "${loop}");
  // Unterminated tokens are left alone rather than swallowing the rest.
  EXPECT_EQ(nscp::paths::expand_tokens("${unclosed/x", resolve), "${unclosed/x");
}

TEST(PathDefaults, LayoutOnlyMovesSharedPathAndOnlyOnWindows) {
  const std::string legacy = nscp::paths::default_for("shared-path", nscp::paths::layout::legacy);
  const std::string modern = nscp::paths::default_for("shared-path", nscp::paths::layout::modern);
#ifdef WIN32
  // Legacy has no static default: the caller answers with its own directory.
  EXPECT_TRUE(legacy.empty());
  EXPECT_EQ(modern, "${common-appdata}/NSClient++");
#else
  // Unix decides its layout from the package prefix; the switch does nothing.
  EXPECT_EQ(legacy, modern);
  EXPECT_FALSE(legacy.empty());
#endif
  // Everything else is expressed relative to shared-path, so it must not vary.
  for (const char *key : {"certificate-path", "module-path", "web-path", "scripts", "log-path", FLEET_FOLDER_KEY}) {
    EXPECT_EQ(nscp::paths::default_for(key, nscp::paths::layout::legacy), nscp::paths::default_for(key, nscp::paths::layout::modern)) << key;
  }
}

// How an upgrade keeps the layout it already has: with no LAYOUT property to
// go on, the installer (and each standalone client) reads the answer back out
// of the host's own boot.ini. Getting this wrong would move an installation
// that never asked to move, or move a modern one back to legacy.
TEST(BootLayout, ReadsTheModeFromBootIni) {
  const boost::filesystem::path dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-boot-%%%%");
  boost::filesystem::create_directories(dir);
  const boost::filesystem::path boot_ini = dir / "boot.ini";

  const auto write = [&boot_ini](const std::string &content) {
    std::ofstream out(boot_ini.string().c_str());
    out << content;
  };

  write("[layout]\nmode = modern\n");
  EXPECT_EQ(nscp::paths::layout_from_boot_ini_file(boot_ini.string()), nscp::paths::layout::modern);

  write("[layout]\nmode = legacy\n");
  EXPECT_EQ(nscp::paths::layout_from_boot_ini_file(boot_ini.string()), nscp::paths::layout::legacy);

  // A boot.ini that predates the setting, which is every existing install.
  write("[settings]\n0 = ini://${shared-path}/nsclient.ini\n");
  EXPECT_EQ(nscp::paths::layout_from_boot_ini_file(boot_ini.string()), nscp::paths::layout::legacy);

  // The raw value comes back too, so a caller can tell "asked for legacy" from
  // "asked for something we do not understand" and say so.
  std::string raw;
  write("[layout]\nmode = moderne\n");
  EXPECT_EQ(nscp::paths::layout_from_boot_ini_file(boot_ini.string(), &raw), nscp::paths::layout::legacy);
  EXPECT_EQ(raw, "moderne");
  EXPECT_FALSE(nscp::paths::is_known_layout(raw));

  boost::system::error_code ignored;
  boost::filesystem::remove_all(dir, ignored);
}

TEST(BootLayout, NoBootIniMeansLegacy) {
  // A fresh install, or one whose boot.ini has not been written yet.
  const boost::filesystem::path missing = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-none-%%%%") / "boot.ini";
  EXPECT_EQ(nscp::paths::layout_from_boot_ini_file(missing.string()), nscp::paths::layout::legacy);
}

TEST(PathDefaults, ParsesTheBootIniModes) {
  EXPECT_EQ(nscp::paths::parse_layout("modern"), nscp::paths::layout::modern);
  EXPECT_EQ(nscp::paths::parse_layout("v2"), nscp::paths::layout::modern);
  EXPECT_EQ(nscp::paths::parse_layout("legacy"), nscp::paths::layout::legacy);
  EXPECT_EQ(nscp::paths::parse_layout(""), nscp::paths::layout::legacy);
  // An unrecognised mode must not silently become the new layout.
  EXPECT_EQ(nscp::paths::parse_layout("moderne"), nscp::paths::layout::legacy);
  EXPECT_FALSE(nscp::paths::is_known_layout("moderne"));
  EXPECT_TRUE(nscp::paths::is_known_layout("modern"));
  EXPECT_TRUE(nscp::paths::is_known_layout(""));
}

TEST_F(PathManagerTest, ModernLayoutMovesEverythingWritableOutOfTheInstallDirectory) {
#ifdef WIN32
  const std::string install = pm->expand_path("${exe-path}");
  pm->set_layout(nscp::paths::layout::modern);
  const std::string shared = pm->expand_path("${shared-path}");
  EXPECT_NE(shared, install);
  EXPECT_EQ(shared.find("${"), std::string::npos) << shared;
  // The folders that carry secrets or are rewritten at runtime have to follow.
  for (const char *token : {"${certificate-path}", "${" FLEET_FOLDER_KEY "}", "${log-path}", "${cache-folder}"}) {
    const std::string resolved = pm->expand_path(token);
    EXPECT_EQ(resolved.find(shared), 0u) << token << " -> " << resolved;
  }
  // Switching back is not a one-way door; the tests below share this fixture.
  pm->set_layout(nscp::paths::layout::legacy);
  EXPECT_EQ(pm->expand_path("${shared-path}"), install);
#else
  GTEST_SKIP() << "the layout switch is Windows-only";
#endif
}

TEST_F(PathManagerTest, FleetFolderExpandsAndIsWritableByTheService) {
  // The fleet sync rewrites everything under this folder as the account the
  // service runs as, so it must resolve fully...
  const std::string expanded = pm->expand_path("${" FLEET_FOLDER_KEY "}");
  EXPECT_FALSE(expanded.empty());
  EXPECT_EQ(expanded.find("${"), std::string::npos);
#ifndef WIN32
  // ...and on unix it must not land in the package directory, which is
  // root-owned while the packaged service runs unprivileged. Pinning this stops
  // the default drifting back onto ${shared-path}, which is the bug this whole
  // layout change exists to fix (docs/design/linux-writable-state.md).
  EXPECT_NE(expanded.find(pm->expand_path("${data-path}")), std::string::npos) << "fleet folder escaped the state directory: " << expanded;
  EXPECT_EQ(expanded.find(pm->expand_path("${shared-path}")), std::string::npos) << "fleet folder is inside the package directory: " << expanded;
#endif
}

TEST_F(PathManagerTest, CaPathExpandsToBundleFile) {
  // ca-path must expand to an absolute file path (not leave any ${...}
  // placeholders behind). On Windows it points inside ${certificate-path};
  // on Linux it points under /etc.
  const std::string expanded = pm->expand_path("${ca-path}");
  EXPECT_FALSE(expanded.empty());
  EXPECT_EQ(expanded.find("${"), std::string::npos);
#ifdef WIN32
  EXPECT_NE(expanded.find("windows-ca.pem"), std::string::npos);
#else
  EXPECT_EQ(expanded.rfind("/etc/", 0), 0u);

  // ...and it must name a bundle that is actually there. This used to be one
  // hardcoded path for every non-Windows platform - the Debian one - so on
  // RHEL-family it named a file that does not exist, and every TLS check that
  // did not carry its own ca= failed with "Failed to load CA <path>: No such
  // file or directory". CONFIG_CA_PATH now detects the platform bundle at
  // configure time, and this asserts the detection actually found it: the build
  // host is the distribution the package targets, so a regression to a
  // hardcoded value shows up here rather than in the field.
  EXPECT_TRUE(boost::filesystem::exists(expanded)) << "ca-path does not exist: " << expanded;
#endif
}

#ifdef WIN32
TEST_F(PathManagerTest, GetFolderKeysWindows) {
  const char *keys[] = {"common-appdata", "appdata"};

  for (const auto &key : keys) {
    EXPECT_FALSE(pm->getFolder(key).empty()) << "Failed for key: " << key;
  }
}
#else
// ${etc} tracks NSCP_SYSCONFDIR (ETC_FOLDER) so it follows CMAKE_INSTALL_PREFIX
// rather than being a literal /etc.
TEST_F(PathManagerTest, GetFolderKeysLinux) { EXPECT_EQ(pm->getFolder("etc"), ETC_FOLDER); }

// boot-conf is the CLI-overridable token for boot.ini's location. Its default
// chains off ${etc} and must expand to a clean absolute path (no leftover
// ${...}) ending in nsclient/boot.ini.
TEST_F(PathManagerTest, BootConfDefaultExpandsCleanly) {
  const std::string expanded = pm->expand_path("${boot-conf}");
  EXPECT_EQ(expanded.find("${"), std::string::npos);
  EXPECT_NE(expanded.find("nsclient/boot.ini"), std::string::npos);
  EXPECT_EQ(expanded, std::string(ETC_FOLDER) + "/nsclient/boot.ini");
}

// A CLI --path-override can relocate boot-conf, which is what lets an operator
// run a binary built for one prefix against a config laid out for another.
TEST_F(PathManagerTest, CliOverrideRelocatesBootConf) {
  pm->set_cli_overrides({{"boot-conf", "/opt/custom/boot.ini"}});
  EXPECT_EQ(pm->expand_path("${boot-conf}"), "/opt/custom/boot.ini");
}
#endif

// set_cli_overrides — the highest-precedence layer (CLI --path-override). It
// must win over boot.ini's [paths] (set_overrides) and the compile-time
// defaults, regardless of the order the two layers are installed in.

TEST_F(PathManagerTest, CliOverridesBeatBootIniAndDefaults) {
  pm->set_overrides({{"certificate-path", "/from-boot-ini"}});
  pm->set_cli_overrides({{"certificate-path", "/from-cli"}, {"module-path", "/cli-modules"}});

  EXPECT_EQ(pm->getFolder("certificate-path"), "/from-cli");  // CLI beats boot.ini
  EXPECT_EQ(pm->getFolder("module-path"), "/cli-modules");    // CLI beats default
}

TEST_F(PathManagerTest, CliOverridesWinIrrespectiveOfApplyOrder) {
  // boot.ini's [paths] are applied after the CLI layer in practice (init_settings
  // runs after set_cli_overrides); the CLI layer must still win.
  pm->set_cli_overrides({{"certificate-path", "/from-cli"}});
  pm->set_overrides({{"certificate-path", "/from-boot-ini"}});
  EXPECT_EQ(pm->getFolder("certificate-path"), "/from-cli");
}

// set_overrides — the override-first behaviour Phase 1.5 added. Overrides
// come from boot.ini's [paths] section; once installed, getFolder must
// prefer them over any compile-time default, and expand_path must resolve
// downstream references through them.

TEST_F(PathManagerTest, OverridesTakePrecedenceOverDefaults) {
  pm->set_overrides({{"certificate-path", "/custom/security"}});
  EXPECT_EQ(pm->getFolder("certificate-path"), "/custom/security");
}

TEST_F(PathManagerTest, OverridesAffectExpandPathChain) {
  // Default ca-path on Windows is "${certificate-path}/windows-ca.pem"; on
  // Linux ca-path is a fixed file so we exercise the chain via certificate-path
  // expansion explicitly.
  pm->set_overrides({{"certificate-path", "/custom/security"}});
  const std::string expanded = pm->expand_path("${certificate-path}/cert.pem");
  EXPECT_EQ(expanded, "/custom/security/cert.pem");
}

TEST_F(PathManagerTest, OverridesReplaceRatherThanMerge) {
  pm->set_overrides({{"certificate-path", "/first"}});
  pm->set_overrides({{"log-path", "/second"}});
  // certificate-path should fall back to the compile-time default now that
  // the override map no longer contains it.
  EXPECT_NE(pm->getFolder("certificate-path"), "/first");
  EXPECT_EQ(pm->getFolder("log-path"), "/second");
}

TEST_F(PathManagerTest, OverridesIgnoredForUnknownKeyFallback) {
  // Unknown keys still fall through to getBasePath, even if overrides are set
  // for other keys.
  pm->set_overrides({{"certificate-path", "/x"}});
  EXPECT_FALSE(pm->getFolder("definitely-not-a-known-key").empty());
}

TEST_F(PathManagerTest, OverrideValuesCanBeTemplates) {
  // boot.ini admins may write shared-path = ${common-appdata}/NSClient++ and
  // expect downstream tokens to chain. The recursive expander handles this.
  pm->set_overrides({{"certificate-path", "${base-path}/custom-sec"}});
  const std::string expanded = pm->expand_path("${certificate-path}");
  EXPECT_EQ(expanded.find("${"), std::string::npos);
  EXPECT_NE(expanded.find("custom-sec"), std::string::npos);
}

// add_overrides — the additive merge used by the CLI --path-override flag to layer
// on top of whatever boot.ini already installed via set_overrides.

TEST_F(PathManagerTest, AddOverridesMergesOnTopOfSetOverrides) {
  pm->set_overrides({{"certificate-path", "/from-boot-ini"}, {"log-path", "/boot-logs"}});
  pm->add_overrides({{"log-path", "/cli-logs"}, {"module-path", "/cli-modules"}});

  // log-path overwritten by CLI, certificate-path preserved from boot, module-path added by CLI.
  EXPECT_EQ(pm->getFolder("certificate-path"), "/from-boot-ini");
  EXPECT_EQ(pm->getFolder("log-path"), "/cli-logs");
  EXPECT_EQ(pm->getFolder("module-path"), "/cli-modules");
}

TEST_F(PathManagerTest, AddOverridesAloneWorksWithoutPriorSet) {
  // CLI args may be the only source of overrides (no boot.ini).
  pm->add_overrides({{"log-path", "/cli-only"}});
  EXPECT_EQ(pm->getFolder("log-path"), "/cli-only");
}

TEST_F(PathManagerTest, AddOverridesEmptyIsNoOp) {
  pm->set_overrides({{"certificate-path", "/existing"}});
  pm->add_overrides({});
  EXPECT_EQ(pm->getFolder("certificate-path"), "/existing");
}

// --- migration: the awkward cases -------------------------------------------
// This runs on other people's machines, once, against files they cannot get
// back. The positive path above is the easy half; these are the ones that
// decide whether a bad day is recoverable.

TEST_F(LayoutMigrationTest, AnAlreadyOccupiedLogDirectoryDoesNotStopTheRest) {
  write(from_ / "nsclient.ini", "CONFIG");
  write(from_ / "log" / "nsclient.log", "OLD");
  // Occupy the destination so the log tree cannot move.
  write(to_ / "log" / "nsclient.log", "NEWER");

  const nscp::paths::migration_report report = nscp::paths::apply_migration(from_.string(), to_.string());
  EXPECT_TRUE(report.ok());
  EXPECT_EQ(read(to_ / "log" / "nsclient.log"), "NEWER") << "the destination copy wins";
  // The thing that actually matters still moved.
  EXPECT_EQ(read(to_ / "nsclient.ini"), "CONFIG");
}

// Whether a failed entry sinks the whole migration depends on which entry it
// is: the agent's identity, yes; an old log the running service is holding
// open, no. A real filesystem failure is not something a test can force
// portably, so the rule is checked where it lives.
TEST(LayoutMigrationReport, OnlyAnEssentialFailureSinksTheMigration) {
  nscp::paths::migration_report report;

  nscp::paths::migration_step log;
  log.name = "log/";
  log.action = nscp::paths::migration_action::failed;
  log.essential = false;
  report.steps.push_back(log);
  EXPECT_TRUE(report.ok()) << "old logs are not worth stranding an agent for";

  nscp::paths::migration_step identity;
  identity.name = "security/agent-state.json";
  identity.action = nscp::paths::migration_action::failed;
  identity.essential = true;
  report.steps.push_back(identity);
  EXPECT_FALSE(report.ok()) << "an identity that did not move must fail the migration";
}

TEST(LayoutMigrationReport, BlockedAndDroppedAreSuccessfulOutcomes) {
  // `blocked` means the destination already had it, which is how a repeated run
  // stays safe; `dropped` is a deliberate decision. Neither is a failure.
  nscp::paths::migration_report report;
  for (const nscp::paths::migration_action action :
       {nscp::paths::migration_action::blocked, nscp::paths::migration_action::dropped, nscp::paths::migration_action::kept,
        nscp::paths::migration_action::absent, nscp::paths::migration_action::moved}) {
    nscp::paths::migration_step step;
    step.name = "x";
    step.action = action;
    report.steps.push_back(step);
  }
  EXPECT_TRUE(report.ok());
}

TEST_F(LayoutMigrationTest, AnOccupiedDirectoryIsLeftAloneRatherThanMerged) {
  // Half-finished previous run: the fleet folder is already partly there. The
  // copy at the destination is the live one, so it wins wholesale - merging two
  // trees would risk pairing a new fleet.ini with a stale bundle cache.
  write(from_ / "fleet" / "fleet.ini", "OLD-MANAGED");
  write(from_ / "fleet" / "cache" / "b.zip", "OLD-BUNDLE");
  write(to_ / "fleet" / "fleet.ini", "LIVE-MANAGED");

  const nscp::paths::migration_report report = nscp::paths::apply_migration(from_.string(), to_.string());
  EXPECT_TRUE(report.ok());
  EXPECT_EQ(read(to_ / "fleet" / "fleet.ini"), "LIVE-MANAGED");
  EXPECT_FALSE(exists(to_ / "fleet" / "cache" / "b.zip")) << "the destination tree is kept as-is, not merged into";
  EXPECT_EQ(action_for(report, "fleet/"), nscp::paths::migration_action::blocked);
}

TEST_F(LayoutMigrationTest, NothingToMoveIsASuccessNotAnError) {
  // A fresh install that has never been configured or enrolled. The command
  // should say "nothing to do", not fail.
  const nscp::paths::migration_report report = nscp::paths::apply_migration(from_.string(), to_.string());
  EXPECT_TRUE(report.ok());
  EXPECT_FALSE(report.has(nscp::paths::migration_action::moved));
  EXPECT_FALSE(report.has(nscp::paths::migration_action::failed));
}

TEST_F(LayoutMigrationTest, AnInstallWithOnlyShippedFilesMovesNothing) {
  // Installed but never configured: security/ holds only what the package put
  // there. Moving any of it would take files out of the installer's hands.
  write(from_ / "security" / "nrpe_dh_512.pem", "DH512");
  write(from_ / "security" / "nrpe_dh_2048.pem", "DH2048");

  const nscp::paths::migration_report report = nscp::paths::apply_migration(from_.string(), to_.string());
  EXPECT_TRUE(report.ok());
  EXPECT_FALSE(report.has(nscp::paths::migration_action::moved));
  EXPECT_EQ(read(from_ / "security" / "nrpe_dh_512.pem"), "DH512");
  EXPECT_EQ(read(from_ / "security" / "nrpe_dh_2048.pem"), "DH2048");
}

TEST_F(LayoutMigrationTest, ADroppedFileIsNotResurrectedAtTheDestination) {
  // windows-ca.pem is dropped rather than moved. It must not turn up on the
  // other side either - a stale trust bundle is the thing being avoided.
  write(from_ / "security" / "windows-ca.pem", "STALE-TRUST");

  const nscp::paths::migration_report report = nscp::paths::apply_migration(from_.string(), to_.string());
  ASSERT_TRUE(report.ok());
  EXPECT_FALSE(exists(from_ / "security" / "windows-ca.pem"));
  EXPECT_FALSE(exists(to_ / "security" / "windows-ca.pem"));
}

TEST_F(LayoutMigrationTest, ReportsEveryDecisionItMade) {
  // The CLI prints this and the installer logs it: an operator has to be able to
  // see what happened to each file, not just whether it worked.
  write(from_ / "nsclient.ini", "CONFIG");
  write(from_ / "security" / "nrpe_dh_512.pem", "DH");
  write(from_ / "security" / "windows-ca.pem", "STALE");

  const nscp::paths::migration_report report = nscp::paths::apply_migration(from_.string(), to_.string());
  ASSERT_TRUE(report.ok());
  const std::vector<std::string> lines = report.describe();
  std::string all;
  for (const std::string &line : lines) all += line + "\n";

  EXPECT_NE(all.find("nsclient.ini"), std::string::npos) << all;
  EXPECT_NE(all.find("nrpe_dh_512.pem"), std::string::npos) << all;
  EXPECT_NE(all.find("windows-ca.pem"), std::string::npos) << all;
  // Every kept/dropped line carries a reason, so the operator is not left
  // guessing why a file stayed behind.
  EXPECT_NE(all.find("shipped with the package"), std::string::npos) << all;
  EXPECT_NE(all.find("re-exported"), std::string::npos) << all;
}

TEST_F(LayoutMigrationTest, AnEmptySourceOrDestinationIsRejected) {
  EXPECT_FALSE(nscp::paths::apply_migration("", to_.string()).ok());
  EXPECT_FALSE(nscp::paths::apply_migration(from_.string(), "").ok());
}

// --- which layout an install ends up on -------------------------------------
// The installer decides this once, unattended, against an installation that
// already exists and may already hold the fleet identity.

namespace {
using nscp::paths::layout;
layout resolve(layout current, const std::string &requested) { return nscp::paths::resolve_requested_layout(current, requested); }
}  // namespace

TEST(ResolveRequestedLayout, AskingForModernOptsIn) {
  EXPECT_EQ(resolve(layout::legacy, "modern"), layout::modern);
  EXPECT_EQ(resolve(layout::legacy, "v2"), layout::modern);
  EXPECT_EQ(resolve(layout::modern, "modern"), layout::modern) << "asking again is a no-op, not a re-migration decision";
}

TEST(ResolveRequestedLayout, AskingForNothingKeepsWhatTheHostHas) {
  // An upgrade that does not repeat the property must not move anything, in
  // either direction. This is the ordinary case: almost nobody passes LAYOUT.
  EXPECT_EQ(resolve(layout::legacy, ""), layout::legacy);
  EXPECT_EQ(resolve(layout::modern, ""), layout::modern);
}

TEST(ResolveRequestedLayout, AnUnrecognisedValueIsNotAGuess) {
  // Typos included. Guessing "modren" meant "modern" would move an
  // installation's files on the strength of a spelling correction.
  for (const char *junk : {"moderne", "MODERN ", "3", "true", "yes", "v3", "legacy-ish"}) {
    EXPECT_EQ(resolve(layout::legacy, junk), layout::legacy) << junk;
    EXPECT_EQ(resolve(layout::modern, junk), layout::modern) << junk;
  }
}

TEST(ResolveRequestedLayout, AModernInstallIsNeverSentBackToLegacy) {
  // There is no migration in that direction, so honouring this would leave the
  // agent reading an install folder whose files are in %ProgramData%.
  EXPECT_EQ(resolve(layout::modern, "legacy"), layout::modern);
}

TEST(ResolveRequestedLayout, ALegacyInstallAskedForLegacyStaysLegacy) {
  EXPECT_EQ(resolve(layout::legacy, "legacy"), layout::legacy);
}
