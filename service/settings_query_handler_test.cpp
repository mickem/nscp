// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for service/settings_query_handler.cpp - the settings API every remote
// caller reaches: the REST settings endpoints, `nscp settings`, and any module
// that registers or reads configuration through the plugin API all arrive
// here as a SettingsRequestMessage.
//
// The handler works on the process-wide settings manager rather than an
// injected store, so each test boots one over a throwaway INI file in a temp
// dir and tears it down again. That is heavier than a pure unit test but it is
// the real thing: registration, reads, writes, save/load and the diff all go
// through the same store the daemon uses.

#include "settings_query_handler.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <memory>
#include <settings/test_helpers.hpp>
#include <string>

#include "../libs/settings_manager/settings_manager_impl.h"

using nsclient::core::settings_query_handler;

namespace {

// Minimal provider: paths are used verbatim (the tests hand out absolute temp
// paths) and nothing is logged.
class test_provider : public settings_manager::provider_interface {
 public:
  test_provider() : logger_(settings_test::make_null_logger()) {}
  std::string expand_path(std::string file) override { return file; }
  nsclient::logging::logger_instance get_logger() const override { return logger_; }
  void apply_path_overrides(std::map<std::string, std::string>) override {}

 private:
  nsclient::logging::logger_instance logger_;
};

// The handler only reaches into the core for the logger and the plugin cache
// (to turn plugin ids into names); the rest of core_interface is never touched
// on these paths.
class test_core : public nsclient::core::core_interface {
 public:
  test_core() : logger_(settings_test::make_null_logger()), plugin_cache_(new nsclient::core::plugin_cache(logger_)) {}

  nsclient::logging::logger_instance get_logger() override { return logger_; }
  nsclient::core::plugin_mgr_instance get_plugin_manager() override { return {}; }
  nsclient::core::path_instance get_path() override { return {}; }
  nsclient::core::plugin_cache *get_plugin_cache() override { return plugin_cache_.get(); }
  nsclient::core::storage_manager_instance get_storage_manager() override { return {}; }

 private:
  nsclient::logging::logger_instance logger_;
  std::unique_ptr<nsclient::core::plugin_cache> plugin_cache_;
};

class SettingsQueryTest : public ::testing::Test {
 protected:
  settings_test::temp_dir dir_;
  boost::filesystem::path ini_;
  std::unique_ptr<test_provider> provider_;
  std::shared_ptr<test_core> core_;

  void SetUp() override {
    ini_ = dir_.file("settings.ini");
    settings_test::write_file(ini_, "");
    provider_ = std::make_unique<test_provider>();
    core_ = std::make_shared<test_core>();
    ASSERT_TRUE(settings_manager::init_settings(provider_.get(), context()));
  }

  void TearDown() override { settings_manager::destroy_settings(); }

  std::string context() const {
    std::string p = ini_.generic_string();
    if (!p.empty() && p.front() == '/') p.erase(0, 1);
    return "ini:///" + p;
  }

  // Run one request through the handler and hand back the response.
  PB::Settings::SettingsResponseMessage run(const PB::Settings::SettingsRequestMessage &request) {
    PB::Settings::SettingsResponseMessage response;
    settings_query_handler(core_, request).parse(response);
    return response;
  }

  // A request with a single payload; the caller fills in the action.
  static PB::Settings::SettingsRequestMessage::Request *new_request(PB::Settings::SettingsRequestMessage &request, int plugin_id = 1234) {
    PB::Settings::SettingsRequestMessage::Request *payload = request.add_payload();
    payload->set_plugin_id(plugin_id);
    return payload;
  }

  // Set a value the way a caller would: through an update request.
  void set_value(const std::string &path, const std::string &key, const std::string &value) {
    PB::Settings::SettingsRequestMessage request;
    PB::Settings::SettingsRequestMessage::Request *payload = new_request(request);
    PB::Settings::Node *node = payload->mutable_update()->mutable_node();
    node->set_path(path);
    node->set_key(key);
    node->set_value(value);
    run(request);
  }

