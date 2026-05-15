#include "plugin_manager.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
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
