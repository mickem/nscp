// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Path handling, wired up the way the service wires it.
//
// path_manager_test covers the expander on its own and helper_test covers the
// settings registry against a stub whose expand_path is a string prefix. Neither
// exercises the join between them, which is where the behaviour an operator
// actually sees is decided:
//
//   settings_registry::notify()
//     -> typed_key::notify()
//       -> lookup_path_processor::process()
//            if (default_root_.empty()) return core_->expand_path(value);
//            return core_->resolve_path(value, default_root_);
//         -> settings_impl_interface::expand_path / resolve_path
//           -> path_manager: overrides, recursion, sentinel, rooting
//
// A bug anywhere along that chain reaches the field as "my file went to the
// wrong folder", and every piece of it can be individually correct while the
// composition is not - a key registered with string_key instead of path_key is
// exactly that, and is what put `${scripts}/x.bat` on disk verbatim.
//
// So these tests drive the real registry, the real key types and a real
// path_manager, and assert on the value that lands in the variable a module
// would read.

#include "path_manager.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <map>
#include <memory>
#include <nscapi/settings/helper.hpp>
#include <nsclient/logger/logger.hpp>
#include <string>
#include <vector>

namespace sh = nscapi::settings_helper;

namespace {

// Boost's operator/ appends the *preferred* separator, so a join that reads
// "/var/log/nscp/x" on POSIX reads "/var/log/nscp\\x" on Windows. These tests
// are about which directory a value lands in, not how the separator is spelt,
// so compare the generic (forward-slash) form. Values produced by token
// substitution alone keep whatever the operator typed and need no such care.
static std::string generic(const std::string &path) { return boost::filesystem::path(path).generic_string(); }

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

// A settings back end that holds values in a map and resolves paths through a
// real path_manager - i.e. the two halves the service puts together, with
// nothing stubbed in between.
class settings_backend : public sh::settings_impl_interface {
 public:
  explicit settings_backend(const nsclient::core::path_instance &paths) : paths_(paths) {}

  void set(const std::string &path, const std::string &key, const std::string &value) { values_[path][key] = value; }

  // Errors reported per key by settings_registry::notify(). The whole point of
  // raising an unknown token is that it arrives here naming the key, so the
  // tests assert on it rather than on the absence of a crash.
  const std::vector<std::string> &errors() const { return errors_; }

  std::string expand_path(std::string key) override { return paths_->expand_path(std::move(key)); }

  std::string get_string(std::string path, std::string key, std::string def) override {
    const auto p = values_.find(path);
    if (p == values_.end()) return def;
    const auto k = p->second.find(key);
    return k == p->second.end() ? def : k->second;
  }
  void set_string(std::string path, std::string key, std::string value) override { values_[path][key] = value; }

  void register_path(std::string, std::string, std::string, bool, bool) override {}
  void register_key(std::string, std::string, std::string, std::string, std::string, std::string, bool, bool, bool) override {}
  void register_subkey(std::string, std::string, std::string, bool, bool) override {}
  void register_tpl(std::string, std::string, std::string, std::string, std::string) override {}
  string_list get_sections(std::string) override { return {}; }
  string_list get_keys(std::string) override { return {}; }
  void remove_key(std::string, std::string) override {}
  void remove_path(std::string) override {}

  void err(const char *, int, std::string message) override { errors_.push_back(message); }
  void warn(const char *, int, std::string) override {}
  void info(const char *, int, std::string) override {}
  void debug(const char *, int, std::string) override {}

 private:
  nsclient::core::path_instance paths_;
  std::map<std::string, std::map<std::string, std::string>> values_;
  std::vector<std::string> errors_;
};

class PathSettingsIntegrationTest : public ::testing::Test {
 protected:
  nsclient::core::path_instance paths_;
  std::shared_ptr<settings_backend> backend_;

  void SetUp() override {
    paths_ = std::make_shared<nsclient::core::path_manager>(nsclient::logging::log_client_accessor(std::make_shared<silent_logger>()));
    // Absolute, predictable roots so the assertions below say what they mean on
    // every platform rather than depending on where the binary happens to live.
    paths_->set_overrides({{"shared-path", "/srv/nscp"}, {"log-path", "/var/log/nscp"}, {"scripts", "${shared-path}/scripts"}});
    backend_ = std::make_shared<settings_backend>(paths_);
  }

  // Register one key through the real registry and run the notify pass, which
  // is what invokes lookup_path_processor.
  void notify_one(const std::string &key_name, sh::key_type key) {
    sh::settings_registry settings(backend_);
    settings.path("/settings/test").add_key().add_string(key_name, std::move(key), "TITLE", "DESCRIPTION", false);
    settings.notify();
  }
};

}  // namespace

