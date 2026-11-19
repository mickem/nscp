// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The core's key/value store (storage_manager) and the request handler plugins
// reach it through (storage_query_handler). The store is what keeps module
// state - the WEB server's session table among it - across restarts, so the
// on-disk round trip is the part worth pinning down.

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <map>
#include <memory>
#include <nsclient/logger/logger.hpp>
#include <string>
#include <vector>

#include "path_manager.hpp"
#include "plugins/plugin_manager.hpp"
#include "plugins/zip_test_writer.hpp"
#include "storage_manager.hpp"
#include "storage_query_handler.hpp"

namespace {

// Records error lines so a test can tell a quiet success from a logged failure.
class recording_logger : public nsclient::logging::logger {
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
};

PB::Storage::Storage::Entry make_entry(const std::string &context, const std::string &key, const std::string &value) {
  PB::Storage::Storage::Entry e;
  e.set_context(context);
  e.set_key(key);
  e.set_value(value);
  return e;
}

std::map<std::string, std::string> as_map(const nsclient::core::storage_manager::entry_list &list) {
  std::map<std::string, std::string> ret;
  for (const auto &e : list) ret[e.key()] = e.value();
  return ret;
}

class StorageTest : public ::testing::Test {
 protected:
  boost::filesystem::path dir_;
  std::shared_ptr<recording_logger> logger_;
  nsclient::core::path_instance path_;

  void SetUp() override {
    dir_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-storage-test-%%%%-%%%%-%%%%");
    boost::filesystem::create_directories(dir_);
    logger_ = std::make_shared<recording_logger>();
    path_ = std::make_shared<nsclient::core::path_manager>(logger_);
    path_->set_overrides({{"data-path", dir_.string()}});
  }

  void TearDown() override {
    boost::system::error_code ec;
    boost::filesystem::remove_all(dir_, ec);
  }

  std::shared_ptr<nsclient::core::storage_manager> make_store() { return std::make_shared<nsclient::core::storage_manager>(path_, logger_); }
  boost::filesystem::path db_file() const { return dir_ / "nsclient.db"; }
  boost::filesystem::path tmp_file() const { return dir_ / "nsclient.tmp"; }
};

}  // namespace

// ===== storage_manager ======================================================

TEST_F(StorageTest, get_on_an_empty_store_returns_nothing) {
  auto store = make_store();
  EXPECT_TRUE(store->get("plugin", "ctx").empty());
}

TEST_F(StorageTest, put_then_get_returns_the_entry) {
  auto store = make_store();
  store->put("plugin", make_entry("ctx", "key", "value"));

  const auto entries = store->get("plugin", "ctx");
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("ctx", entries.front().context());
  EXPECT_EQ("key", entries.front().key());
  EXPECT_EQ("value", entries.front().value());
}

TEST_F(StorageTest, put_with_the_same_key_replaces_the_value) {
  auto store = make_store();
  store->put("plugin", make_entry("ctx", "key", "first"));
  store->put("plugin", make_entry("ctx", "key", "second"));

  const auto entries = store->get("plugin", "ctx");
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("second", entries.front().value());
}

TEST_F(StorageTest, get_returns_every_key_in_the_context) {
  auto store = make_store();
  store->put("plugin", make_entry("ctx", "a", "1"));
  store->put("plugin", make_entry("ctx", "b", "2"));
  store->put("plugin", make_entry("ctx", "c", "3"));

  const std::map<std::string, std::string> expected{{"a", "1"}, {"b", "2"}, {"c", "3"}};
  EXPECT_EQ(expected, as_map(store->get("plugin", "ctx")));
}

TEST_F(StorageTest, get_is_scoped_to_the_owner_and_the_context) {
  auto store = make_store();
  store->put("plugin", make_entry("ctx", "mine", "1"));
  store->put("plugin", make_entry("other", "elsewhere", "2"));
  store->put("another", make_entry("ctx", "theirs", "3"));

  const std::map<std::string, std::string> expected{{"mine", "1"}};
  EXPECT_EQ(expected, as_map(store->get("plugin", "ctx")));
  EXPECT_TRUE(store->get("nobody", "ctx").empty());
}

TEST_F(StorageTest, save_without_a_change_writes_no_file) {
  auto store = make_store();
  store->save();
  EXPECT_FALSE(boost::filesystem::exists(db_file()));
  EXPECT_TRUE(logger_->errors.empty());
}

