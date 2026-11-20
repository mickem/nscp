// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The core's key/value store (storage_manager) and the request handler plugins
// reach it through (storage_query_handler). The store is what keeps module
// state - the WEB server's session table among it - across restarts, so the
// on-disk round trip is the part worth pinning down.
//
// Stored entries are owned by the module that wrote them: the handler resolves
// the request's plugin_id to the plugin's alias or module name and keys the
// entry by it. It used to read the request's `id` instead, which nobody sets,
// so every module resolved to the plugin with id 0 and they all shared one
// namespace - the WEB server's session table could be overwritten by any other
// module storing "web.sessions"/"sessions". A file written by such an agent
// (version < 2) is loaded under a legacy owner, and each module claims its own
// contexts from it on its first read.

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <map>
#include <memory>
#include <nscapi/protobuf/io.hpp>
#include <nscapi/protobuf/storage.hpp>
#include <nsclient/logger/logger.hpp>
#include <settings/test_helpers.hpp>
#include <string>
#include <utility>
#include <vector>

#include "path_manager.hpp"
#include "plugins/plugin_interface.hpp"
#include "plugins/plugin_manager.hpp"
#include "plugins/zip_test_writer.hpp"
#include "storage_manager.hpp"
#include "storage_query_handler.hpp"

namespace {

// A loaded module, as far as storage can tell: an id, a module name and an
// optional alias. Nothing else is ever called on it.
class storage_test_plugin : public nsclient::core::plugin_interface {
 public:
  storage_test_plugin(unsigned int id, std::string module, std::string alias = "") : plugin_interface(id, std::move(alias)), module_(std::move(module)) {}

