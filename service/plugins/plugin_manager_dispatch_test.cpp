// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The plugin manager's dispatch entry points - send_notification (a submission
// to one or more channels), simple_query and simple_exec (what `nscp client`
// and the script modules call) and the metrics, diagnostics and facts rounds -
// against mock plugins put straight into the registries the way a module's own
// registration would put them there.

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <list>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_submit.hpp>
#include <nsclient/logger/logger.hpp>
#include <string>
#include <vector>

#include "../path_manager.hpp"
#include "plugin_manager.hpp"

namespace {

class dispatch_logger : public nsclient::logging::logger {
 public:
  std::vector<std::string> errors;
  std::vector<std::string> infos;

  void trace(const std::string &, const char *, int, const std::string &) override {}
  void debug(const std::string &, const char *, int, const std::string &) override {}
  void info(const std::string &, const char *, int, const std::string &message) override { infos.push_back(message); }
  void warning(const std::string &, const char *, int, const std::string &) override {}
  void error(const std::string &, const char *, int, const std::string &message) override { errors.push_back(message); }
  void critical(const std::string &, const char *, int, const std::string &message) override { errors.push_back(message); }
  bool should_trace() const override { return false; }
  bool should_debug() const override { return false; }
  bool should_info() const override { return true; }
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

  bool logged(const std::vector<std::string> &lines, const std::string &needle) const {
    for (const auto &l : lines)
      if (l.find(needle) != std::string::npos) return true;
    return false;
  }
};

// A module that answers queries with a fixed line and submissions with an OK
// (or throws, when told to).
class dispatch_plugin : public nsclient::core::plugin_interface {
 public:
  dispatch_plugin(unsigned int id, const std::string &module) : plugin_interface(id, ""), module_(module) {}

  std::string query_message = "all good";
  PB::Common::ResultCode query_result = PB::Common::ResultCode::OK;
  bool query_garbage = false;
  bool throw_on_notification = false;
  std::vector<std::string> notifications;
  std::vector<std::string> query_requests;

