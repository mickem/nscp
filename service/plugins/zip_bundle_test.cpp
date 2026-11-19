// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// A ".zip module" loaded the way the agent loads one: through a real
// plugin_manager pointed at a module folder holding the archive. Loading it
// unpacks the scripts it carries into ${scripts}/tmp/<bundle>, asks the
// modules it names to load, and runs its on_start commands; unloading it takes
// the unpacked scripts away again. zip_plugin_test.cpp covers the metadata on
// its own; this is the half that touches the disk and the plugin manager.
//
// No real module is available here, so asking for CheckExternalScripts (or any
// other provider) fails. That failure is part of what is checked: a bundle
// whose provider is missing must still load, and say why its scripts are not
// registered.

#include <gtest/gtest.h>

#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <memory>
#include <nsclient/logger/logger.hpp>
#include <settings/test_helpers.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "../../libs/settings_manager/settings_manager_impl.h"
#include "../path_manager.hpp"
#include "plugin_manager.hpp"
#include "zip_test_writer.hpp"

namespace fs = boost::filesystem;

namespace {

class bundle_logger : public nsclient::logging::logger {
 public:
  std::vector<std::string> errors;

  void trace(const std::string &, const char *, int, const std::string &) override {}
  void debug(const std::string &, const char *, int, const std::string &) override {}
  void info(const std::string &, const char *, int, const std::string &) override {}
  void warning(const std::string &, const char *, int, const std::string &) override {}
  void error(const std::string &, const char *, int, const std::string &message) override { errors.push_back(message); }
  void critical(const std::string &, const char *, int, const std::string &message) override { errors.push_back(message); }
  bool should_trace() const override { return false; }
  bool should_debug() const override { return false; }
  bool should_info() const override { return false; }
  bool should_warning() const override { return false; }
  bool should_error() const override { return true; }
  bool should_critical() const override { return true; }
  void raw(const std::string &) override {}
  void add_subscriber(nsclient::logging::logging_subscriber_instance) override {}
  nsclient::logging::unsubscribe_result close_subscriber(nsclient::logging::logging_subscriber_instance) override { return {}; }
  void reopen_subscriber(nsclient::logging::logging_subscriber_instance) override {}
  bool drop_subscriber(nsclient::logging::logging_subscriber_instance) override { return false; }
  nsclient::logging::unsubscribe_result remove_subscriber(nsclient::logging::logging_subscriber_instance) override { return {}; }
  std::vector<nsclient::logging::logging_subscriber_instance> clear_subscribers() override { return {}; }
  bool startup() override { return false; }
  bool shutdown() override { return false; }
  void destroy() override {}
  void configure() override {}
  void set_log_level(std::string) override {}
  std::string get_log_level() const override { return ""; }
  void set_backend(std::string) override {}

