// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "plugin_manager.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nsclient/logger/logger.hpp>

#include "plugin_cache.hpp"

// Mock logger
class mock_logger : public nsclient::logging::log_interface {
 public:
  void trace(const std::string& module, const char* file, const int line, const std::string& message) override {}
  void debug(const std::string& module, const char* file, const int line, const std::string& message) override {}
  void info(const std::string& module, const char* file, const int line, const std::string& message) override {}
  void warning(const std::string& module, const char* file, const int line, const std::string& message) override {}
  void error(const std::string& module, const char* file, const int line, const std::string& message) override {}
  void critical(const std::string& module, const char* file, const int line, const std::string& message) override {}

  bool should_trace() const override { return false; }
  bool should_debug() const override { return false; }
  bool should_info() const override { return false; }
  bool should_warning() const override { return false; }
  bool should_error() const override { return false; }
  bool should_critical() const override { return false; }
};

// Mock loader

nscapi::core_api::FUNPTR NSAPILoader(const char* buffer) { return nullptr; }

TEST(plugin_manager_test, test_is_module) {
  boost::filesystem::path dll_path("test.dll");
  boost::filesystem::path so_path("test.so");
  boost::filesystem::path txt_path("test.txt");

#ifdef WIN32
  EXPECT_TRUE(nsclient::core::plugin_manager::is_module(dll_path));
  EXPECT_FALSE(nsclient::core::plugin_manager::is_module(so_path));
#else
  EXPECT_FALSE(nsclient::core::plugin_manager::is_module(dll_path));
  EXPECT_TRUE(nsclient::core::plugin_manager::is_module(so_path));
#endif
  EXPECT_FALSE(nsclient::core::plugin_manager::is_module(txt_path));
}

TEST(plugin_manager_test, test_file_to_module) {
  boost::filesystem::path dll_path("test.dll");
  boost::filesystem::path so_path("libtest.so");
  boost::filesystem::path simple_so_path("test.so");

#ifdef WIN32
  EXPECT_EQ(nsclient::core::plugin_manager::file_to_module(dll_path), "test");
#else
  EXPECT_EQ(nsclient::core::plugin_manager::file_to_module(so_path), "test");
  EXPECT_EQ(nsclient::core::plugin_manager::file_to_module(simple_so_path), "test");
#endif
}

// ===== extract_subject_from_header ========================================
//
// Tests for the policy decision point's subject resolution. The function
// is the trust boundary between metadata stamped by core_helper (a
// numeric plugin_id and an optional principal string) and the trusted
// module name the policy actually matches against.

namespace {

class MockSubjectLogger : public nsclient::logging::logger {
 public:
  void trace(const std::string&, const char*, int, const std::string&) override {}
  void debug(const std::string&, const char*, int, const std::string&) override {}
  void info(const std::string&, const char*, int, const std::string&) override {}
  void warning(const std::string&, const char*, int, const std::string&) override {}
  void error(const std::string&, const char*, int, const std::string&) override {}
  void critical(const std::string&, const char*, int, const std::string&) override {}
  bool should_trace() const override { return false; }
  bool should_debug() const override { return false; }
  bool should_info() const override { return false; }
  bool should_warning() const override { return false; }
  bool should_error() const override { return false; }
  bool should_critical() const override { return false; }
  void raw(const std::string&) override {}
  void add_subscriber(nsclient::logging::logging_subscriber_instance) override {}
  void remove_subscriber(nsclient::logging::logging_subscriber_instance) override {}
  void clear_subscribers() override {}
  bool startup() override { return false; }
  bool shutdown() override { return false; }
  void destroy() override {}
  void configure() override {}
  void set_log_level(std::string) override {}
  std::string get_log_level() const override { return ""; }
  void set_backend(std::string) override {}
};

PB::Common::Header make_header(const std::string& caller_plugin_id, const std::string& principal) {
  PB::Common::Header h;
  if (!caller_plugin_id.empty()) {
    auto* kv = h.add_metadata();
    kv->set_key("nscp.caller_plugin_id");
    kv->set_value(caller_plugin_id);
  }
  if (!principal.empty()) {
    auto* kv = h.add_metadata();
    kv->set_key("nscp.principal");
    kv->set_value(principal);
  }
  return h;
}

std::unique_ptr<nsclient::core::plugin_cache> make_cache_with(unsigned int id, const std::string& dll, const std::string& alias) {
  auto logger = std::make_shared<MockSubjectLogger>();
  auto cache = std::make_unique<nsclient::core::plugin_cache>(logger);
  nsclient::core::plugin_cache::plugin_cache_list_type list;
  nsclient::core::plugin_cache_item item;
  item.id = id;
  item.dll = dll;
  item.alias = alias;
  list.push_back(item);
  cache->add_plugins(list);
  return cache;
}

}  // namespace

