// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for service/registry_query_handler.cpp - the registry API: modules
// registering their commands, aliases, notification channels and event
// subscriptions, and `nscp test`, the REST registry endpoints and the web UI
// asking what is registered.
//
// The handler runs against a real plugin_manager. The modules in it are mock
// plugins put straight into its command/channel/event lists, which is what
// load_single_plugin() would do for a real DLL; a settings store over a
// throwaway INI file stands in for the agent's own, so the enabled/disabled
// state of a module reads the way it does in the daemon.

#include "registry_query_handler.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <boost/filesystem.hpp>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/registry.hpp>
#include <settings/test_helpers.hpp>
#include <string>
#include <vector>

#include "../libs/settings_manager/settings_manager_impl.h"
#include "path_manager.hpp"
#include "plugins/plugin_cache.hpp"
#include "plugins/plugin_interface.hpp"

using nsclient::core::registry_query_handler;
using PB::Registry::ItemType;
using Inventory = PB::Registry::RegistryResponseMessage::Response::Inventory;

namespace {

// A loaded module, as far as the registry can tell. Which handlers it claims
// decides which of the plugin_manager's lists will take it.
class registry_test_plugin : public nsclient::core::plugin_interface {
 public:
  registry_test_plugin(unsigned int id, const std::string &module, bool experimental = false)
      : plugin_interface(id, ""), module_(module), experimental_(experimental) {}

  bool load_plugin(NSCAPI::moduleLoadMode) override { return true; }
  bool has_start() override { return false; }
  bool start_plugin() override { return true; }
  bool has_prepare_shutdown() override { return false; }
  void prepare_shutdown_plugin() override {}
  void unload_plugin() override {}
  std::string getName() override { return module_ + " title"; }
  std::string getDescription() override { return module_ + " description"; }
  std::string get_version() override { return "1.2.3"; }
  bool is_experimental() override { return experimental_; }
  bool hasCommandHandler() override { return true; }

  // Answers the registry's "help-pb" query with a parameter list naming the
  // command, so a test can tell that the details came from the right module.
  NSCAPI::nagiosReturn handleCommand(std::string request, std::string &reply) override {
    PB::Commands::QueryRequestMessage req;
    req.ParseFromString(request);
    PB::Commands::QueryResponseMessage resp;
    for (const auto &p : req.payload()) {
      queries.push_back(p.command() + (p.arguments_size() > 0 ? " " + p.arguments(0) : ""));
      PB::Registry::ParameterDetails details;
      details.add_parameter()->set_name(p.command() + "-option");
      PB::Commands::QueryResponseMessage::Response *r = resp.add_payload();
      r->set_command(p.command());
      r->set_result(PB::Common::ResultCode::OK);
      r->set_data(details.SerializeAsString());
    }
    reply = resp.SerializeAsString();
    return NSCAPI::cmd_return_codes::isSuccess;
  }
  bool hasNotificationHandler() override { return true; }
  NSCAPI::nagiosReturn handleNotification(const char *, std::string &, std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  NSCAPI::nagiosReturn handle_schedule(const std::string &) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMessageHandler() override { return false; }
  void handleMessage(const char *, unsigned int) override {}
  bool has_on_event() override { return true; }
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
  bool is_duplicate(boost::filesystem::path file, std::string alias) override { return module_ == file.string() && get_alias() == alias; }
  std::string getModule() override { return module_; }
  void on_log_message(const std::string &) override {}

  std::vector<std::string> queries;

 private:
  std::string module_;
  bool experimental_;
};

class test_provider : public settings_manager::provider_interface {
 public:
  test_provider() : logger_(settings_test::make_null_logger()) {}
  std::string expand_path(std::string file) override { return file; }
  nsclient::logging::logger_instance get_logger() const override { return logger_; }
  void apply_path_overrides(std::map<std::string, std::string>) override {}