  void register_key(const std::string &path, const std::string &key, bool sensitive = false) {
    PB::Settings::SettingsRequestMessage request;
    PB::Settings::SettingsRequestMessage::Request *payload = new_request(request);
    PB::Settings::SettingsRequestMessage::Request::Registration *reg = payload->mutable_registration();
    reg->mutable_node()->set_path(path);
    reg->mutable_node()->set_key(key);
    reg->mutable_info()->set_type("string");
    reg->mutable_info()->set_title("A key");
    reg->mutable_info()->set_description("Describes a key");
    reg->mutable_info()->set_default_value("the-default");
    reg->mutable_info()->set_is_sensitive(sensitive);
    run(request);
  }
};

}  // namespace

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

TEST_F(SettingsQueryTest, a_request_without_an_action_is_rejected) {
  PB::Settings::SettingsRequestMessage request;
  new_request(request);

  const PB::Settings::SettingsResponseMessage response = run(request);

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().message(), "Settings error: Invalid action");
}

TEST_F(SettingsQueryTest, every_payload_gets_its_own_response) {
  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_status();
  new_request(request)->mutable_status();

  EXPECT_EQ(run(request).payload_size(), 2);
}

TEST_F(SettingsQueryTest, status_reports_the_store_behind_the_settings) {
  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_status();

  const PB::Settings::SettingsResponseMessage response = run(request);

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).status().type(), "ini");
  EXPECT_FALSE(response.payload(0).status().has_changed()) << "nothing written yet";
}

TEST_F(SettingsQueryTest, status_reports_pending_changes) {
  set_value("/section", "key", "value");

  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_status();

  EXPECT_TRUE(run(request).payload(0).status().has_changed());
}

// ---------------------------------------------------------------------------
// Update and query - the read/write pair behind `nscp settings --set/--show`.
// ---------------------------------------------------------------------------

TEST_F(SettingsQueryTest, a_value_written_can_be_read_back) {
  set_value("/section", "key", "value");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Query *q = new_request(request)->mutable_query();
  q->mutable_node()->set_path("/section");
  q->mutable_node()->set_key("key");

  const PB::Settings::SettingsResponseMessage response = run(request);

  EXPECT_EQ(response.payload(0).query().node().value(), "value");
}

TEST_F(SettingsQueryTest, a_missing_value_comes_back_as_the_requested_default) {
  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Query *q = new_request(request)->mutable_query();
  q->mutable_node()->set_path("/section");
  q->mutable_node()->set_key("nothing-here");
  q->set_default_value("fallback");

  EXPECT_EQ(run(request).payload(0).query().node().value(), "fallback");
}

TEST_F(SettingsQueryTest, a_key_update_with_no_value_removes_the_key) {
  set_value("/section", "key", "value");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::Node *node = new_request(request)->mutable_update()->mutable_node();
  node->set_path("/section");
  node->set_key("key");
  run(request);

  PB::Settings::SettingsRequestMessage read;
  PB::Settings::SettingsRequestMessage::Request::Query *q = new_request(read)->mutable_query();
  q->mutable_node()->set_path("/section");
  q->mutable_node()->set_key("key");
  EXPECT_EQ(run(read).payload(0).query().node().value(), "");
}

TEST_F(SettingsQueryTest, an_update_with_neither_key_nor_value_removes_the_path) {
  set_value("/section", "key", "value");

  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_update()->mutable_node()->set_path("/section");
  run(request);

  PB::Settings::SettingsRequestMessage read;
  PB::Settings::SettingsRequestMessage::Request::Query *q = new_request(read)->mutable_query();
  q->mutable_node()->set_path("/section");
  q->set_include_keys(true);
  EXPECT_EQ(run(read).payload(0).query().nodes_size(), 0);
}

TEST_F(SettingsQueryTest, a_path_query_lists_the_keys_underneath_it) {
  set_value("/section", "first", "1");
  set_value("/section", "second", "2");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Query *q = new_request(request)->mutable_query();
  q->mutable_node()->set_path("/section");
  q->set_include_keys(true);

  const PB::Settings::SettingsResponseMessage response = run(request);

  EXPECT_EQ(response.payload(0).query().nodes_size(), 2);
}

