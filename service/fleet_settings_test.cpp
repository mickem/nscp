// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "fleet_settings.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem/path.hpp>
#include <map>
#include <memory>
#include <nsclient/logger/logger.hpp>
#include <set>
#include <string>
#include <vector>

namespace fs = nsclient::core::fleet_settings;

namespace {

// Compare directories, not separators: see path_settings_integration_test.
std::string generic(const std::string &path) { return boost::filesystem::path(path).generic_string(); }

class silent_logger : public nsclient::logging::log_interface {
 public:
  void trace(const std::string &, const char *, int, const std::string &) override {}
  void debug(const std::string &, const char *, int, const std::string &) override {}
  void info(const std::string &, const char *, int, const std::string &) override {}
  void warning(const std::string &, const char *, int, const std::string &) override {}
  void error(const std::string &, const char *, int, const std::string &) override {}
  void critical(const std::string &, const char *, int, const std::string &) override {}
  bool should_trace() const override { return false; }
  bool should_debug() const override { return false; }
  bool should_info() const override { return false; }
  bool should_warning() const override { return false; }
  bool should_error() const override { return false; }
  bool should_critical() const override { return false; }
};

class FleetSettingsTest : public ::testing::Test {
 protected:
  nsclient::core::path_instance paths_;
  std::map<std::string, std::string> values_;
  // What was declared, as the settings registry would see it, and how often
  // each key was declared by the last read().
  std::map<std::string, std::string> types_;
  std::map<std::string, int> declared_;
  std::map<std::string, std::string> defaults_;
  std::vector<std::string> errors_;

  void SetUp() override {
    paths_ = std::make_shared<nsclient::core::path_manager>(nsclient::logging::log_client_accessor(std::make_shared<silent_logger>()));
    paths_->set_overrides({{"shared-path", "/srv/nscp"}, {FLEET_FOLDER_KEY, "/srv/nscp/fleet"}});
  }

  fleet_config read() {
    errors_.clear();
    declared_.clear();
    return fs::read(
        [this](const char *key, const char *type, const char *title, const char *description, const char *fallback) {
          EXPECT_STRNE(title, "") << key;
          EXPECT_STRNE(description, "") << key;
          types_[key] = type;
          ++declared_[key];
          defaults_[key] = fallback;
          const auto found = values_.find(key);
          return found == values_.end() ? std::string(fallback) : found->second;
        },
        *paths_, errors_);
  }
};

}  // namespace

TEST_F(FleetSettingsTest, AnEmptySectionIsTheDefaults) {
  const fleet_config config = read();
  const fleet_config defaults;
  EXPECT_TRUE(errors_.empty());
  EXPECT_EQ(config.tls_version, "tlsv1.2+");
  EXPECT_EQ(config.timeout_seconds, 60u);
  EXPECT_TRUE(config.failure_context);
  EXPECT_EQ(config.failure_context_interval, 3600u);
  EXPECT_EQ(config.poll_interval, 60u);
  EXPECT_EQ(config.min_poll_interval, 1u);
  EXPECT_EQ(config.max_poll_interval, 86400u);
  // The defaults an operator reads in the settings are the ones the struct
  // carries when a value cannot be used; two sources that must not drift.
  EXPECT_EQ(config.timeout_seconds, defaults.timeout_seconds);
  EXPECT_EQ(config.failure_context, defaults.failure_context);
  EXPECT_EQ(config.failure_context_interval, defaults.failure_context_interval);
  EXPECT_EQ(config.poll_interval, defaults.poll_interval);
  EXPECT_EQ(config.min_poll_interval, defaults.min_poll_interval);
  EXPECT_EQ(config.max_poll_interval, defaults.max_poll_interval);
  // "auto" is this machine's name, whatever that is.
  EXPECT_FALSE(config.hostname.empty());
  EXPECT_NE(config.hostname, "auto");
}

TEST_F(FleetSettingsTest, EveryKeyIsDeclaredOnce) {
  read();
  std::set<std::string> keys;
  for (const auto &entry : types_) keys.insert(entry.first);
  EXPECT_EQ(keys, std::set<std::string>({"state file", "managed path", "hostname", "tls version", "timeout", "failure context", "failure context interval",
                                         "poll interval", "min poll interval", "max poll interval"}));
  for (const auto &entry : types_) EXPECT_EQ(entry.second, entry.first == "failure context" ? "bool" : "string") << entry.first;
  for (const auto &entry : declared_) EXPECT_EQ(entry.second, 1) << entry.first;
}

TEST_F(FleetSettingsTest, TheTreeTheSyncWritesIsRootedAtTheFleetFolder) {
  // Default: the fleet folder itself.
  EXPECT_EQ(generic(read().managed_path), "/srv/nscp/fleet");
  // A relative name would otherwise land wherever the service was started.
  values_["managed path"] = "managed";
  EXPECT_EQ(generic(read().managed_path), "/srv/nscp/fleet/managed");
  // A token is expanded and, being absolute, used as given.
  values_["managed path"] = "${shared-path}/elsewhere";
  EXPECT_EQ(generic(read().managed_path), "/srv/nscp/elsewhere");
  EXPECT_TRUE(errors_.empty());
}

