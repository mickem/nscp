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
  std::vector<std::string> debugs;

  void trace(const std::string &, const char *, int, const std::string &) override {}
  void debug(const std::string &, const char *, int, const std::string &message) override { debugs.push_back(message); }
  void info(const std::string &, const char *, int, const std::string &) override {}
  void warning(const std::string &, const char *, int, const std::string &) override {}
  void error(const std::string &, const char *, int, const std::string &message) override { errors.push_back(message); }
  void critical(const std::string &, const char *, int, const std::string &message) override { errors.push_back(message); }
  bool should_trace() const override { return false; }
  bool should_debug() const override { return true; }
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
  std::size_t debug_count(const std::string &needle) const {
    return std::count_if(debugs.begin(), debugs.end(), [&](const std::string &e) { return e.find(needle) != std::string::npos; });
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
    // ${exe-path} too: a module missing from the module folder is looked for
    // in ${exe-path}/modules next, and on a build where that is the build's
    // own module folder the real CheckExternalScripts would load and stay.
    paths_->set_overrides({{"scripts", scripts_.string()}, {"exe-path", dir_.string()}});
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

TEST_F(ZipBundleTest, a_probe_is_logged_as_one_and_takes_no_plugin_id) {
  // The web UI probes a module by name just before loading it. That used to
  // log "Loading module" twice and leave a gap in the plugin ids, which read
  // as the module having been loaded twice.
  make_bundle("bundle", {{"module.json", R"({"name":"Bundle","description":"from disk"})"}});
  make_bundle("other", {{"module.json", R"({"name":"Other","description":""})"}});
  bool loaded = false;
  const auto first = plugins_->only_load_module("other", "", loaded);
  ASSERT_TRUE(loaded);

  const auto probe = plugins_->probe_module("bundle", loaded);
  ASSERT_TRUE(loaded);
  ASSERT_TRUE(probe);
  EXPECT_EQ(nsclient::core::plugin_interface::no_plugin_id, probe->get_id());
  EXPECT_EQ("from disk", probe->getDescription());
  EXPECT_EQ(1u, logger_->debug_count("Inspecting module"));
  EXPECT_EQ(1u, logger_->debug_count("Loading module"));  // "other", not the probe

  const auto real = plugins_->only_load_module("bundle", "", loaded);
  ASSERT_TRUE(loaded);
  EXPECT_EQ(first->get_id() + 1, real->get_id());
  EXPECT_EQ(2u, logger_->debug_count("Loading module"));
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

// ===== the plugin manager's lifecycle, driven by a bundle ===================
//
// A bundle is a module the plugin manager can load without a shared library,
// so it stands in for one here: activating modules from [/modules], starting,
// reloading, cloning and stopping them.

namespace {
void set_module(const std::string &key, const std::string &value) { settings_manager::get_settings()->set_string("/modules", key, value); }
const zip_test::zip_entry kSimpleModule{"module.json", R"({"name":"Bundle","description":"","scripts":["check.sh"]})"};
const zip_test::zip_entry kSimpleScript{"check.sh", "echo ok"};
}  // namespace

TEST_F(ZipBundleTest, load_active_plugins_loads_what_the_modules_section_enables) {
  make_bundle("bundle", {kSimpleModule, kSimpleScript});
  make_bundle("other", {kSimpleModule, kSimpleScript});
  set_module("bundle", "enabled");
  set_module("other", "disabled");

  plugins_->load_active_plugins();
  EXPECT_EQ(std::vector<std::string>{"bundle"}, plugins_->get_loaded_modules());
  // Registered, not yet started: nothing is unpacked until start_plugins.
  EXPECT_FALSE(fs::exists(unpacked("bundle") / "check.sh"));

  plugins_->start_plugins(NSCAPI::normalStart);
  EXPECT_TRUE(fs::exists(unpacked("bundle") / "check.sh"));
}

TEST_F(ZipBundleTest, a_module_can_be_loaded_under_an_alias) {
  make_bundle("bundle", {kSimpleModule, kSimpleScript});
  set_module("mine", "bundle");

  plugins_->load_active_plugins();
  ASSERT_EQ(std::vector<std::string>{"bundle"}, plugins_->get_loaded_modules());
  EXPECT_TRUE(plugins_->get_plugin_cache()->has_module("mine"));
  EXPECT_TRUE(plugins_->reload_plugin("mine"));
}

TEST_F(ZipBundleTest, an_enabled_module_missing_from_disk_is_reported_and_skipped) {
  make_bundle("bundle", {kSimpleModule, kSimpleScript});
  set_module("bundle", "enabled");
  set_module("NotInstalled", "enabled");

  plugins_->load_active_plugins();
  EXPECT_EQ(std::vector<std::string>{"bundle"}, plugins_->get_loaded_modules());
  EXPECT_TRUE(logger_->logged("NotInstalled"));
}

TEST_F(ZipBundleTest, load_active_plugins_needs_a_module_path) {
  const auto unset = std::make_shared<nsclient::core::plugin_manager>(paths_, logger_);
  EXPECT_THROW(unset->load_active_plugins(), std::exception);
}

TEST_F(ZipBundleTest, load_all_plugins_loads_disabled_modules_too) {
  // "All" is what `nscp settings --generate` and the module listing want:
  // every module named in [/modules], whatever it is set to.
  make_bundle("bundle", {kSimpleModule, kSimpleScript});
  set_module("bundle", "disabled");

  plugins_->load_all_plugins();
  EXPECT_EQ(std::vector<std::string>{"bundle"}, plugins_->get_loaded_modules());
}

TEST_F(ZipBundleTest, is_enabled_reads_the_modules_section) {
  set_module("on", "enabled");
  set_module("one", "1");
  set_module("off", "disabled");
  set_module("zero", "0");
  set_module("no", "false");
  set_module("aliased", "SomeModule");

  EXPECT_TRUE(plugins_->is_enabled("on"));
  EXPECT_TRUE(plugins_->is_enabled("one"));
  EXPECT_TRUE(plugins_->is_enabled("aliased"));
  EXPECT_FALSE(plugins_->is_enabled("off"));
  EXPECT_FALSE(plugins_->is_enabled("zero"));
  EXPECT_FALSE(plugins_->is_enabled("no"));
  EXPECT_FALSE(plugins_->is_enabled("missing"));
}

TEST_F(ZipBundleTest, enable_and_disable_write_the_modules_section) {
  ASSERT_TRUE(plugins_->enable_plugin("CheckThing"));
  EXPECT_TRUE(plugins_->is_enabled("CheckThing"));
  EXPECT_EQ("enabled", settings_manager::get_settings()->get_string("/modules", "CheckThing", ""));

  ASSERT_TRUE(plugins_->disable_plugin("CheckThing"));
  EXPECT_FALSE(plugins_->is_enabled("CheckThing"));
  EXPECT_EQ("disabled", settings_manager::get_settings()->get_string("/modules", "CheckThing", ""));
}

TEST_F(ZipBundleTest, reloading_a_bundle_unpacks_it_again) {
  make_bundle("bundle", {kSimpleModule, kSimpleScript});
  ASSERT_TRUE(plugins_->load_single_plugin("bundle", "", true));
  fs::remove(unpacked("bundle") / "check.sh");

  EXPECT_TRUE(plugins_->reload_plugin("bundle"));
  EXPECT_TRUE(fs::exists(unpacked("bundle") / "check.sh"));
}

TEST_F(ZipBundleTest, a_loaded_module_can_be_found_and_cloned_by_id) {
  make_bundle("bundle", {kSimpleModule, kSimpleScript});
  ASSERT_TRUE(plugins_->load_single_plugin("bundle", "", false));
  const auto id = plugins_->get_plugin_cache()->find_plugin("bundle");
  ASSERT_TRUE(id);

  EXPECT_EQ("bundle", plugins_->find_plugin(id.value())->getModule());
  const int clone = plugins_->clone_plugin(id.value());
  EXPECT_GE(clone, 0);
  EXPECT_NE(static_cast<int>(id.value()), clone);
  EXPECT_FALSE(plugins_->is_dispatching_on_this_thread("bundle"));
}

TEST_F(ZipBundleTest, the_shutdown_sequence_unloads_every_module) {
  make_bundle("bundle", {kSimpleModule, kSimpleScript});
  make_bundle("second", {kSimpleModule, kSimpleScript});
  set_module("bundle", "enabled");
  set_module("second", "enabled");
  plugins_->load_active_plugins();
  plugins_->start_plugins(NSCAPI::normalStart);
  plugins_->post_start_plugins();
  ASSERT_TRUE(fs::exists(unpacked("bundle") / "check.sh"));
  ASSERT_TRUE(fs::exists(unpacked("second") / "check.sh"));

  plugins_->prepare_shutdown_plugins();
  plugins_->stop_plugins();
  EXPECT_TRUE(plugins_->get_loaded_modules().empty());
  EXPECT_FALSE(fs::exists(unpacked("bundle")));
  EXPECT_FALSE(fs::exists(unpacked("second")));
}