  bool load_plugin(NSCAPI::moduleLoadMode) override { return true; }
  bool has_start() override { return false; }
  bool start_plugin() override { return true; }
  bool has_prepare_shutdown() override { return false; }
  void prepare_shutdown_plugin() override {}
  void unload_plugin() override {}
  std::string getName() override { return module_; }
  std::string getDescription() override { return module_; }
  std::string get_version() override { return "1.0.0"; }
  bool hasCommandHandler() override { return false; }
  NSCAPI::nagiosReturn handleCommand(std::string, std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasNotificationHandler() override { return false; }
  NSCAPI::nagiosReturn handleNotification(const char *, std::string &, std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  NSCAPI::nagiosReturn handle_schedule(const std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMessageHandler() override { return false; }
  void handleMessage(const char *, unsigned int) override {}
  bool has_on_event() override { return false; }
  NSCAPI::nagiosReturn on_event(const std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMetricsFetcher() override { return false; }
  NSCAPI::nagiosReturn fetchMetrics(std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasFactsFetcher() override { return false; }
  NSCAPI::nagiosReturn fetchFacts(const std::string &, std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMetricsSubmitter() override { return false; }
  NSCAPI::nagiosReturn submitMetrics(const std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool has_command_line_exec() override { return false; }
  int commandLineExec(bool, std::string &, std::string &) override { return 0; }
  bool has_routing_handler() override { return false; }
  bool route_message(const char *, const char *, unsigned int, char **, char **, unsigned int *) override { return false; }
  bool is_duplicate(boost::filesystem::path, std::string) override { return false; }
  std::string getModule() override { return module_; }
  void on_log_message(const std::string &) override {}

 private:
  std::string module_;
};

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
// Most of these come from a mock plugin put straight into the plugin list
// (module "plugin", id kOwnerId): what they check is the dispatch. The last
// ones load a zip bundle so a request can name a plugin loaded the real way.

namespace {
// Well clear of the ids the plugin list hands out (from 0), so a bundle loaded
// by the same test cannot collide with it.
constexpr int kOwnerId = 100;

PB::Storage::StorageRequestMessage::Request *add_put(PB::Storage::StorageRequestMessage &msg, const std::string &context, const std::string &key,
                                                     const std::string &value, bool flush = false) {
  auto *r = msg.add_payload();
  r->set_plugin_id(kOwnerId);
  r->mutable_put()->mutable_entry()->CopyFrom(make_entry(context, key, value));
  r->mutable_put()->set_flush(flush);
  return r;
}
PB::Storage::StorageRequestMessage::Request *add_get(PB::Storage::StorageRequestMessage &msg, const std::string &context) {
  auto *r = msg.add_payload();
  r->set_plugin_id(kOwnerId);
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
    plugins_->get_plugin_list()->append_plugin(std::make_shared<storage_test_plugin>(kOwnerId, "plugin"));
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
  EXPECT_EQ(expected, as_map(store_->get("plugin", "ctx")));
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
  EXPECT_EQ(expected, as_map(loaded->get("plugin", "ctx")));
}

TEST_F(StorageQueryHandlerTest, get_answers_one_payload_with_every_entry_in_the_context) {
  store_->put("plugin", make_entry("ctx", "a", "1"));
  store_->put("plugin", make_entry("ctx", "b", "2"));
  store_->put("plugin", make_entry("other", "c", "3"));

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
// return its id. The handler reads `plugin_id`, which the module-side helper
// fills in on every get and put.
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
  r->set_plugin_id(static_cast<int>(id));
  run(put);

  const std::map<std::string, std::string> expected{{"key", "value"}};
  EXPECT_EQ(expected, as_map(store_->get("bundle", "ctx")));
  EXPECT_TRUE(store_->get("plugin", "ctx").empty());

  PB::Storage::StorageRequestMessage get;
  auto *g = add_get(get, "ctx");
  g->set_plugin_id(static_cast<int>(id));
  const auto response = run(get);
  ASSERT_EQ(1, response.payload_size());
  ASSERT_EQ(1, response.payload(0).get().entry_size());
  EXPECT_EQ("value", response.payload(0).get().entry(0).value());
}

// ===== ownership and the legacy layout ======================================

namespace {

template <typename T>
void write_chunk(::google::protobuf::io::CodedOutputStream &stream, const T &obj) {
  const std::string tmp = obj.SerializeAsString();
  stream.WriteVarint32(static_cast<uint32_t>(tmp.size()));
  stream.WriteString(tmp);
}

template <typename T>
bool read_chunk(::google::protobuf::io::CodedInputStream &stream, T &obj) {
  uint32_t size = 0;
  if (!stream.ReadVarint32(&size)) return false;
  std::string tmp;
  if (!stream.ReadString(&tmp, size)) return false;
  return obj.ParseFromString(tmp);
}

struct stored_block {
  std::string owner;
  std::string context;
  std::string key;
  std::string value;
};

class StorageOwnerTest : public ::testing::Test {
 protected:
  settings_test::temp_dir dir_;
  nsclient::logging::logger_instance logger_ = settings_test::make_null_logger();
  nsclient::core::path_instance paths_;
  nsclient::core::plugin_mgr_instance plugins_;
  nsclient::core::storage_manager_instance storage_;

  // Ids as the real plugin list hands them out: the first module loaded gets
  // 0, which is exactly the id an unset `id` field resolved to.
  static constexpr int kSystem = 0;     // CheckSystem
  static constexpr int kWeb = 1;        // WEBServer
  static constexpr int kLogA = 2;       // CheckLogFile, alias "logs_a"
  static constexpr int kLogB = 3;       // CheckLogFile, alias "logs_b"
  static constexpr int kUnknown = 42;   // not loaded

  void SetUp() override {
    paths_ = std::make_shared<nsclient::core::path_manager>(logger_);
    paths_->set_overrides({{"data-path", dir_.string()}});
    plugins_ = std::make_shared<nsclient::core::plugin_manager>(paths_, logger_);
    plugins_->get_plugin_list()->append_plugin(std::make_shared<storage_test_plugin>(kSystem, "CheckSystem"));
    plugins_->get_plugin_list()->append_plugin(std::make_shared<storage_test_plugin>(kWeb, "WEBServer"));
    plugins_->get_plugin_list()->append_plugin(std::make_shared<storage_test_plugin>(kLogA, "CheckLogFile", "logs_a"));
    plugins_->get_plugin_list()->append_plugin(std::make_shared<storage_test_plugin>(kLogB, "CheckLogFile", "logs_b"));
    storage_ = std::make_shared<nsclient::core::storage_manager>(paths_, logger_);
  }

  void TearDown() override {
    storage_.reset();
    plugins_.reset();
  }

  boost::filesystem::path db() const { return dir_.file("nsclient.db"); }

  PB::Storage::StorageResponseMessage run(const PB::Storage::StorageRequestMessage &request) {
    PB::Storage::StorageResponseMessage response;
    nsclient::core::storage_query_handler(storage_, plugins_, logger_, request).parse(response);
    return response;
  }

  // What nscapi::core_helper::put_storage sends.
  PB::Storage::StorageResponseMessage put(int plugin_id, const std::string &context, const std::string &key, const std::string &value, bool flush = false) {
    PB::Storage::StorageRequestMessage request;
    auto *payload = request.add_payload();
    payload->set_plugin_id(plugin_id);
    payload->mutable_put()->mutable_entry()->set_context(context);
    payload->mutable_put()->mutable_entry()->set_key(key);
    payload->mutable_put()->mutable_entry()->set_value(value);
    payload->mutable_put()->set_flush(flush);
    return run(request);
  }

  // What nscapi::core_helper::get_storage_strings sends.
  PB::Storage::StorageResponseMessage get_raw(int plugin_id, const std::string &context) {
    PB::Storage::StorageRequestMessage request;
    auto *payload = request.add_payload();
    payload->set_plugin_id(plugin_id);
    payload->mutable_get()->set_context(context);
    return run(request);
  }

  // key -> value, the way get_storage_strings hands it to a module.
  std::map<std::string, std::string> get(int plugin_id, const std::string &context) {
    std::map<std::string, std::string> ret;
    const PB::Storage::StorageResponseMessage response = get_raw(plugin_id, context);
    EXPECT_EQ(1, response.payload_size());
    for (const auto &p : response.payload()) {
      EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, p.result().code()) << p.result().message();
      for (const auto &e : p.get().entry()) ret[e.key()] = e.value();
    }
    return ret;
  }

  // An nsclient.db as an agent without per-module owners wrote it: version 1,
  // and every entry owned by whatever plugin had id 0 at the time.
  void write_legacy_db(const std::vector<stored_block> &blocks) {
    std::ofstream out(db().string().c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    {
      ::google::protobuf::io::OstreamOutputStream raw(&out);
      ::google::protobuf::io::CodedOutputStream coded(&raw);
      ::PB::Storage::Storage::File header;
      header.set_version(1);
      header.set_entries(static_cast<long long>(blocks.size()));
      write_chunk(coded, header);
      for (const stored_block &b : blocks) {
        ::PB::Storage::Storage::Block block;
        block.set_owner(b.owner);
        block.mutable_entry()->set_context(b.context);
        block.mutable_entry()->set_key(b.key);
        block.mutable_entry()->set_value(b.value);
        write_chunk(coded, block);
      }
    }
  }

  // The file on disk, as written: its header version and its blocks.
  long long read_db(std::vector<stored_block> &blocks) {
    std::ifstream in(db().string().c_str(), std::ios::in | std::ios::binary);
    ::google::protobuf::io::IstreamInputStream raw(&in);
    ::google::protobuf::io::CodedInputStream coded(&raw);
    ::PB::Storage::Storage::File header;
    EXPECT_TRUE(read_chunk(coded, header));
    for (long long i = 0; i < header.entries(); i++) {
      ::PB::Storage::Storage::Block block;
      EXPECT_TRUE(read_chunk(coded, block));
      blocks.push_back({block.owner(), block.entry().context(), block.entry().key(), block.entry().value()});
    }
    return header.version();
  }

  void reload() {
    storage_ = std::make_shared<nsclient::core::storage_manager>(paths_, logger_);
    storage_->load();
  }
};

TEST_F(StorageOwnerTest, modules_storing_the_same_context_and_key_do_not_overwrite_each_other) {
  put(kWeb, "web.sessions", "sessions", "web-state");
  put(kSystem, "web.sessions", "sessions", "something-else");

  EXPECT_EQ((std::map<std::string, std::string>{{"sessions", "web-state"}}), get(kWeb, "web.sessions"));
  EXPECT_EQ((std::map<std::string, std::string>{{"sessions", "something-else"}}), get(kSystem, "web.sessions"));
}

TEST_F(StorageOwnerTest, the_first_plugin_loaded_is_not_everyones_owner) {
  // Plugin id 0 is a real plugin, and the id the old handler always used.
  // Writing as id 1 must not be visible to id 0.
  put(kWeb, "web.sessions", "sessions", "web-state");
  EXPECT_TRUE(get(kSystem, "web.sessions").empty());
}

TEST_F(StorageOwnerTest, two_instances_of_one_module_are_kept_apart_by_alias) {
  put(kLogA, "logfile.bookmarks", "/var/log/a", "100");
  put(kLogB, "logfile.bookmarks", "/var/log/b", "200");

  EXPECT_EQ((std::map<std::string, std::string>{{"/var/log/a", "100"}}), get(kLogA, "logfile.bookmarks"));
  EXPECT_EQ((std::map<std::string, std::string>{{"/var/log/b", "200"}}), get(kLogB, "logfile.bookmarks"));
}

TEST_F(StorageOwnerTest, entries_are_saved_under_the_owning_module) {
  put(kWeb, "web.sessions", "sessions", "web-state", true);  // flush writes it now

  std::vector<stored_block> blocks;
  EXPECT_EQ(2, read_db(blocks));
  ASSERT_EQ(1u, blocks.size());
  EXPECT_EQ("WEBServer", blocks[0].owner);
  EXPECT_EQ("web-state", blocks[0].value);

  reload();
  EXPECT_EQ((std::map<std::string, std::string>{{"sessions", "web-state"}}), get(kWeb, "web.sessions"));
  EXPECT_TRUE(get(kSystem, "web.sessions").empty());
}

TEST_F(StorageOwnerTest, an_unknown_plugin_id_is_refused) {
  const PB::Storage::StorageResponseMessage put_response = put(kUnknown, "web.sessions", "sessions", "stolen");
  ASSERT_EQ(1, put_response.payload_size());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_ERROR, put_response.payload(0).result().code());

  const PB::Storage::StorageResponseMessage get_response = get_raw(kUnknown, "web.sessions");
  ASSERT_EQ(1, get_response.payload_size());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_ERROR, get_response.payload(0).result().code());
  EXPECT_EQ(0, get_response.payload(0).get().entry_size());

  storage_->save();
  EXPECT_FALSE(boost::filesystem::exists(db()));
}

TEST_F(StorageOwnerTest, an_unknown_plugin_cannot_read_unclaimed_legacy_entries) {
  write_legacy_db({{"CheckSystem", "web.sessions", "sessions", "old-web-state"}});
  reload();
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_ERROR, get_raw(kUnknown, "web.sessions").payload(0).result().code());
  EXPECT_EQ((std::map<std::string, std::string>{{"sessions", "old-web-state"}}), get(kWeb, "web.sessions"));
}

TEST_F(StorageOwnerTest, a_legacy_file_is_claimed_by_the_first_module_to_read_each_context) {
  // As an older agent left it: everything owned by the plugin that had id 0
  // (here CheckSystem), whichever module actually wrote it.
  write_legacy_db({
      {"CheckSystem", "web.sessions", "sessions", "old-web-state"},
      {"CheckSystem", "disk.trends", "C:", "trend"},
      {"", "logfile.bookmarks", "/var/log/a", "100"},
  });
  reload();

  EXPECT_EQ((std::map<std::string, std::string>{{"sessions", "old-web-state"}}), get(kWeb, "web.sessions"));
  // Moved, not copied: another module reading the same context gets nothing.
  EXPECT_TRUE(get(kSystem, "web.sessions").empty());
  // ...and the module that claimed it still has it on the next read.
  EXPECT_EQ((std::map<std::string, std::string>{{"sessions", "old-web-state"}}), get(kWeb, "web.sessions"));
  // The owner on disk does not matter: an entry written under "" is claimed
  // the same way.
  EXPECT_EQ((std::map<std::string, std::string>{{"/var/log/a", "100"}}), get(kLogA, "logfile.bookmarks"));

  storage_->save();
  std::vector<stored_block> blocks;
  EXPECT_EQ(2, read_db(blocks));
  std::map<std::string, std::string> owners;  // context -> owner
  for (const stored_block &b : blocks) owners[b.context] = b.owner;
  EXPECT_EQ("WEBServer", owners["web.sessions"]);
  EXPECT_EQ("logs_a", owners["logfile.bookmarks"]);
  // Nobody has read disk.trends yet, so it is still waiting to be claimed,
  // and survives the save.
  EXPECT_EQ("", owners["disk.trends"]);
  EXPECT_EQ(3u, blocks.size());

  // A version 2 file is taken at its word: the owners are real, nothing is
  // re-read as legacy.
  reload();
  EXPECT_TRUE(get(kSystem, "web.sessions").empty());
  EXPECT_EQ((std::map<std::string, std::string>{{"sessions", "old-web-state"}}), get(kWeb, "web.sessions"));
  EXPECT_EQ((std::map<std::string, std::string>{{"C:", "trend"}}), get(kSystem, "disk.trends"));
}

TEST_F(StorageOwnerTest, a_module_with_state_of_its_own_does_not_take_legacy_entries) {
  write_legacy_db({{"CheckSystem", "web.sessions", "sessions", "old-web-state"}});
  reload();

  put(kWeb, "web.sessions", "sessions", "new-web-state");
  EXPECT_EQ((std::map<std::string, std::string>{{"sessions", "new-web-state"}}), get(kWeb, "web.sessions"));
}

TEST_F(StorageOwnerTest, a_legacy_file_is_rewritten_in_the_new_layout_even_before_anything_is_claimed) {
  write_legacy_db({{"CheckSystem", "web.sessions", "sessions", "old-web-state"}});
  reload();
  storage_->save();

  std::vector<stored_block> blocks;
  EXPECT_EQ(2, read_db(blocks));
  ASSERT_EQ(1u, blocks.size());
  EXPECT_EQ("", blocks[0].owner);
}

TEST_F(StorageOwnerTest, claiming_a_context_does_not_take_a_longer_context_it_is_a_prefix_of) {
  write_legacy_db({
      {"CheckSystem", "disk", "a", "1"},
      {"CheckSystem", "disk.trends", "C:", "trend"},
  });
  reload();

  EXPECT_EQ((std::map<std::string, std::string>{{"a", "1"}}), get(kWeb, "disk"));
  EXPECT_EQ((std::map<std::string, std::string>{{"C:", "trend"}}), get(kSystem, "disk.trends"));
}

}  // namespace