TEST(plugin_manager_extract_subject, empty_header_yields_star) {
  auto cache = make_cache_with(7, "CheckSystem", "");
  PB::Common::Header h;  // no metadata at all
  EXPECT_EQ("*", nsclient::core::plugin_manager::extract_subject_from_header(h, cache.get()));
}

TEST(plugin_manager_extract_subject, null_cache_yields_star) {
  // No cache available - cannot resolve plugin_id - fall back to "*".
  PB::Common::Header h = make_header("42", "");
  EXPECT_EQ("*", nsclient::core::plugin_manager::extract_subject_from_header(h, nullptr));
}

TEST(plugin_manager_extract_subject, valid_plugin_id_resolves_via_alias) {
  // alias takes precedence over dll - matches plugin_cache::find_plugin_alias.
  auto cache = make_cache_with(7, "CheckSystem", "sys");
  PB::Common::Header h = make_header("7", "");
  EXPECT_EQ("sys", nsclient::core::plugin_manager::extract_subject_from_header(h, cache.get()));
}

TEST(plugin_manager_extract_subject, valid_plugin_id_falls_back_to_dll_when_no_alias) {
  auto cache = make_cache_with(7, "CheckSystem", "");
  PB::Common::Header h = make_header("7", "");
  EXPECT_EQ("CheckSystem", nsclient::core::plugin_manager::extract_subject_from_header(h, cache.get()));
}

TEST(plugin_manager_extract_subject, unknown_plugin_id_yields_star) {
  // "Failed to find plugin ..." from plugin_cache must NOT leak into the
  // subject as literal text - it has to become "*" so a strict allow-list
  // catches the unknown-caller case.
  auto cache = make_cache_with(7, "CheckSystem", "");
  PB::Common::Header h = make_header("999", "");
  EXPECT_EQ("*", nsclient::core::plugin_manager::extract_subject_from_header(h, cache.get()));
}

TEST(plugin_manager_extract_subject, non_numeric_plugin_id_yields_star) {
  // str::stox throws on non-numeric input - the catch block forces "*".
  auto cache = make_cache_with(7, "CheckSystem", "");
  PB::Common::Header h = make_header("not-a-number", "");
  EXPECT_EQ("*", nsclient::core::plugin_manager::extract_subject_from_header(h, cache.get()));
}

TEST(plugin_manager_extract_subject, principal_is_appended) {
  auto cache = make_cache_with(7, "WEBServer", "");
  PB::Common::Header h = make_header("7", "operator");
  EXPECT_EQ("WEBServer:operator", nsclient::core::plugin_manager::extract_subject_from_header(h, cache.get()));
}

TEST(plugin_manager_extract_subject, principal_without_resolvable_module_yields_star_colon_principal) {
  // Cache doesn't contain id 7 - module becomes "*". Principal still applies.
  auto cache = make_cache_with(99, "Other", "");
  PB::Common::Header h = make_header("7", "operator");
  EXPECT_EQ("*:operator", nsclient::core::plugin_manager::extract_subject_from_header(h, cache.get()));
}

TEST(plugin_manager_extract_subject, ignores_unrelated_metadata) {
  // Unknown nscp.* keys and user-defined keys must not perturb resolution.
  auto cache = make_cache_with(7, "CheckSystem", "");
  PB::Common::Header h = make_header("7", "");
  auto* extra = h.add_metadata();
  extra->set_key("nscp.unknown");
  extra->set_value("ignored");
  auto* user = h.add_metadata();
  user->set_key("some.user.key");
  user->set_value("also ignored");
  EXPECT_EQ("CheckSystem", nsclient::core::plugin_manager::extract_subject_from_header(h, cache.get()));
}

// The contract between a facts producer and the core: what a response does to
// the repository. Driven through the producer-side builder every module uses
// (nscapi::facts::response) and applied without a loaded module, so what is
// pinned here is the real path from a producer's calls to the stored document.
namespace {
std::string apply(const std::string& response, nsclient::core::fact_repository& facts, std::map<std::string, std::string>& errors,
                  std::set<std::string>* produced_out = nullptr, const unsigned int plugin_id = 1) {
  std::set<std::string> produced;
  const std::string failure = nsclient::core::plugin_manager::apply_facts_response(response, plugin_id, facts, errors, produced);
  if (produced_out != nullptr) *produced_out = produced;
  return failure;
}

std::string json_of(const nsclient::core::fact_repository& facts) { return facts.to_json(); }

bool has_set(const nsclient::core::fact_repository& facts, const std::string& name) {
  const PB::Facts::Object all = facts.get_all();
  return nscapi::facts::tree::get(all, name) != nullptr;
}
}  // namespace