 private:
  nsclient::logging::logger_instance logger_;
};

class RegistryQueryTest : public ::testing::Test {
 protected:
  settings_test::temp_dir dir_;
  std::unique_ptr<test_provider> provider_;
  nsclient::logging::logger_instance logger_ = settings_test::make_null_logger();
  nsclient::core::path_instance paths_;
  nsclient::core::plugin_mgr_instance plugins_;
  std::shared_ptr<registry_test_plugin> checks_;   // id 1: CheckSystem
  std::shared_ptr<registry_test_plugin> scripts_;  // id 2: CheckExternalScripts, experimental module

  void SetUp() override {
    const boost::filesystem::path ini = dir_.file("settings.ini");
    settings_test::write_file(ini, "[/modules]\nCheckSystem = enabled\nCheckExternalScripts = disabled\n");
    provider_ = std::make_unique<test_provider>();
    std::string p = ini.generic_string();
    if (!p.empty() && p.front() == '/') p.erase(0, 1);
    ASSERT_TRUE(settings_manager::init_settings(provider_.get(), "ini:///" + p));

    paths_ = std::make_shared<nsclient::core::path_manager>(logger_);
    plugins_ = std::make_shared<nsclient::core::plugin_manager>(paths_, logger_);

    checks_ = std::make_shared<registry_test_plugin>(1, "CheckSystem");
    scripts_ = std::make_shared<registry_test_plugin>(2, "CheckExternalScripts", true);
    for (const auto &plugin : {checks_, scripts_}) {
      plugins_->get_commands()->add_plugin(plugin);
      plugins_->get_channels()->add_plugin(plugin);
      plugins_->get_event_subscribers()->add_plugin(plugin);
      plugins_->get_plugin_cache()->add_plugin(plugin);
    }
  }

  void TearDown() override {
    plugins_.reset();
    settings_manager::destroy_settings();
  }

  PB::Registry::RegistryResponseMessage run(const PB::Registry::RegistryRequestMessage &request) {
    PB::Registry::RegistryResponseMessage response;
    registry_query_handler(paths_, plugins_, logger_, request).parse(response);
    return response;
  }

  PB::Registry::RegistryResponseMessage inventory(ItemType type, const std::string &name = "", bool fetch_all = false) {
    PB::Registry::RegistryRequestMessage request;
    auto *inv = request.add_payload()->mutable_inventory();
    inv->add_type(type);
    if (!name.empty()) inv->set_name(name);
    inv->set_fetch_all(fetch_all);
    return run(request);
  }

  PB::Registry::RegistryResponseMessage registration(ItemType type, unsigned int plugin_id, const std::string &name,
                                                     const std::vector<std::string> &aliases = {}, bool unregister = false, bool experimental = false,
                                                     const std::string &description = "") {
    PB::Registry::RegistryRequestMessage request;
    auto *reg = request.add_payload()->mutable_registration();
    reg->set_type(type);
    reg->set_plugin_id(plugin_id);
    reg->set_name(name);
    reg->set_unregister(unregister);
    for (const std::string &a : aliases) reg->add_alias(a);
    reg->mutable_info()->set_description(description);
    reg->mutable_info()->set_experimental(experimental);
    return run(request);
  }

  PB::Registry::RegistryResponseMessage control(ItemType type, PB::Registry::Command command, const std::string &name) {
    PB::Registry::RegistryRequestMessage request;
    auto *ctl = request.add_payload()->mutable_control();
    ctl->set_type(type);
    ctl->set_command(command);
    ctl->set_name(name);
    return run(request);
  }

  // The inventory entries of one response, by name.
  static std::vector<std::string> names(const PB::Registry::RegistryResponseMessage &r) {
    std::vector<std::string> out;
    for (const auto &p : r.payload())
      for (const auto &i : p.inventory()) out.push_back(i.name());
    std::sort(out.begin(), out.end());
    return out;
  }