// --- expand only (path_key without a root) -----------------------------------

TEST_F(PathSettingsIntegrationTest, PathKeyExpandsATokenThroughTheWholeChain) {
  std::string value;
  backend_->set("/settings/test", "file", "${scripts}/check.bat");
  notify_one("file", sh::path_key(&value));
  EXPECT_EQ(value, "/srv/nscp/scripts/check.bat");
}

TEST_F(PathSettingsIntegrationTest, PathKeyResolvesATokenThatChainsThroughAnother) {
  // ${scripts} is itself defined as ${shared-path}/scripts, so this only works
  // if the expander recurses - the property the docs describe as "moving one
  // folder moves everything under it".
  std::string value;
  paths_->set_overrides({{"shared-path", "/opt/moved"}, {"scripts", "${shared-path}/scripts"}});
  backend_->set("/settings/test", "file", "${scripts}/check.bat");
  notify_one("file", sh::path_key(&value));
  EXPECT_EQ(value, "/opt/moved/scripts/check.bat");
}

TEST_F(PathSettingsIntegrationTest, StringKeyLeavesATokenUnexpanded) {
  // The distinction that caused this whole class of bug: a path-shaped setting
  // registered with string_key never reaches the expander, so `${scripts}/x.bat`
  // is stored, and later opened, verbatim. Pinned so that registering a path
  // with the wrong key type stays a visible difference rather than a silent one.
  std::string value;
  backend_->set("/settings/test", "file", "${scripts}/check.bat");
  notify_one("file", sh::string_key(&value));
  EXPECT_EQ(value, "${scripts}/check.bat");
}

TEST_F(PathSettingsIntegrationTest, PathKeyWithoutARootLeavesARelativeValueRelative) {
  // No root declared means the operator is naming a location themselves, and a
  // relative one stays relative - expand_path substitutes tokens, it does not
  // make anything absolute.
  std::string value;
  backend_->set("/settings/test", "file", "some/where.txt");
  notify_one("file", sh::path_key(&value));
  EXPECT_EQ(value, "some/where.txt");
}

// --- expand and root (path_key with a root) ----------------------------------

TEST_F(PathSettingsIntegrationTest, PathKeyWithARootPutsABareNameInThatFolder) {
  std::string value;
  backend_->set("/settings/test", "file", "nsclient.log");
  notify_one("file", sh::path_key(&value, "", "${log-path}"));
  EXPECT_EQ(generic(value), "/var/log/nscp/nsclient.log");
}

TEST_F(PathSettingsIntegrationTest, PathKeyWithARootPutsARelativeSubdirectoryInThatFolder) {
  std::string value;
  backend_->set("/settings/test", "file", "scripts/check_lsi_raid.pl");
  notify_one("file", sh::path_key(&value, "", "${shared-path}"));
  EXPECT_EQ(generic(value), "/srv/nscp/scripts/check_lsi_raid.pl");
}

TEST_F(PathSettingsIntegrationTest, PathKeyWithARootLeavesAnAbsoluteValueAlone) {
  std::string value;
  backend_->set("/settings/test", "file", "/var/log/elsewhere.log");
  notify_one("file", sh::path_key(&value, "", "${log-path}"));
  EXPECT_EQ(value, "/var/log/elsewhere.log");
}

TEST_F(PathSettingsIntegrationTest, PathKeyWithARootStillExpandsATokenAndDoesNotRootIt) {
  std::string value;
  backend_->set("/settings/test", "file", "${scripts}/check.bat");
  notify_one("file", sh::path_key(&value, "", "${log-path}"));
  EXPECT_EQ(value, "/srv/nscp/scripts/check.bat") << "an already-rooted value was joined onto the default root";
}

TEST_F(PathSettingsIntegrationTest, PathKeyWithARootKeepsTheNoPathSentinel) {
  // `none` has to survive the whole chain, or file logging cannot be switched
  // off and every `ca = none` becomes a file called none inside the root.
  std::string value;
  backend_->set("/settings/test", "file", "none");
  notify_one("file", sh::path_key(&value, "", "${log-path}"));
  EXPECT_EQ(value, "none");
}

TEST_F(PathSettingsIntegrationTest, PathKeyWithARootAppliesToTheDefaultValueToo) {
  // Nothing configured, so the key's own default is what gets processed. This is
  // SimpleFileWriter's case: its default was a bare "output.txt", which without
  // rooting is written to whatever the working directory happens to be.
  std::string value;
  notify_one("file", sh::path_key(&value, "output.txt", "${log-path}"));
  EXPECT_EQ(generic(value), "/var/log/nscp/output.txt");
}