TEST(plugin_manager_facts, a_returned_set_is_stored_and_counts_as_produced) {
  nsclient::core::fact_repository facts;
  std::map<std::string, std::string> errors;
  std::set<std::string> produced;
  nscapi::facts::response response;
  response.set("os").value("family", "linux");
  EXPECT_EQ(apply(response.serialize(), facts, errors, &produced), "");
  EXPECT_EQ(json_of(facts), R"({"os":{"family":"linux"}})");
  EXPECT_EQ(produced, std::set<std::string>{"os"});
  EXPECT_TRUE(errors.empty());
}

TEST(plugin_manager_facts, a_removed_set_is_dropped_and_not_claimed) {
  nsclient::core::fact_repository facts;
  std::map<std::string, std::string> errors;
  {
    nscapi::facts::response response;
    response.set("docker").value("version", "26.1.0");
    response.set("os").value("family", "linux");
    apply(response.serialize(), facts, errors);
  }
  std::set<std::string> produced;
  {
    // The docker socket went away: the set is removed rather than left to
    // freeze at its last value.
    nscapi::facts::response response;
    response.remove("docker");
    response.set("os").value("family", "linux");
    apply(response.serialize(), facts, errors, &produced);
  }
  EXPECT_FALSE(has_set(facts, "docker"));
  EXPECT_EQ(produced, std::set<std::string>{"os"});
}

TEST(plugin_manager_facts, a_rejected_set_is_reported_as_an_error) {
  nsclient::core::fact_repository facts;
  std::map<std::string, std::string> errors;
  // Hand-built rather than through the builder: the builder drops an invalid
  // key before it is ever sent, and what is under test here is the core
  // refusing one that reaches it anyway - a module that does not use the
  // builder, or a newer one talking to an older core.
  PB::Facts::FactsMessage message;
  PB::Facts::FactsMessage::Response* payload = message.add_payload();
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  PB::Facts::FactSet* set = payload->add_sets();
  set->set_id("os");
  PB::Facts::Field* field = set->mutable_facts()->add_fields();
  field->set_key("Family");
  field->mutable_value()->set_string_value("linux");

  EXPECT_EQ(apply(message.SerializeAsString(), facts, errors), "");
  EXPECT_FALSE(has_set(facts, "os"));
  ASSERT_EQ(errors.count("os"), 1u);
  EXPECT_NE(errors.at("os").find("Family"), std::string::npos) << errors.at("os");
}

TEST(plugin_manager_facts, a_set_that_failed_to_collect_is_still_produced) {
  nsclient::core::fact_repository facts;
  std::map<std::string, std::string> errors;
  std::set<std::string> produced;
  nscapi::facts::response response;
  response.error("software.installed", "access denied");
  EXPECT_EQ(apply(response.serialize(), facts, errors, &produced), "");
  EXPECT_EQ(errors.at("software.installed"), "access denied");
  EXPECT_EQ(produced, std::set<std::string>{"software.installed"}) << "a set that is enabled but failing must not be pruned as if it had been switched off";
}

TEST(plugin_manager_facts, a_set_that_failed_to_collect_keeps_the_value_it_had) {
  nsclient::core::fact_repository facts;
  std::map<std::string, std::string> errors;
  {
    nscapi::facts::response response;
    response.set("software").list("installed").record("nscp").value("version", "0.12.5");
    apply(response.serialize(), facts, errors);
  }
  const unsigned long long revision = facts.get_revision();
  {
    // The next round cannot read the hive. The set is mentioned, with a
    // reason and no document, so the core keeps what it has.
    nscapi::facts::response response;
    response.error("software", "access denied");
    EXPECT_EQ(apply(response.serialize(), facts, errors), "");
  }
  EXPECT_EQ(json_of(facts), R"({"software":{"installed":[{"id":"nscp","version":"0.12.5"}]}})");
  EXPECT_EQ(facts.get_revision(), revision) << "a failed collection is not a change to the inventory";
}

TEST(plugin_manager_facts, a_module_level_error_fails_the_round) {
  nsclient::core::fact_repository facts;
  std::map<std::string, std::string> errors;
  std::set<std::string> produced;
  nscapi::facts::response response;
  response.failed("Failed to collect facts: the collector threw");
  const std::string failure = apply(response.serialize(), facts, errors, &produced);
  EXPECT_NE(failure, "");
  EXPECT_NE(failure.find("the collector threw"), std::string::npos) << failure;
  EXPECT_TRUE(produced.empty()) << "a failed round must prune nothing";
}

TEST(plugin_manager_facts, an_unreadable_response_says_why_and_changes_nothing) {
  nsclient::core::fact_repository facts;
  std::map<std::string, std::string> errors;
  EXPECT_NE(apply(std::string("\xff\xff\xff\xff not a facts message", 24), facts, errors), "");
  EXPECT_NE(apply(PB::Facts::FactsMessage().SerializeAsString(), facts, errors), "") << "a message with no payload says nothing about what is produced";
  EXPECT_NE(apply("", facts, errors), "") << "an empty buffer is a failed round, not an empty inventory";
  EXPECT_EQ(json_of(facts), "{}");
}
