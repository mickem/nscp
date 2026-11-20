// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the zip plugin loader - the thing behind a ".zip module": an
// archive carrying a module.json that names the scripts to install, the
// modules they need and the commands to run once loaded.
//
// Everything here stops at the metadata: the constructor opens the archive and
// parses module.json, which is where an operator's mistake (a missing file, a
// typo in the json, a script under a name nothing can provide) turns into a
// clear error or a silently wrong provider. Installing the scripts needs a
// plugin manager to load the bundle through; zip_bundle_test.cpp covers that.
//
// The archives are built here rather than checked in, because the interesting
// cases are all about *what module.json says* - a fixture per variation would
// mean a dozen opaque binaries in the tree.

#include "zip_plugin.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "zip_test_writer.hpp"

namespace fs = boost::filesystem;
using nsclient::core::zip_plugin;

namespace {

using zip_test::write_zip;
using zip_test::zip_entry;

class ZipPluginTest : public ::testing::Test {
 protected:
  fs::path dir_;

  void SetUp() override {
    dir_ = fs::temp_directory_path() / fs::unique_path("nscp-zipplugin-%%%%-%%%%");
    fs::create_directories(dir_);
  }
  void TearDown() override {
    boost::system::error_code ec;
    fs::remove_all(dir_, ec);
  }

  // Build an archive and hand back a loaded plugin for it. The paths and
  // plugin-manager handles stay null: reading the metadata never touches them,
  // and installing the scripts (which does) is out of scope here.
  fs::path make_archive(const std::string &name, const std::vector<zip_entry> &entries) {
    const fs::path path = dir_ / name;
    write_zip(path, entries);
    return path;
  }

  static std::unique_ptr<zip_plugin> open(const fs::path &archive) {
    return std::make_unique<zip_plugin>(1, archive, "", nsclient::core::path_instance(), nsclient::core::plugin_mgr_instance(),
                                        nsclient::logging::logger_instance());
  }
};

}  // namespace

TEST_F(ZipPluginTest, reads_the_name_and_description_from_module_json) {
  const fs::path archive = make_archive("sample.zip", {{"module.json", R"({"name":"Sample","description":"A sample module"})"}});

  const auto plugin = open(archive);

  EXPECT_EQ(plugin->getName(), "Sample");
  EXPECT_EQ(plugin->getDescription(), "A sample module");
}

TEST_F(ZipPluginTest, the_module_name_comes_from_the_archive_file_name) {
  // The extracted scripts land in a folder named after the archive, so the
  // ".zip" has to come off - otherwise every path carries it.
  const fs::path archive = make_archive("my-module.zip", {{"module.json", R"({"name":"Sample","description":""})"}});

  EXPECT_EQ(open(archive)->getModule(), "my-module");
}