  bool logged(const std::string &needle) const {
    return std::any_of(errors.begin(), errors.end(), [&](const std::string &e) { return e.find(needle) != std::string::npos; });
  }
};

std::string read_file(const fs::path &p) {
  std::ifstream in(p.string().c_str(), std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Loading a module reads the [/modules] section, so the settings store has to
// exist: an INI file in the test's own folder stands in for the agent's.
class bundle_settings_provider : public settings_manager::provider_interface {
 public:
  std::string expand_path(std::string file) override { return file; }
  nsclient::logging::logger_instance get_logger() const override { return logger_; }
  void apply_path_overrides(std::map<std::string, std::string>) override {}

 private:
  nsclient::logging::logger_instance logger_ = settings_test::make_null_logger();
};

class ZipBundleTest : public ::testing::Test {
 protected:
  fs::path dir_;
  fs::path modules_;
  fs::path scripts_;
  std::shared_ptr<bundle_logger> logger_;
  nsclient::core::path_instance paths_;
  nsclient::core::plugin_mgr_instance plugins_;
  std::unique_ptr<bundle_settings_provider> provider_;

  void SetUp() override {
    dir_ = fs::temp_directory_path() / fs::unique_path("nscp-zipbundle-%%%%-%%%%-%%%%");
    modules_ = dir_ / "modules";
    scripts_ = dir_ / "scripts";
    fs::create_directories(modules_);
    fs::create_directories(scripts_);
    const fs::path ini = dir_ / "settings.ini";
    settings_test::write_file(ini, "[/modules]\n");
    provider_ = std::make_unique<bundle_settings_provider>();
    std::string p = ini.generic_string();
    if (!p.empty() && p.front() == '/') p.erase(0, 1);
    ASSERT_TRUE(settings_manager::init_settings(provider_.get(), "ini:///" + p));
    logger_ = std::make_shared<bundle_logger>();
    paths_ = std::make_shared<nsclient::core::path_manager>(logger_);
    paths_->set_overrides({{"scripts", scripts_.string()}});
    plugins_ = std::make_shared<nsclient::core::plugin_manager>(paths_, logger_);
    plugins_->set_path(modules_);
  }

  void TearDown() override {
    plugins_.reset();
    settings_manager::destroy_settings();
    boost::system::error_code ec;
    fs::remove_all(dir_, ec);
  }

  void make_bundle(const std::string &name, const std::vector<zip_test::zip_entry> &entries) { zip_test::write_zip(modules_ / (name + ".zip"), entries); }

  fs::path unpacked(const std::string &bundle) const { return scripts_ / "tmp" / bundle; }
};

}  // namespace

TEST_F(ZipBundleTest, loading_a_bundle_unpacks_its_scripts) {
  make_bundle("bundle", {{"module.json", R"({"name":"Bundle","description":"d","scripts":["scripts/check_foo.sh","lib/check_bar.py"]})"},
                         {"scripts/check_foo.sh", "#!/bin/sh\necho foo\n"},
                         {"lib/check_bar.py", "print('bar')\n"}});

  ASSERT_TRUE(plugins_->load_single_plugin("bundle", "", true));

  // Unpacked flat, by file name, into the bundle's own folder.
  EXPECT_EQ("#!/bin/sh\necho foo\n", read_file(unpacked("bundle") / "check_foo.sh"));
  EXPECT_EQ("print('bar')\n", read_file(unpacked("bundle") / "check_bar.py"));
}

TEST_F(ZipBundleTest, a_loaded_bundle_is_listed_under_its_file_name) {
  make_bundle("bundle", {{"module.json", R"({"name":"Bundle","description":"the description"})"}});
  ASSERT_TRUE(plugins_->load_single_plugin("bundle", "", true));

  const auto loaded = plugins_->get_loaded_modules();
  ASSERT_EQ(1u, loaded.size());
  EXPECT_EQ("bundle", loaded.front());
  EXPECT_TRUE(plugins_->get_plugin_cache()->has_module("bundle"));
}

TEST_F(ZipBundleTest, a_bundle_whose_provider_is_missing_still_loads_and_says_so) {
  // Nothing here can provide CheckExternalScripts: the bundle asks for it
  // (it is implied by a non-python script), fails to get it, and the script
  // registration that follows fails too - each logged, none fatal.
  make_bundle("bundle", {{"module.json", R"({"name":"Bundle","description":"","scripts":["check_foo.sh"]})"}, {"check_foo.sh", "echo"}});

  ASSERT_TRUE(plugins_->load_single_plugin("bundle", "", true));
  EXPECT_TRUE(fs::exists(unpacked("bundle") / "check_foo.sh"));
  EXPECT_TRUE(logger_->logged("CheckExternalScripts"));
}

TEST_F(ZipBundleTest, a_script_missing_from_the_archive_is_skipped_and_logged) {
  make_bundle("bundle", {{"module.json", R"({"name":"Bundle","description":"","scripts":["missing.sh","present.sh"]})"}, {"present.sh", "echo"}});

  ASSERT_TRUE(plugins_->load_single_plugin("bundle", "", true));
  EXPECT_FALSE(fs::exists(unpacked("bundle") / "missing.sh"));
  EXPECT_TRUE(fs::exists(unpacked("bundle") / "present.sh"));
  EXPECT_TRUE(logger_->logged("Failed to add script missing.sh"));
}

TEST_F(ZipBundleTest, an_empty_on_start_command_is_logged_and_skipped) {
  make_bundle("bundle", {{"module.json", R"({"name":"Bundle","description":"","on_start":["   "]})"}});

  ASSERT_TRUE(plugins_->load_single_plugin("bundle", "", true));
  EXPECT_TRUE(logger_->logged("Empty on_start command in bundle"));
}

TEST_F(ZipBundleTest, an_unparsable_on_start_command_is_logged_and_skipped) {
  // `\q` is not an escape the command-line tokenizer knows.
  make_bundle("bundle", {{"module.json", R"({"name":"Bundle","description":"","on_start":["cmd \\q"]})"}});

  ASSERT_TRUE(plugins_->load_single_plugin("bundle", "", true));
  EXPECT_TRUE(logger_->logged("Failed to parse \"cmd \\q\""));
}

TEST_F(ZipBundleTest, an_on_start_command_for_a_missing_module_is_reported) {
  make_bundle("bundle", {{"module.json", R"({"name":"Bundle","description":"","on_start":["NoSuchModule.run arg"]})"}});

  ASSERT_TRUE(plugins_->load_single_plugin("bundle", "", true));
  EXPECT_TRUE(logger_->logged("NoSuchModule"));
}

TEST_F(ZipBundleTest, unloading_a_bundle_removes_what_it_unpacked) {
  make_bundle("bundle", {{"module.json", R"({"name":"Bundle","description":"","scripts":["a.sh","b.sh"]})"}, {"a.sh", "a"}, {"b.sh", "b"}});
  ASSERT_TRUE(plugins_->load_single_plugin("bundle", "", true));
  ASSERT_TRUE(fs::exists(unpacked("bundle") / "a.sh"));

  ASSERT_TRUE(plugins_->remove_plugin("bundle"));
  EXPECT_FALSE(fs::exists(unpacked("bundle")));
  EXPECT_FALSE(fs::exists(scripts_ / "tmp"));
  EXPECT_TRUE(plugins_->get_loaded_modules().empty());
}

TEST_F(ZipBundleTest, a_broken_bundle_does_not_load) {
  make_bundle("broken", {{"readme.txt", "no module.json here"}});

  EXPECT_FALSE(plugins_->load_single_plugin("broken", "", true));
  EXPECT_TRUE(plugins_->get_loaded_modules().empty());
  EXPECT_TRUE(logger_->logged("module.json"));
}

TEST_F(ZipBundleTest, a_bundle_refuses_the_calls_a_compiled_module_would_serve) {
  make_bundle("bundle", {{"module.json", R"({"name":"Bundle","description":""})"}});
  bool loaded = false;
  const auto plugin = plugins_->only_load_module("bundle", "", loaded);
  ASSERT_TRUE(loaded);
  ASSERT_TRUE(plugin);

  std::string reply;
  std::string request;
  EXPECT_THROW(plugin->handleCommand("", reply), nsclient::core::plugin_exception);
  EXPECT_THROW(plugin->handle_schedule(""), nsclient::core::plugin_exception);
  EXPECT_THROW(plugin->handleNotification("chan", request, reply), nsclient::core::plugin_exception);
  EXPECT_THROW(plugin->on_event(""), nsclient::core::plugin_exception);
  EXPECT_THROW(plugin->fetchMetrics(reply), nsclient::core::plugin_exception);
  EXPECT_THROW(plugin->fetchFacts("", reply), nsclient::core::plugin_exception);
  EXPECT_THROW(plugin->submitMetrics(""), nsclient::core::plugin_exception);
  EXPECT_THROW(plugin->handleMessage("x", 1), nsclient::core::plugin_exception);
  EXPECT_THROW(plugin->commandLineExec(true, request, reply), nsclient::core::plugin_exception);
}