TEST_F(StorageTest, save_and_load_round_trip_every_field) {
  {
    auto store = make_store();
    auto entry = make_entry("ctx", "key", std::string("bin\0ary", 7));
    entry.set_binary_data(true);
    entry.set_private_data(true);
    store->put("plugin", entry);
    store->put("plugin", make_entry("ctx", "other", "value"));
    store->put("another", make_entry("ctx2", "k", "v"));
    store->save();
  }
  ASSERT_TRUE(boost::filesystem::exists(db_file()));
  // Written to the temporary name and renamed into place, so nothing is left behind.
  EXPECT_FALSE(boost::filesystem::exists(tmp_file()));

  auto loaded = make_store();
  loaded->load();

  const auto entries = loaded->get("plugin", "ctx");
  ASSERT_EQ(2u, entries.size());
  const auto &first = entries.front();  // std::map order: "key" before "other"
  EXPECT_EQ("key", first.key());
  EXPECT_EQ(std::string("bin\0ary", 7), first.value());
  EXPECT_TRUE(first.binary_data());
  EXPECT_TRUE(first.private_data());
  EXPECT_EQ("other", entries.back().key());
  EXPECT_FALSE(entries.back().binary_data());

  const std::map<std::string, std::string> expected{{"k", "v"}};
  EXPECT_EQ(expected, as_map(loaded->get("another", "ctx2")));
  EXPECT_TRUE(logger_->errors.empty());
}

TEST_F(StorageTest, load_of_a_missing_file_yields_an_unmodified_store) {
  // A fresh install has no nsclient.db: load() must leave the store empty and
  // unmodified, so the shutdown save does not write an empty file.
  auto store = make_store();
  store->load();
  store->save();
  EXPECT_FALSE(boost::filesystem::exists(db_file()));
}

TEST_F(StorageTest, load_of_a_missing_file_yields_an_empty_store) {
  auto store = make_store();
  store->load();
  EXPECT_TRUE(store->get("plugin", "ctx").empty());
}

TEST_F(StorageTest, save_replaces_the_previous_file) {
  {
    auto store = make_store();
    store->put("plugin", make_entry("ctx", "key", "old"));
    store->save();
  }
  {
    auto store = make_store();
    store->load();
    store->put("plugin", make_entry("ctx", "key", "new"));
    store->put("plugin", make_entry("ctx", "added", "x"));
    store->save();
  }
  auto loaded = make_store();
  loaded->load();
  const std::map<std::string, std::string> expected{{"added", "x"}, {"key", "new"}};
  EXPECT_EQ(expected, as_map(loaded->get("plugin", "ctx")));
}

TEST_F(StorageTest, save_creates_a_missing_data_directory) {
  const auto nested = dir_ / "not" / "there" / "yet";
  path_->set_overrides({{"data-path", nested.string()}});
  auto store = make_store();
  store->put("plugin", make_entry("ctx", "key", "value"));
  store->save();
  EXPECT_TRUE(boost::filesystem::exists(nested / "nsclient.db"));
  EXPECT_TRUE(logger_->errors.empty());
}

TEST_F(StorageTest, save_into_an_unwritable_location_is_logged_not_thrown) {
  // The data path is a regular file, so the directory under it cannot be
  // created: save() must report that rather than let it escape.
  const auto blocker = dir_ / "blocker";
  { std::ofstream(blocker.string()) << "x"; }
  path_->set_overrides({{"data-path", (blocker / "sub").string()}});
  auto store = make_store();
  store->put("plugin", make_entry("ctx", "key", "value"));
  EXPECT_NO_THROW(store->save());
  EXPECT_FALSE(logger_->errors.empty());
}

// ===== storage_query_handler ================================================
//
// Most of these load no module, so every request resolves to the unnamed
// owner: what they check is the dispatch. The last ones load a zip bundle so a
// request can name a real plugin.

namespace {
PB::Storage::StorageRequestMessage::Request *add_put(PB::Storage::StorageRequestMessage &msg, const std::string &context, const std::string &key,
                                                     const std::string &value, bool flush = false) {
  auto *r = msg.add_payload();
  r->mutable_put()->mutable_entry()->CopyFrom(make_entry(context, key, value));
  r->mutable_put()->set_flush(flush);
  return r;
}
PB::Storage::StorageRequestMessage::Request *add_get(PB::Storage::StorageRequestMessage &msg, const std::string &context) {
  auto *r = msg.add_payload();
  r->mutable_get()->set_context(context);
  return r;
}
}  // namespace

class StorageQueryHandlerTest : public StorageTest {
 protected:
  nsclient::core::plugin_mgr_instance plugins_;
  std::shared_ptr<nsclient::core::storage_manager> store_;

  void SetUp() override {
    StorageTest::SetUp();
    plugins_ = std::make_shared<nsclient::core::plugin_manager>(path_, logger_);
    store_ = make_store();
  }

  PB::Storage::StorageResponseMessage run(const PB::Storage::StorageRequestMessage &request) {
    PB::Storage::StorageResponseMessage response;
    nsclient::core::storage_query_handler handler(store_, plugins_, logger_, request);
    handler.parse(response);
    return response;
  }
};

TEST_F(StorageQueryHandlerTest, put_stores_the_entry_and_answers_nothing) {
  PB::Storage::StorageRequestMessage request;
  add_put(request, "ctx", "key", "value");
  const auto response = run(request);

  EXPECT_EQ(0, response.payload_size());
  const std::map<std::string, std::string> expected{{"key", "value"}};
  EXPECT_EQ(expected, as_map(store_->get("", "ctx")));
}