  static const Inventory *find(const PB::Registry::RegistryResponseMessage &r, const std::string &name) {
    for (const auto &p : r.payload())
      for (const auto &i : p.inventory())
        if (i.name() == name) return &i;
    return nullptr;
  }

  static std::string metadata(const Inventory &i, const std::string &key) {
    for (const auto &kv : i.info().metadata())
      if (kv.key() == key) return kv.value();
    return "<missing>";
  }

  static PB::Common::Result_StatusCodeType status(const PB::Registry::RegistryResponseMessage &r) {
    return r.payload_size() == 0 ? PB::Common::Result_StatusCodeType_STATUS_DELAYED : r.payload(0).result().code();
  }
};

}  // namespace

// ============================================================================
// Registering queries and aliases
// ============================================================================

TEST_F(RegistryQueryTest, RegisteredCommandIsListedWithItsModule) {
  const auto reg = registration(ItemType::QUERY, 1, "check_cpu", {}, false, false, "Check CPU load");
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, status(reg));

  const auto r = inventory(ItemType::QUERY);
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, status(r));
  const Inventory *cpu = find(r, "check_cpu");
  ASSERT_NE(nullptr, cpu);
  EXPECT_EQ(ItemType::COMMAND, cpu->type());
  EXPECT_EQ("Check CPU load", cpu->info().description());
  ASSERT_EQ(1, cpu->info().plugin_size());
  EXPECT_EQ("CheckSystem", cpu->info().plugin(0));
  EXPECT_FALSE(cpu->info().experimental());
}

TEST_F(RegistryQueryTest, CommandAliasesAreRegisteredAlongsideTheCommand) {
  registration(ItemType::QUERY, 1, "check_cpu", {"cpu", "processor"});

  EXPECT_EQ((std::vector<std::string>{"check_cpu"}), names(inventory(ItemType::QUERY)));
  const auto aliases = inventory(ItemType::QUERY_ALIAS);
  EXPECT_EQ((std::vector<std::string>{"cpu", "processor"}), names(aliases));
  const Inventory *cpu = find(aliases, "cpu");
  ASSERT_NE(nullptr, cpu);
  EXPECT_EQ(ItemType::QUERY_ALIAS, cpu->type());
  EXPECT_EQ("Alternative name for: check_cpu", cpu->info().description());
}

TEST_F(RegistryQueryTest, UnregisteringRemovesTheCommandAndItsAliases) {
  registration(ItemType::QUERY, 1, "check_cpu", {"cpu"});
  registration(ItemType::QUERY, 1, "check_memory");
  registration(ItemType::QUERY, 1, "check_cpu", {"cpu"}, true);

  EXPECT_EQ((std::vector<std::string>{"check_memory"}), names(inventory(ItemType::QUERY)));
  EXPECT_TRUE(names(inventory(ItemType::QUERY_ALIAS)).empty());
}

TEST_F(RegistryQueryTest, StandaloneAliasRegistration) {
  registration(ItemType::QUERY_ALIAS, 1, "alias_cpu", {"alias_cpu_ex"}, false, false, "Load average alias");
  const auto r = inventory(ItemType::QUERY_ALIAS);
  EXPECT_EQ((std::vector<std::string>{"alias_cpu", "alias_cpu_ex"}), names(r));
  const Inventory *alias = find(r, "alias_cpu_ex");
  ASSERT_NE(nullptr, alias);
  EXPECT_EQ("Load average alias", alias->info().description());
}

TEST_F(RegistryQueryTest, QueryByNameReturnsOnlyThatCommand) {
  registration(ItemType::QUERY, 1, "check_cpu");
  registration(ItemType::QUERY, 1, "check_memory");
  EXPECT_EQ((std::vector<std::string>{"check_memory"}), names(inventory(ItemType::QUERY, "check_memory")));
}

TEST_F(RegistryQueryTest, QueryByUnknownNameReturnsNothing) {
  registration(ItemType::QUERY, 1, "check_cpu");
  EXPECT_TRUE(names(inventory(ItemType::QUERY, "check_nothing")).empty());
}

