// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "path_manager.hpp"

#include <config.h>
#include <gtest/gtest.h>

#include <memory>
#include <nsclient/logger/logger.hpp>

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
                        "base-path",        "temp",        "shared-path", "exe-path", "data-path", "ca-path"};

  for (const auto &key : keys) {
    EXPECT_FALSE(pm->getFolder(key).empty()) << "Failed for key: " << key;
  }
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