TEST_F(FleetSettingsTest, TheManifestLocationIsExpanded) {
  const std::string by_default = generic(read().state_file);
  EXPECT_EQ(by_default.find("${"), std::string::npos) << by_default;
  values_["state file"] = "${shared-path}/agent-state.json";
  EXPECT_EQ(generic(read().state_file), "/srv/nscp/agent-state.json");
}

TEST_F(FleetSettingsTest, AnExplicitHostnameIsUsedAsGiven) {
  values_["hostname"] = "web-01";
  EXPECT_EQ(read().hostname, "web-01");
}

TEST_F(FleetSettingsTest, DurationsTakeAUnit) {
  values_["timeout"] = "2m";
  values_["failure context interval"] = "30s";
  values_["poll interval"] = "5m";
  values_["min poll interval"] = "10";
  values_["max poll interval"] = "2h";
  const fleet_config config = read();
  EXPECT_TRUE(errors_.empty());
  EXPECT_EQ(config.timeout_seconds, 120u);
  EXPECT_EQ(config.failure_context_interval, 30u);
  EXPECT_EQ(config.poll_interval, 300u);
  EXPECT_EQ(config.min_poll_interval, 10u);
  EXPECT_EQ(config.max_poll_interval, 7200u);
}

TEST_F(FleetSettingsTest, FailureContextCanBeTurnedOff) {
  values_["failure context"] = "false";
  EXPECT_FALSE(read().failure_context);
  values_["failure context"] = "true";
  EXPECT_TRUE(read().failure_context);
}

TEST_F(FleetSettingsTest, ADurationThatCannotBeReadFallsBackToTheDefaultAndNamesTheKey) {
  for (const std::string key : {"timeout", "failure context interval", "poll interval", "min poll interval", "max poll interval"}) {
    values_.clear();
    values_[key] = "soon";
    const fleet_config config = read();
    const fleet_config defaults;
    ASSERT_EQ(errors_.size(), 1u) << key;
    EXPECT_NE(errors_.front().find("Invalid fleet '" + key + "' value 'soon', falling back to " + defaults_[key]), std::string::npos) << errors_.front();
    EXPECT_EQ(config.timeout_seconds, defaults.timeout_seconds) << key;
    EXPECT_EQ(config.failure_context_interval, defaults.failure_context_interval) << key;
    EXPECT_EQ(config.poll_interval, defaults.poll_interval) << key;
    EXPECT_EQ(config.min_poll_interval, defaults.min_poll_interval) << key;
    EXPECT_EQ(config.max_poll_interval, defaults.max_poll_interval) << key;
  }
}

TEST_F(FleetSettingsTest, APollIntervalOfZeroIsRefused) {
  // Zero would be a host polling the fleet server in a tight loop.
  for (const std::string key : {"poll interval", "min poll interval", "max poll interval"}) {
    values_.clear();
    values_[key] = "0s";
    const fleet_config config = read();
    const fleet_config defaults;
    ASSERT_EQ(errors_.size(), 1u) << key;
    EXPECT_NE(errors_.front().find("must be at least 1s"), std::string::npos) << errors_.front();
    EXPECT_EQ(config.poll_interval, defaults.poll_interval) << key;
    EXPECT_EQ(config.min_poll_interval, defaults.min_poll_interval) << key;
    EXPECT_EQ(config.max_poll_interval, defaults.max_poll_interval) << key;
  }
}

TEST_F(FleetSettingsTest, ATimeoutOfZeroIsRefused) {
  // Zero would be a deadline every request to the fleet server misses.
  values_["timeout"] = "0s";
  const fleet_config config = read();
  ASSERT_EQ(errors_.size(), 1u);
  EXPECT_NE(errors_.front().find("Invalid fleet 'timeout' value '0s', falling back to 60s: must be at least 1s"), std::string::npos) << errors_.front();
  EXPECT_EQ(config.timeout_seconds, fleet_config().timeout_seconds);
  values_["timeout"] = "1s";
  EXPECT_EQ(read().timeout_seconds, 1u);
  EXPECT_TRUE(errors_.empty());
}

TEST_F(FleetSettingsTest, AFailureContextIntervalOfZeroIsLeftToThePacersFloor) {
  // failure_context_pacer::configure() raises anything under its 5s floor, so
  // the reader passes a zero through rather than refusing it twice.
  values_["failure context interval"] = "0s";
  EXPECT_EQ(read().failure_context_interval, 0u);
  EXPECT_TRUE(errors_.empty());
}

TEST_F(FleetSettingsTest, OneBadValueDoesNotStopTheRestFromApplying) {
  values_["timeout"] = "soon";
  values_["poll interval"] = "5m";
  values_["tls version"] = "tlsv1.3";
  const fleet_config config = read();
  EXPECT_EQ(errors_.size(), 1u);
  EXPECT_EQ(config.poll_interval, 300u);
  EXPECT_EQ(config.tls_version, "tlsv1.3");
}

TEST_F(FleetSettingsTest, WhatIsNotASettingIsLeftForTheCaller) {
  const fleet_config config = read();
  EXPECT_TRUE(config.nscp_version.empty());
  EXPECT_FALSE(config.allow_plaintext);
  EXPECT_FALSE(config.local_config_probe);
}