TEST_F(SettingsQueryTest, a_recursive_query_walks_into_sub_sections) {
  set_value("/section/child", "key", "value");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Query *q = new_request(request)->mutable_query();
  q->mutable_node()->set_path("/section");
  q->set_recursive(true);

  const PB::Settings::SettingsResponseMessage response = run(request);

  ASSERT_GT(response.payload(0).query().nodes_size(), 0);
  EXPECT_EQ(response.payload(0).query().nodes(0).path(), "/section/child");
}

TEST_F(SettingsQueryTest, a_trailing_slash_on_the_queried_path_is_ignored) {
  // recurse_find strips it; without that the child lookup would go looking
  // under "/section//child".
  set_value("/section", "key", "value");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Query *q = new_request(request)->mutable_query();
  q->mutable_node()->set_path("/section/");
  q->set_include_keys(true);

  EXPECT_EQ(run(request).payload(0).query().nodes_size(), 1);
}

TEST_F(SettingsQueryTest, a_sensitive_value_is_redacted_when_asked) {
  register_key("/section", "password", /*sensitive=*/true);
  set_value("/section", "password", "hunter2");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Query *q = new_request(request)->mutable_query();
  q->mutable_node()->set_path("/section");
  q->mutable_node()->set_key("password");
  q->set_redact_sensitive(true);

  EXPECT_EQ(run(request).payload(0).query().node().value(), "***");
}

TEST_F(SettingsQueryTest, a_sensitive_value_is_returned_when_redaction_is_off) {
  // The daemon itself has to be able to read the value it stored.
  register_key("/section", "password", /*sensitive=*/true);
  set_value("/section", "password", "hunter2");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Query *q = new_request(request)->mutable_query();
  q->mutable_node()->set_path("/section");
  q->mutable_node()->set_key("password");

  EXPECT_EQ(run(request).payload(0).query().node().value(), "hunter2");
}

TEST_F(SettingsQueryTest, an_ordinary_value_is_untouched_by_redaction) {
  register_key("/section", "key");
  set_value("/section", "key", "value");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Query *q = new_request(request)->mutable_query();
  q->mutable_node()->set_path("/section");
  q->mutable_node()->set_key("key");
  q->set_redact_sensitive(true);

  EXPECT_EQ(run(request).payload(0).query().node().value(), "value");
}

// ---------------------------------------------------------------------------
// Registration - what a module does at load time so its configuration shows up
// in the docs, the web UI and --add-defaults.
// ---------------------------------------------------------------------------

TEST_F(SettingsQueryTest, a_registered_key_is_reported_by_the_inventory) {
  register_key("/section", "key");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Inventory *inv = new_request(request)->mutable_inventory();
  inv->mutable_node()->set_path("/section");
  inv->mutable_node()->set_key("key");

  const PB::Settings::SettingsResponseMessage response = run(request);

  ASSERT_EQ(response.payload(0).inventory_size(), 1);
  EXPECT_EQ(response.payload(0).inventory(0).info().title(), "A key");
  EXPECT_EQ(response.payload(0).inventory(0).info().type(), "string");
}

TEST_F(SettingsQueryTest, an_inventory_of_a_path_lists_its_registered_keys) {
  register_key("/section", "key");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Inventory *inv = new_request(request)->mutable_inventory();
  inv->mutable_node()->set_path("/section");
  inv->set_fetch_keys(true);

  const PB::Settings::SettingsResponseMessage response = run(request);

  ASSERT_GE(response.payload(0).inventory_size(), 1);
  EXPECT_EQ(response.payload(0).inventory(0).node().key(), "key");
  EXPECT_EQ(response.payload(0).inventory(0).info().default_value(), "the-default");
}

TEST_F(SettingsQueryTest, an_inventory_reports_keys_nobody_registered_too) {
  // Configuration in the file that no module claims still has to be visible,
  // otherwise the web UI silently drops whatever it does not understand.
  set_value("/section", "stray", "value");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Inventory *inv = new_request(request)->mutable_inventory();
  inv->mutable_node()->set_path("/section");
  inv->set_fetch_keys(true);

  const PB::Settings::SettingsResponseMessage response = run(request);

  ASSERT_EQ(response.payload(0).inventory_size(), 1);
  EXPECT_EQ(response.payload(0).inventory(0).node().key(), "stray");
  EXPECT_TRUE(response.payload(0).inventory(0).info().advanced()) << "unregistered keys are reported as advanced";
}