// ============================================================================
// Experimental flag: the command's own, or inherited from its module
// ============================================================================

TEST_F(RegistryQueryTest, CommandDeclaredExperimentalIsExperimental) {
  registration(ItemType::QUERY, 1, "check_new", {}, false, true);
  const auto cmd_response = inventory(ItemType::QUERY);
  const Inventory *cmd = find(cmd_response, "check_new");
  ASSERT_NE(nullptr, cmd);
  EXPECT_TRUE(cmd->info().experimental());
}

TEST_F(RegistryQueryTest, CommandOfAnExperimentalModuleInheritsTheFlag) {
  // The module declares itself experimental once; none of its commands do.
  registration(ItemType::QUERY, 2, "check_script", {"script"});
  const auto cmd_response = inventory(ItemType::QUERY);
  const Inventory *cmd = find(cmd_response, "check_script");
  ASSERT_NE(nullptr, cmd);
  EXPECT_TRUE(cmd->info().experimental());
  EXPECT_EQ("CheckExternalScripts", cmd->info().plugin(0));
  // ...and the same holds for its aliases and for a lookup by name.
  const auto alias_response = inventory(ItemType::QUERY_ALIAS);
  const Inventory *alias = find(alias_response, "script");
  ASSERT_NE(nullptr, alias);
  EXPECT_TRUE(alias->info().experimental());
  const auto named_response = inventory(ItemType::QUERY, "check_script");
  const Inventory *named = find(named_response, "check_script");
  ASSERT_NE(nullptr, named);
  EXPECT_TRUE(named->info().experimental());
}

// ============================================================================
// fetch_all: the command's own parameter description
// ============================================================================

TEST_F(RegistryQueryTest, FetchAllAsksTheModuleForItsParameters) {
  registration(ItemType::QUERY, 1, "check_cpu");
  registration(ItemType::QUERY, 1, "check_memory");

  const auto all = inventory(ItemType::QUERY, "", true);
  const Inventory *cpu = find(all, "check_cpu");
  ASSERT_NE(nullptr, cpu);
  ASSERT_EQ(1, cpu->parameters().parameter_size());
  EXPECT_EQ("check_cpu-option", cpu->parameters().parameter(0).name());

  const auto one = inventory(ItemType::QUERY, "check_memory", true);
  const Inventory *mem = find(one, "check_memory");
  ASSERT_NE(nullptr, mem);
  ASSERT_EQ(1, mem->parameters().parameter_size());
  EXPECT_EQ("check_memory-option", mem->parameters().parameter(0).name());

  EXPECT_EQ((std::vector<std::string>{"check_cpu help-pb", "check_memory help-pb", "check_memory help-pb"}), checks_->queries);
}

TEST_F(RegistryQueryTest, PlainListingDoesNotCallIntoModules) {
  registration(ItemType::QUERY, 1, "check_cpu");
  inventory(ItemType::QUERY);
  EXPECT_TRUE(checks_->queries.empty());
}

// ============================================================================
// Notification handlers and event subscriptions
// ============================================================================

TEST_F(RegistryQueryTest, HandlersListTheirSubscribers) {
  registration(ItemType::HANDLER, 1, "nsca");
  registration(ItemType::HANDLER, 2, "nsca");
  registration(ItemType::HANDLER, 2, "graphite");

  const auto r = inventory(ItemType::HANDLER);
  EXPECT_EQ((std::vector<std::string>{"graphite", "nsca"}), names(r));
  const Inventory *nsca = find(r, "nsca");
  ASSERT_NE(nullptr, nsca);
  EXPECT_EQ(ItemType::HANDLER, nsca->type());
  std::vector<std::string> plugins(nsca->info().plugin().begin(), nsca->info().plugin().end());
  std::sort(plugins.begin(), plugins.end());
  EXPECT_EQ((std::vector<std::string>{"CheckExternalScripts", "CheckSystem"}), plugins);

  EXPECT_EQ((std::vector<std::string>{"graphite"}), names(inventory(ItemType::HANDLER, "graphite")));
}