TEST_F(StorageQueryHandlerTest, put_without_flush_does_not_write_to_disk) {
  PB::Storage::StorageRequestMessage request;
  add_put(request, "ctx", "key", "value", false);
  run(request);
  EXPECT_FALSE(boost::filesystem::exists(db_file()));
}

TEST_F(StorageQueryHandlerTest, put_with_flush_writes_the_store_to_disk) {
  PB::Storage::StorageRequestMessage request;
  add_put(request, "ctx", "key", "value", true);
  run(request);
  ASSERT_TRUE(boost::filesystem::exists(db_file()));

  auto loaded = make_store();
  loaded->load();
  const std::map<std::string, std::string> expected{{"key", "value"}};
  EXPECT_EQ(expected, as_map(loaded->get("", "ctx")));
}

TEST_F(StorageQueryHandlerTest, get_answers_one_payload_with_every_entry_in_the_context) {
  store_->put("", make_entry("ctx", "a", "1"));
  store_->put("", make_entry("ctx", "b", "2"));
  store_->put("", make_entry("other", "c", "3"));

  PB::Storage::StorageRequestMessage request;
  add_get(request, "ctx");
  const auto response = run(request);

  ASSERT_EQ(1, response.payload_size());
  std::map<std::string, std::string> got;
  for (const auto &e : response.payload(0).get().entry()) got[e.key()] = e.value();
  const std::map<std::string, std::string> expected{{"a", "1"}, {"b", "2"}};
  EXPECT_EQ(expected, got);
}

TEST_F(StorageQueryHandlerTest, get_of_an_unknown_context_answers_an_empty_payload) {
  PB::Storage::StorageRequestMessage request;
  add_get(request, "nothing-here");
  const auto response = run(request);

  ASSERT_EQ(1, response.payload_size());
  EXPECT_EQ(0, response.payload(0).get().entry_size());
}

TEST_F(StorageQueryHandlerTest, requests_in_one_message_run_in_order) {
  PB::Storage::StorageRequestMessage request;
  add_get(request, "ctx");
  add_put(request, "ctx", "key", "value");
  add_get(request, "ctx");
  const auto response = run(request);

  // One payload per get; the put in between is seen by the second only.
  ASSERT_EQ(2, response.payload_size());
  EXPECT_EQ(0, response.payload(0).get().entry_size());
  ASSERT_EQ(1, response.payload(1).get().entry_size());
  EXPECT_EQ("value", response.payload(1).get().entry(0).value());
}

TEST_F(StorageQueryHandlerTest, a_request_with_neither_get_nor_put_is_logged_and_skipped) {
  PB::Storage::StorageRequestMessage request;
  request.add_payload();  // no action set
  add_get(request, "ctx");
  const auto response = run(request);

  EXPECT_EQ(1, response.payload_size());
  ASSERT_EQ(1u, logger_->errors.size());
  EXPECT_NE(std::string::npos, logger_->errors.front().find("Unsupported action"));
}

namespace {
// Load a zip bundle so there is a real plugin for a request to name, and
// return its id. Both `id` and `plugin_id` are set on requests built from it:
// the handler currently reads the former, the module-side helper fills in the
// latter, and the test should not care which one wins.
unsigned int load_bundle(const nsclient::core::plugin_mgr_instance &plugins, const boost::filesystem::path &dir, const std::string &name) {
  const boost::filesystem::path modules = dir / "modules";
  boost::filesystem::create_directories(modules);
  zip_test::write_zip(modules / (name + ".zip"), {{"module.json", R"({"name":"x","description":""})"}});
  plugins->set_path(modules);
  if (!plugins->load_single_plugin(name, "", false)) return 0;
  for (unsigned int id = 0; id < 16; id++) {
    const auto p = plugins->find_plugin(id);
    if (p && p->getModule() == name) return id;
  }
  return 0;
}
}  // namespace

TEST_F(StorageQueryHandlerTest, a_request_from_a_loaded_module_is_stored_under_its_name) {
  const unsigned int id = load_bundle(plugins_, dir_, "bundle");
  ASSERT_NE(nullptr, plugins_->find_plugin(id));

  PB::Storage::StorageRequestMessage put;
  auto *r = add_put(put, "ctx", "key", "value");
  r->set_id(id);
  r->set_plugin_id(static_cast<int>(id));
  run(put);

  const std::map<std::string, std::string> expected{{"key", "value"}};
  EXPECT_EQ(expected, as_map(store_->get("bundle", "ctx")));
  EXPECT_TRUE(store_->get("", "ctx").empty());

  PB::Storage::StorageRequestMessage get;
  auto *g = add_get(get, "ctx");
  g->set_id(id);
  g->set_plugin_id(static_cast<int>(id));
  const auto response = run(get);
  ASSERT_EQ(1, response.payload_size());
  ASSERT_EQ(1, response.payload(0).get().entry_size());
  EXPECT_EQ("value", response.payload(0).get().entry(0).value());
}