TEST_F(SettingsQueryTest, a_registered_path_is_reported_as_a_folder) {
  PB::Settings::SettingsRequestMessage reg_request;
  PB::Settings::SettingsRequestMessage::Request::Registration *reg = new_request(reg_request)->mutable_registration();
  reg->mutable_node()->set_path("/section");
  reg->mutable_info()->set_title("A section");
  reg->mutable_info()->set_description("Describes a section");
  run(reg_request);

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Inventory *inv = new_request(request)->mutable_inventory();
  inv->mutable_node()->set_path("/section");
  inv->set_fetch_paths(true);

  const PB::Settings::SettingsResponseMessage response = run(request);

  ASSERT_EQ(response.payload(0).inventory_size(), 1);
  EXPECT_EQ(response.payload(0).inventory(0).info().type(), "folder");
  EXPECT_EQ(response.payload(0).inventory(0).info().title(), "A section");
}

TEST_F(SettingsQueryTest, a_registered_template_is_reported_by_the_inventory) {
  PB::Settings::SettingsRequestMessage reg_request;
  PB::Settings::SettingsRequestMessage::Request::Registration *reg = new_request(reg_request)->mutable_registration();
  reg->mutable_node()->set_path("/section");
  reg->mutable_info()->set_title("A template");
  reg->set_fields(R"({"fields": []})");
  run(reg_request);

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Inventory *inv = new_request(request)->mutable_inventory();
  inv->set_fetch_templates(true);

  const PB::Settings::SettingsResponseMessage response = run(request);

  ASSERT_EQ(response.payload(0).inventory_size(), 1);
  EXPECT_EQ(response.payload(0).inventory(0).info().type(), "template");
  EXPECT_TRUE(response.payload(0).inventory(0).info().is_template());
}

TEST_F(SettingsQueryTest, malformed_template_fields_do_not_take_the_registration_down) {
  // The payload is whatever the module handed us; bad json is logged and the
  // rest of the template still registers.
  PB::Settings::SettingsRequestMessage reg_request;
  PB::Settings::SettingsRequestMessage::Request::Registration *reg = new_request(reg_request)->mutable_registration();
  reg->mutable_node()->set_path("/section");
  reg->mutable_info()->set_title("A template");
  reg->set_fields("{not json");

  const PB::Settings::SettingsResponseMessage response = run(reg_request);

  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_OK);
}

// ---------------------------------------------------------------------------
// Control - load and save.
// ---------------------------------------------------------------------------

TEST_F(SettingsQueryTest, save_writes_the_pending_changes_to_disk) {
  set_value("/section", "key", "value");

  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_control()->set_command(PB::Settings::Command::SAVE);
  run(request);

  EXPECT_NE(settings_test::read_file(ini_).find("value"), std::string::npos);
}

TEST_F(SettingsQueryTest, a_control_command_that_is_neither_load_nor_save_says_so) {
  // RELOAD is in the protocol but the handler only implements LOAD and SAVE.
  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_control()->set_command(PB::Settings::Command::RELOAD);

  EXPECT_EQ(run(request).payload(0).result().message(), "Unknown command");
}

// ---------------------------------------------------------------------------
// Diff - what an operator-facing "what am I about to save?" view is built on.
// ---------------------------------------------------------------------------