TEST_F(ZipPluginTest, an_archive_without_module_json_is_rejected) {
  const fs::path archive = make_archive("no-metadata.zip", {{"readme.txt", "nothing to see"}});

  EXPECT_THROW(open(archive), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, a_file_that_is_not_an_archive_is_rejected) {
  const fs::path path = dir_ / "not-a-zip.zip";
  std::ofstream(path.string().c_str()) << "this is not a zip file";

  EXPECT_THROW(open(path), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, a_missing_file_is_rejected) {
  EXPECT_THROW(open(dir_ / "does-not-exist.zip"), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, malformed_json_is_rejected) {
  const fs::path archive = make_archive("bad-json.zip", {{"module.json", "{not json at all"}});

  EXPECT_THROW(open(archive), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, module_json_without_a_name_is_rejected) {
  // as_string() on a missing key throws, and that has to surface as the
  // module failing to load rather than as an unhandled boost::json error.
  const fs::path archive = make_archive("no-name.zip", {{"module.json", R"({"description":"no name here"})"}});

  EXPECT_THROW(open(archive), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, accepts_the_full_metadata_shape) {
  // Everything module.json may carry, in the two forms a script entry takes:
  // a bare file name, and an object naming its provider explicitly.
  const fs::path archive = make_archive("full.zip", {{"module.json", R"({
        "name": "Full",
        "description": "Every key",
        "scripts": [
          "check_something.py",
          "check_other.sh",
          {"provider": "LUAScript", "script": "s.lua", "alias": "a", "command": "c"}
        ],
        "modules": ["CheckHelpers"],
        "on_start": ["CheckHelpers.add --name x"]
      })"},
                                                     {"check_something.py", "print('hi')\n"},
                                                     {"check_other.sh", "echo hi\n"},
                                                     {"s.lua", "-- lua\n"}});

  const auto plugin = open(archive);

  EXPECT_EQ(plugin->getName(), "Full");
  EXPECT_EQ(plugin->getDescription(), "Every key");
}

TEST_F(ZipPluginTest, a_script_entry_that_is_not_a_string_or_an_object_is_rejected) {
  const fs::path archive = make_archive("bad-script.zip", {{"module.json", R"({"name":"X","description":"","scripts":[42]})"}});

  EXPECT_THROW(open(archive), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, a_script_object_missing_its_keys_is_rejected) {
  const fs::path archive = make_archive("partial-script.zip", {{"module.json", R"({"name":"X","description":"","scripts":[{"script":"s.lua"}]})"}});

  EXPECT_THROW(open(archive), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, the_metadata_may_be_preceded_by_other_entries) {
  // read_metadata walks the whole archive looking for module.json rather than
  // assuming it comes first.
  const fs::path archive = make_archive("late-metadata.zip", {{"a.txt", "first"},
                                                              {"b.txt", "second"},
                                                              {"module.json", R"({"name":"Late","description":"found anyway"})"}});

  EXPECT_EQ(open(archive)->getName(), "Late");
}

TEST_F(ZipPluginTest, reports_a_fixed_version_and_never_claims_duplicates) {
  const fs::path archive = make_archive("simple.zip", {{"module.json", R"({"name":"Simple","description":""})"}});
  const auto plugin = open(archive);

  EXPECT_EQ(plugin->get_version(), "1.0.0");
  EXPECT_FALSE(plugin->is_duplicate(archive, ""));
  EXPECT_FALSE(plugin->hasCommandHandler());
  EXPECT_FALSE(plugin->hasNotificationHandler());
  EXPECT_FALSE(plugin->has_on_event());
}

TEST_F(ZipPluginTest, has_no_lifecycle_hooks_beyond_load_and_unload) {
  // A zip module's whole lifecycle is "install the scripts on load": it has no
  // start phase, nothing to prepare before shutdown, and start_plugin is a
  // fixed yes so the plugin manager treats the load as complete.
  const fs::path archive = make_archive("simple.zip", {{"module.json", R"({"name":"Simple","description":""})"}});
  const auto plugin = open(archive);

  EXPECT_FALSE(plugin->has_start());
  EXPECT_TRUE(plugin->start_plugin());
  EXPECT_FALSE(plugin->has_prepare_shutdown());
  EXPECT_NO_THROW(plugin->prepare_shutdown_plugin());
}

TEST_F(ZipPluginTest, advertises_no_handlers_or_metrics) {
  // The capability flags drive dispatch in the plugin manager; a zip module
  // must opt out of everything or it would be handed traffic its handlers
  // (which all throw) cannot serve.
  const fs::path archive = make_archive("simple.zip", {{"module.json", R"({"name":"Simple","description":""})"}});
  const auto plugin = open(archive);

  EXPECT_FALSE(plugin->hasMessageHandler());
  EXPECT_FALSE(plugin->has_command_line_exec());
  EXPECT_FALSE(plugin->has_routing_handler());
  EXPECT_FALSE(plugin->hasMetricsFetcher());
  EXPECT_FALSE(plugin->hasMetricsSubmitter());
}

TEST_F(ZipPluginTest, log_messages_are_swallowed) {
  // Both log sinks - the static one and the logging_subscriber override - are
  // deliberate no-ops: a zip module has no DLL to forward log records into.
  std::string payload = "a log line";
  EXPECT_NO_THROW(zip_plugin::on_log_message(payload));
  EXPECT_EQ(payload, "a log line");

  const fs::path archive = make_archive("simple.zip", {{"module.json", R"({"name":"Simple","description":""})"}});
  const auto plugin = open(archive);
  nsclient::logging::logging_subscriber &subscriber = *plugin;
  EXPECT_NO_THROW(subscriber.on_log_message(payload));
}

TEST_F(ZipPluginTest, routing_a_message_through_a_zip_module_is_an_error) {
  // A zip module carries scripts; it has no handlers of its own, so anything
  // that would dispatch to one has to fail loudly rather than silently drop.
  const fs::path archive = make_archive("simple.zip", {{"module.json", R"({"name":"Simple","description":""})"}});
  const auto plugin = open(archive);

  char *new_channel = nullptr;
  char *new_buffer = nullptr;
  unsigned int new_len = 0;
  EXPECT_THROW(plugin->route_message("chan", "data", 4, &new_channel, &new_buffer, &new_len), nsclient::core::plugin_exception);
}