TEST_F(PathSettingsIntegrationTest, PathKeyWithARootKeepsAnUnsetValueUnset) {
  // "" means "not configured" on a good number of path options and the consumers
  // test .empty(); rooting it would turn "no certificate key" into a real path.
  std::string value = "sentinel";
  notify_one("file", sh::path_key(&value, "", "${log-path}"));
  EXPECT_EQ(value, "");
}

// --- overrides, through the chain --------------------------------------------

TEST_F(PathSettingsIntegrationTest, ABootIniOverrideMovesWhereABareNameLands) {
  std::string value;
  paths_->set_overrides({{"log-path", "/elsewhere/logs"}});
  backend_->set("/settings/test", "file", "nsclient.log");
  notify_one("file", sh::path_key(&value, "", "${log-path}"));
  EXPECT_EQ(generic(value), "/elsewhere/logs/nsclient.log");
}

TEST_F(PathSettingsIntegrationTest, ACliOverrideBeatsBootIniAllTheWayToTheKey) {
  std::string value;
  paths_->set_overrides({{"log-path", "/from/boot-ini"}});
  paths_->set_cli_overrides({{"log-path", "/from/cli"}});
  backend_->set("/settings/test", "file", "nsclient.log");
  notify_one("file", sh::path_key(&value, "", "${log-path}"));
  EXPECT_EQ(generic(value), "/from/cli/nsclient.log");
}

TEST_F(PathSettingsIntegrationTest, ARelativeOverrideIsDroppedSoTheKeyStillGetsAnAbsolutePath) {
  // The override is refused by path_manager, the compiled default applies, and
  // the key still ends up with something absolute rather than inheriting the
  // bad root.
  std::string value;
  paths_->set_overrides({{"log-path", "relative/logs"}});
  backend_->set("/settings/test", "file", "nsclient.log");
  notify_one("file", sh::path_key(&value, "", "${log-path}"));
  EXPECT_TRUE(boost::filesystem::path(value).is_absolute()) << "a relative override reached the key: " << value;
  EXPECT_EQ(boost::filesystem::path(value).filename().string(), "nsclient.log");
}

// --- unknown tokens ----------------------------------------------------------

TEST_F(PathSettingsIntegrationTest, AnUnknownTokenIsReportedAgainstTheKeyAndTheValueIsNotApplied) {
  // The claim the whole error path rests on: notify() catches per key, names it,
  // and the variable keeps whatever it had - so a typo cannot quietly install a
  // wrong path. Assert both halves.
  std::string value = "untouched";
  backend_->set("/settings/test", "file", "${scripst}/check.bat");
  notify_one("file", sh::path_key(&value));

  EXPECT_EQ(value, "untouched") << "a path built from an unknown token was applied anyway";
  ASSERT_EQ(backend_->errors().size(), 1u) << "the failure was not reported";
  EXPECT_NE(backend_->errors()[0].find("file"), std::string::npos) << "the report did not name the key: " << backend_->errors()[0];
  EXPECT_NE(backend_->errors()[0].find("scripst"), std::string::npos) << "the report did not name the token: " << backend_->errors()[0];
}

TEST_F(PathSettingsIntegrationTest, AnUnknownTokenInOneKeyDoesNotStopTheNext) {
  // Per-key, not per-pass: one mistyped path must not cost a module the rest of
  // its configuration.
  std::string bad, good;
  backend_->set("/settings/test", "bad", "${nope}/x.bat");
  backend_->set("/settings/test", "good", "${scripts}/y.bat");

  sh::settings_registry settings(backend_);
  settings.path("/settings/test")
      .add_key()
      .add_string("bad", sh::path_key(&bad), "TITLE", "DESCRIPTION", false)
      .add_string("good", sh::path_key(&good), "TITLE", "DESCRIPTION", false);
  settings.notify();

  EXPECT_EQ(bad, "");
  EXPECT_EQ(good, "/srv/nscp/scripts/y.bat") << "a later key was skipped because an earlier one failed";
  EXPECT_EQ(backend_->errors().size(), 1u);
}

TEST_F(PathSettingsIntegrationTest, AnUnknownTokenDefinedInPathsBecomesValid) {
  // The token set is open: the error means "no such path is configured", not
  // "not on a fixed list". An operator's own [paths] entry has to work.
  std::string value;
  paths_->add_overrides({{"my-own-folder", "/srv/mine"}});
  backend_->set("/settings/test", "file", "${my-own-folder}/check.bat");
  notify_one("file", sh::path_key(&value));

  EXPECT_EQ(value, "/srv/mine/check.bat");
  EXPECT_TRUE(backend_->errors().empty()) << "a defined token was reported as unknown: " << backend_->errors()[0];
}