TEST_F(SettingsQueryTest, a_pending_write_shows_up_in_the_diff) {
  set_value("/section", "key", "value");

  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_diff();

  const PB::Settings::SettingsResponseMessage response = run(request);

  bool found = false;
  for (const auto &e : response.payload(0).diff().entries()) {
    if (e.path() == "/section" && e.key() == "key") {
      EXPECT_EQ(e.change_type(), PB::Settings::SettingsResponseMessage::Response::Diff::ADDED);
      EXPECT_EQ(e.new_value(), "value");
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(SettingsQueryTest, the_diff_is_empty_once_the_changes_are_saved) {
  set_value("/section", "key", "value");

  PB::Settings::SettingsRequestMessage save;
  new_request(save)->mutable_control()->set_command(PB::Settings::Command::SAVE);
  run(save);

  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_diff();

  EXPECT_EQ(run(request).payload(0).diff().entries_size(), 0);
}

TEST_F(SettingsQueryTest, a_diff_filter_keeps_only_the_requested_path) {
  set_value("/wanted", "key", "value");
  set_value("/unwanted", "key", "value");

  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_diff()->mutable_node()->set_path("/wanted");

  const PB::Settings::SettingsResponseMessage response = run(request);

  for (const auto &e : response.payload(0).diff().entries()) {
    EXPECT_EQ(e.path(), "/wanted") << "an unrelated path leaked past the filter";
  }
  EXPECT_GT(response.payload(0).diff().entries_size(), 0);
}

TEST_F(SettingsQueryTest, a_non_recursive_diff_filter_excludes_sub_paths) {
  set_value("/section/child", "key", "value");

  PB::Settings::SettingsRequestMessage unfiltered;
  new_request(unfiltered)->mutable_diff();
  ASSERT_GT(run(unfiltered).payload(0).diff().entries_size(), 0) << "the change is pending to begin with";

  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_diff()->mutable_node()->set_path("/section");

  EXPECT_EQ(run(request).payload(0).diff().entries_size(), 0);
}

TEST_F(SettingsQueryTest, a_recursive_diff_filter_includes_sub_paths) {
  set_value("/section/child", "key", "value");

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Diff *diff = new_request(request)->mutable_diff();
  diff->mutable_node()->set_path("/section");
  diff->set_recursive(true);

  EXPECT_GT(run(request).payload(0).diff().entries_size(), 0);
}

TEST_F(SettingsQueryTest, a_recursive_diff_filter_does_not_match_a_mere_prefix) {
  // "/section" must not swallow "/section-other": the match is on path
  // segments, not characters.
  set_value("/section-other", "key", "value");

  PB::Settings::SettingsRequestMessage unfiltered;
  new_request(unfiltered)->mutable_diff();
  ASSERT_GT(run(unfiltered).payload(0).diff().entries_size(), 0) << "the change is pending to begin with";

  PB::Settings::SettingsRequestMessage request;
  PB::Settings::SettingsRequestMessage::Request::Diff *diff = new_request(request)->mutable_diff();
  diff->mutable_node()->set_path("/section");
  diff->set_recursive(true);

  EXPECT_EQ(run(request).payload(0).diff().entries_size(), 0);
}

TEST_F(SettingsQueryTest, a_sensitive_value_is_masked_in_the_diff) {
  register_key("/section", "password", /*sensitive=*/true);
  set_value("/section", "password", "hunter2");

  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_diff();

  const PB::Settings::SettingsResponseMessage response = run(request);

  bool found = false;
  for (const auto &e : response.payload(0).diff().entries()) {
    if (e.key() != "password") continue;
    found = true;
    EXPECT_TRUE(e.is_sensitive());
    EXPECT_EQ(e.new_value(), "***");
  }
  EXPECT_TRUE(found);
}

TEST_F(SettingsQueryTest, a_removal_is_reported_as_removed) {
  set_value("/section", "key", "value");
  PB::Settings::SettingsRequestMessage save;
  new_request(save)->mutable_control()->set_command(PB::Settings::Command::SAVE);
  run(save);

  PB::Settings::SettingsRequestMessage remove;
  PB::Settings::Node *node = new_request(remove)->mutable_update()->mutable_node();
  node->set_path("/section");
  node->set_key("key");
  run(remove);

  PB::Settings::SettingsRequestMessage request;
  new_request(request)->mutable_diff();

  const PB::Settings::SettingsResponseMessage response = run(request);

  bool found = false;
  for (const auto &e : response.payload(0).diff().entries()) {
    if (e.path() == "/section" && e.key() == "key") {
      EXPECT_EQ(e.change_type(), PB::Settings::SettingsResponseMessage::Response::Diff::REMOVED);
      EXPECT_EQ(e.old_value(), "value");
      found = true;
    }
  }
  EXPECT_TRUE(found);
}