  bool load_plugin(NSCAPI::moduleLoadMode) override { return true; }
  bool has_start() override { return false; }
  bool start_plugin() override { return true; }
  bool has_prepare_shutdown() override { return false; }
  void prepare_shutdown_plugin() override {}
  void unload_plugin() override {}
  std::string getName() override { return module_; }
  std::string getDescription() override { return module_; }
  std::string get_version() override { return "1.0.0"; }
  bool hasCommandHandler() override { return true; }
  NSCAPI::nagiosReturn handleCommand(std::string request, std::string &reply) override {
    query_requests.push_back(request);
    if (query_garbage) {
      reply = "not a protobuf message \xff\xff";
      return NSCAPI::cmd_return_codes::isSuccess;
    }
    PB::Commands::QueryResponseMessage resp;
    auto *p = resp.add_payload();
    p->set_command("check");
    p->set_result(query_result);
    p->add_lines()->set_message(query_message);
    reply = resp.SerializeAsString();
    return NSCAPI::cmd_return_codes::isSuccess;
  }
  bool hasNotificationHandler() override { return true; }
  NSCAPI::nagiosReturn handleNotification(const char *channel, std::string &, std::string &reply) override {
    if (throw_on_notification) throw std::runtime_error("boom");
    notifications.emplace_back(channel);
    nscapi::protobuf::functions::create_simple_submit_response_ok(channel, "cmd", module_ + " took it", reply);
    return NSCAPI::api_return_codes::isSuccess;
  }
  NSCAPI::nagiosReturn handle_schedule(const std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMessageHandler() override { return false; }
  void handleMessage(const char *, unsigned int) override {}
  bool has_on_event() override { return false; }
  NSCAPI::nagiosReturn on_event(const std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMetricsFetcher() override { return false; }
  bool hasFactsFetcher() override { return false; }
  NSCAPI::nagiosReturn fetchFacts(const std::string &, std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  NSCAPI::nagiosReturn fetchMetrics(std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
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

class DispatchTest : public ::testing::Test {
 protected:
  std::shared_ptr<dispatch_logger> logger_ = std::make_shared<dispatch_logger>();
  nsclient::core::path_instance paths_;
  nsclient::core::plugin_mgr_instance plugins_;
  std::shared_ptr<dispatch_plugin> nsca_;
  std::shared_ptr<dispatch_plugin> graphite_;

  void SetUp() override {
    paths_ = std::make_shared<nsclient::core::path_manager>(logger_);
    plugins_ = std::make_shared<nsclient::core::plugin_manager>(paths_, logger_);
    nsca_ = std::make_shared<dispatch_plugin>(1, "NSCAClient");
    graphite_ = std::make_shared<dispatch_plugin>(2, "GraphiteClient");
    for (const auto &p : {nsca_, graphite_}) {
      plugins_->get_channels()->add_plugin(p);
      plugins_->get_commands()->add_plugin(p);
    }
    plugins_->get_channels()->register_listener(1, "NSCA");
    plugins_->get_channels()->register_listener(2, "GRAPHITE");
  }

  static std::string submit_request() {
    PB::Commands::SubmitRequestMessage msg;
    msg.mutable_header()->set_source_id("test");
    auto *p = msg.add_payload();
    p->set_command("check_something");
    p->set_result(PB::Common::ResultCode::WARNING);
    p->add_lines()->set_message("disk is filling up");
    return msg.SerializeAsString();
  }

  static std::vector<std::string> messages(const std::string &response) {
    PB::Commands::SubmitResponseMessage msg;
    EXPECT_TRUE(msg.ParseFromString(response));
    std::vector<std::string> out;
    for (const auto &p : msg.payload()) out.push_back(p.result().message());
    return out;
  }
};

}  // namespace

// ===== send_notification ====================================================

TEST_F(DispatchTest, a_submission_reaches_the_channel_listener) {
  std::string request = submit_request();
  std::string response;
  EXPECT_EQ(NSCAPI::api_return_codes::isSuccess, plugins_->send_notification("NSCA", request, response));

  EXPECT_EQ(std::vector<std::string>{"NSCA"}, nsca_->notifications);
  EXPECT_TRUE(graphite_->notifications.empty());
  EXPECT_EQ(std::vector<std::string>{"NSCAClient took it"}, messages(response));
}

TEST_F(DispatchTest, a_submission_to_several_channels_keeps_every_reply) {
  // One reply per handler, merged: a failure on one channel must not be
  // hidden by a success on the next.
  std::string request = submit_request();
  std::string response;
  EXPECT_EQ(NSCAPI::api_return_codes::isSuccess, plugins_->send_notification("NSCA,GRAPHITE", request, response));

  EXPECT_EQ(std::vector<std::string>{"NSCA"}, nsca_->notifications);
  EXPECT_EQ(std::vector<std::string>{"GRAPHITE"}, graphite_->notifications);
  EXPECT_EQ((std::vector<std::string>{"NSCAClient took it", "GraphiteClient took it"}), messages(response));
}

TEST_F(DispatchTest, two_listeners_on_one_channel_both_answer) {
  plugins_->get_channels()->register_listener(2, "NSCA");
  std::string request = submit_request();
  std::string response;
  EXPECT_EQ(NSCAPI::api_return_codes::isSuccess, plugins_->send_notification("NSCA", request, response));

  EXPECT_EQ(2u, messages(response).size());
}

TEST_F(DispatchTest, the_noop_channel_accepts_anything) {
  std::string request = submit_request();
  std::string response;
  EXPECT_EQ(NSCAPI::api_return_codes::isSuccess, plugins_->send_notification("noop", request, response));
  std::string message;
  EXPECT_TRUE(nscapi::protobuf::functions::parse_simple_submit_response(response, message));
  EXPECT_TRUE(nsca_->notifications.empty());
}

TEST_F(DispatchTest, the_log_channel_writes_each_result_to_the_log) {
  std::string request = submit_request();
  std::string response;
  EXPECT_EQ(NSCAPI::api_return_codes::isSuccess, plugins_->send_notification("log", request, response));
  EXPECT_TRUE(logger_->logged(logger_->infos, "disk is filling up"));
}

TEST_F(DispatchTest, a_channel_nobody_listens_to_fails) {
  std::string request = submit_request();
  std::string response;
  EXPECT_EQ(NSCAPI::api_return_codes::hasFailed, plugins_->send_notification("NOBODY", request, response));
  EXPECT_TRUE(logger_->logged(logger_->errors, "No handler for channel: NOBODY"));
}

TEST_F(DispatchTest, a_listener_that_throws_is_logged_and_the_others_still_answer) {
  plugins_->get_channels()->register_listener(2, "NSCA");
  nsca_->throw_on_notification = true;
  std::string request = submit_request();
  std::string response;
  EXPECT_EQ(NSCAPI::api_return_codes::isSuccess, plugins_->send_notification("NSCA", request, response));

  EXPECT_TRUE(logger_->logged(logger_->errors, "Plugin throw exception: NSCAClient"));
  EXPECT_EQ(std::vector<std::string>{"GraphiteClient took it"}, messages(response));
}

// ===== simple_query =========================================================

TEST_F(DispatchTest, a_query_runs_the_registered_command) {
  plugins_->get_commands()->register_command(1, "check_thing", "a check");
  graphite_->query_message = "WARNING: almost full";
  graphite_->query_result = PB::Common::ResultCode::WARNING;
  plugins_->get_commands()->register_command(2, "check_other", "another check");

  std::list<std::string> resp;
  EXPECT_EQ(NSCAPI::query_return_codes::returnOK, plugins_->simple_query("", "check_thing", {"a=1"}, resp));
  EXPECT_EQ(std::list<std::string>{"all good"}, resp);
  ASSERT_EQ(1u, nsca_->query_requests.size());
  PB::Commands::QueryRequestMessage sent;
  ASSERT_TRUE(sent.ParseFromString(nsca_->query_requests.front()));
  ASSERT_EQ(1, sent.payload_size());
  EXPECT_EQ("check_thing", sent.payload(0).command());
  ASSERT_EQ(1, sent.payload(0).arguments_size());
  EXPECT_EQ("a=1", sent.payload(0).arguments(0));

  resp.clear();
  EXPECT_EQ(NSCAPI::query_return_codes::returnWARN, plugins_->simple_query("", "check_other", {}, resp));
  EXPECT_EQ(std::list<std::string>{"WARNING: almost full"}, resp);
}

TEST_F(DispatchTest, a_query_for_an_unknown_command_is_unknown) {
  std::list<std::string> resp;
  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, plugins_->simple_query("", "check_nothing", {}, resp));
  ASSERT_EQ(1u, resp.size());
  EXPECT_NE(std::string::npos, resp.front().find("check_nothing"));
}

TEST_F(DispatchTest, a_query_naming_a_module_that_cannot_load_still_runs_and_reports_it) {
  // The module name only gives it a chance to be loaded on demand; the
  // command itself is found through the registry.
  plugins_->get_commands()->register_command(1, "check_thing", "a check");
  std::list<std::string> resp;
  plugins_->simple_query("NoSuchModule", "check_thing", {}, resp);
  ASSERT_GE(resp.size(), 2u);
  EXPECT_EQ("all good", resp.front());
  EXPECT_NE(std::string::npos, resp.back().find("NoSuchModule"));
}

TEST_F(DispatchTest, an_unreadable_query_answer_is_unknown) {
  plugins_->get_commands()->register_command(1, "check_thing", "a check");
  nsca_->query_garbage = true;
  std::list<std::string> resp;
  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, plugins_->simple_query("", "check_thing", {}, resp));
}

// ===== simple_exec ==========================================================

TEST_F(DispatchTest, an_exec_without_a_module_says_so) {
  std::list<std::string> resp;
  EXPECT_EQ(1, plugins_->simple_exec("command-without-module", {}, resp));
  EXPECT_EQ(std::list<std::string>{"No module was specified..."}, resp);
}

TEST_F(DispatchTest, an_exec_in_a_module_that_cannot_load_says_so) {
  std::list<std::string> resp;
  EXPECT_EQ(1, plugins_->simple_exec("NoSuchModule.run", {"x"}, resp));
  ASSERT_EQ(1u, resp.size());
  EXPECT_NE(std::string::npos, resp.front().find("NoSuchModule"));
}

TEST_F(DispatchTest, exec_command_with_no_module_able_to_run_it_is_ignored) {
  // No module in the plugin list takes command-line execution.
  std::string response;
  EXPECT_EQ(NSCAPI::cmd_return_codes::returnIgnored, plugins_->exec_command("any", "", response));
  PB::Commands::ExecuteResponseMessage msg;
  ASSERT_TRUE(msg.ParseFromString(response));
  EXPECT_EQ(0, msg.payload_size());
}

// ===== rounds with nothing to collect from ==================================

TEST_F(DispatchTest, a_metrics_round_without_fetchers_carries_the_core_bundle) {
  PB::Metrics::MetricsBundle core;
  core.set_key("core");
  const auto message = plugins_->process_metrics(core);
  ASSERT_EQ(1, message.payload_size());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, message.payload(0).result().code());
  ASSERT_EQ(1, message.payload(0).bundles_size());
  EXPECT_EQ("core", message.payload(0).bundles(0).key());
}

TEST_F(DispatchTest, a_diagnostics_round_without_producers_is_empty) {
  EXPECT_FALSE(plugins_->has_diagnostics_fetchers());
  EXPECT_EQ(0, plugins_->collect_diagnostics().sections_size());
}

TEST_F(DispatchTest, a_facts_round_without_a_repository_does_nothing) {
  EXPECT_FALSE(plugins_->has_facts_fetchers());
  EXPECT_NO_THROW(plugins_->process_facts("manual"));
}

TEST_F(DispatchTest, lifecycle_calls_for_a_module_that_is_not_loaded_fail) {
  EXPECT_EQ(-1, plugins_->clone_plugin(42));
  EXPECT_FALSE(plugins_->reload_plugin("NoSuchModule"));
  EXPECT_FALSE(plugins_->remove_plugin("NoSuchModule"));
  EXPECT_FALSE(plugins_->is_dispatching_on_this_thread());
  EXPECT_FALSE(plugins_->is_dispatching_on_this_thread("NoSuchModule"));
}