TEST_F(RegistryQueryTest, UnregisteringAHandlerKeepsOtherSubscribers) {
  registration(ItemType::HANDLER, 1, "nsca");
  registration(ItemType::HANDLER, 2, "nsca");
  registration(ItemType::HANDLER, 1, "nsca", {}, true);
  const auto nsca_response = inventory(ItemType::HANDLER);
  const Inventory *nsca = find(nsca_response, "nsca");
  ASSERT_NE(nullptr, nsca);
  ASSERT_EQ(1, nsca->info().plugin_size());
  EXPECT_EQ("CheckExternalScripts", nsca->info().plugin(0));
}

TEST_F(RegistryQueryTest, EventSubscriptionsAreListedAndRemovable) {
  registration(ItemType::EVENT, 1, "eventlog:system");
  registration(ItemType::EVENT, 2, "logfile");

  const auto r = inventory(ItemType::EVENT);
  EXPECT_EQ((std::vector<std::string>{"eventlog:system", "logfile"}), names(r));
  EXPECT_EQ(ItemType::EVENT, find(r, "logfile")->type());
  EXPECT_EQ((std::vector<std::string>{"logfile"}), names(inventory(ItemType::EVENT, "logfile")));

  registration(ItemType::EVENT, 2, "logfile", {}, true);
  EXPECT_EQ((std::vector<std::string>{"eventlog:system"}), names(inventory(ItemType::EVENT)));
}

// ============================================================================
// Modules
// ============================================================================

TEST_F(RegistryQueryTest, LoadedModulesAreListedWithTheirState) {
  const auto r = inventory(ItemType::MODULE);
  EXPECT_EQ((std::vector<std::string>{"CheckExternalScripts", "CheckSystem"}), names(r));

  const Inventory *sys = find(r, "CheckSystem");
  ASSERT_NE(nullptr, sys);
  EXPECT_EQ(ItemType::MODULE, sys->type());
  EXPECT_EQ("CheckSystem", sys->id());
  EXPECT_EQ("CheckSystem title", sys->info().title());
  EXPECT_EQ("CheckSystem description", sys->info().description());
  EXPECT_FALSE(sys->info().experimental());
  EXPECT_EQ("1", metadata(*sys, "plugin_id"));
  EXPECT_EQ("true", metadata(*sys, "loaded"));
  EXPECT_EQ("", metadata(*sys, "alias"));
  EXPECT_EQ("true", metadata(*sys, "enabled"));

  const Inventory *scripts = find(r, "CheckExternalScripts");
  ASSERT_NE(nullptr, scripts);
  EXPECT_TRUE(scripts->info().experimental());
  EXPECT_EQ("false", metadata(*scripts, "enabled"));
}

TEST_F(RegistryQueryTest, ModuleByNameStopsAtThatModule) {
  EXPECT_EQ((std::vector<std::string>{"CheckSystem"}), names(inventory(ItemType::MODULE, "CheckSystem")));
}

TEST_F(RegistryQueryTest, CachedButUnloadedModuleIsOnlyListedWhenAskedForAll) {
  nsclient::core::plugin_cache::plugin_cache_list_type list;
  nsclient::core::plugin_cache_item item;
  item.id = 9;
  item.dll = "CheckLogFile";
  item.title = "CheckLogFile title";
  item.is_loaded = false;
  list.push_back(item);
  plugins_->get_plugin_cache()->add_plugins(list);

  EXPECT_EQ(nullptr, find(inventory(ItemType::MODULE), "CheckLogFile"));
  const auto log_response = inventory(ItemType::MODULE, "", true);
  const Inventory *log = find(log_response, "CheckLogFile");
  ASSERT_NE(nullptr, log);
  EXPECT_EQ("false", metadata(*log, "loaded"));
  // Not in the [/modules] section at all: not enabled.
  EXPECT_EQ("false", metadata(*log, "enabled"));
}

TEST_F(RegistryQueryTest, CloningAModuleHandsBackANewId) {
  // Only a module in the plugin list can be cloned; the mocks are in the
  // command list but were never loaded through it, so the clone is refused
  // and no id is handed back.
  const auto r = registration(ItemType::MODULE, 1, "CheckSystem");
  ASSERT_EQ(1, r.payload_size());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, status(r));
  EXPECT_FALSE(r.payload(0).has_registration() && r.payload(0).registration().item_id() != 0);
}

TEST_F(RegistryQueryTest, EnableAndDisableWriteTheModulesSection) {
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, status(control(ItemType::MODULE, PB::Registry::Command::DISABLE, "CheckSystem")));
  EXPECT_FALSE(plugins_->is_enabled("CheckSystem"));
  EXPECT_EQ("false", metadata(*find(inventory(ItemType::MODULE), "CheckSystem"), "enabled"));

  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, status(control(ItemType::MODULE, PB::Registry::Command::ENABLE, "CheckExternalScripts")));
  EXPECT_TRUE(plugins_->is_enabled("CheckExternalScripts"));
}

TEST_F(RegistryQueryTest, LoadingAModuleThatDoesNotExistFails) {
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_ERROR, status(control(ItemType::MODULE, PB::Registry::Command::LOAD, "NoSuchModuleAnywhere")));
}

TEST_F(RegistryQueryTest, UnloadingAModuleThatIsNotLoadedFails) {
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_ERROR, status(control(ItemType::MODULE, PB::Registry::Command::UNLOAD, "NoSuchModuleAnywhere")));
}

TEST_F(RegistryQueryTest, ControlOfANonModuleIsAnError) {
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_ERROR, status(control(ItemType::QUERY, PB::Registry::Command::LOAD, "check_cpu")));
}

// ============================================================================
// Mixed and malformed requests
// ============================================================================

TEST_F(RegistryQueryTest, InventoryOfAllCoversEveryKind) {
  registration(ItemType::QUERY, 1, "check_cpu", {"cpu"});
  registration(ItemType::HANDLER, 1, "nsca");
  registration(ItemType::EVENT, 1, "logfile");

  const auto r = inventory(ItemType::ALL);
  ASSERT_EQ(1, r.payload_size());
  std::vector<ItemType> types;
  for (const auto &i : r.payload(0).inventory()) types.push_back(i.type());
  for (ItemType t : {ItemType::COMMAND, ItemType::QUERY_ALIAS, ItemType::MODULE, ItemType::HANDLER, ItemType::EVENT})
    EXPECT_NE(types.end(), std::find(types.begin(), types.end(), t)) << "missing item type " << t;
}

TEST_F(RegistryQueryTest, EachPayloadGetsItsOwnResponse) {
  PB::Registry::RegistryRequestMessage request;
  auto *reg = request.add_payload()->mutable_registration();
  reg->set_type(ItemType::QUERY);
  reg->set_plugin_id(1);
  reg->set_name("check_cpu");
  request.add_payload()->mutable_inventory()->add_type(ItemType::QUERY);
  request.add_payload();  // neither inventory, registration nor control: ignored

  const auto r = run(request);
  ASSERT_EQ(2, r.payload_size());
  EXPECT_EQ((std::vector<std::string>{"check_cpu"}), names(r));
}

TEST_F(RegistryQueryTest, UnsupportedRegistrationTypeStillAnswers) {
  const auto r = registration(ItemType::COMMAND, 1, "whatever");
  ASSERT_EQ(1, r.payload_size());
  EXPECT_EQ(PB::Common::Result_StatusCodeType_STATUS_OK, status(r));
  EXPECT_TRUE(names(inventory(ItemType::QUERY)).empty());
}
