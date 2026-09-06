// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/function.hpp>
#include <client/simple_client.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <nscapi/protobuf/settings.hpp>
#include <nscapi/protobuf/settings_functions.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>

static void create_registry_query(const nscapi::core_wrapper *core, const std::string &command, const PB::Registry::ItemType &type,
                                  PB::Registry::RegistryResponseMessage &response_message, const bool fetch_all = false) {
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  if (!command.empty()) {
    payload->mutable_inventory()->set_name(command);
    payload->mutable_inventory()->set_fetch_all(true);
  } else if (fetch_all) {
    // Without a name, fetch_all is what makes the core look past the plugins
    // it has already instantiated and scan the module directory. That scan
    // loads every module found, so only ask when the answer is worth it.
    payload->mutable_inventory()->set_fetch_all(true);
  }
  payload->mutable_inventory()->add_type(type);
  std::string pb_response;
  core->registry_query(rrm.SerializeAsString(), pb_response);
  response_message.ParseFromString(pb_response);
}

std::string render_command(const ::PB::Registry::RegistryResponseMessage::Response::Inventory &inv) {
  std::string data = "command:\t" + inv.name() + "\n" + inv.info().description() + "\n\nParameters:\n";
  for (int i = 0; i < inv.parameters().parameter_size(); i++) {
    ::PB::Registry::ParameterDetail p = inv.parameters().parameter(i);
    std::string desc = p.long_description();
    std::size_t pos = desc.find('\n');
    if (pos != std::string::npos) desc = desc.substr(0, pos - 1);
    data += p.name() + "\t" + desc + "\n";
  }
  return data;
}
std::string render_plugin(const ::PB::Registry::RegistryResponseMessage::Response::Inventory &inv) {
  std::string loaded = "[ ]";
  for (int i = 0; i < inv.info().metadata_size(); i++) {
    if (inv.info().metadata(i).key() == "loaded" && inv.info().metadata(i).value() == "true") loaded = "[X]";
  }
  return loaded + "\t" + inv.name() + "\t-" + inv.info().description();
}
std::string render_query(const ::PB::Registry::RegistryResponseMessage::Response::Inventory &inv) { return inv.name() + "\t-" + inv.info().description(); }

static std::string render_list(const PB::Registry::RegistryResponseMessage &response_message,
                               boost::function<std::string(const ::PB::Registry::RegistryResponseMessage::Response::Inventory &)> renderer) {
  std::string list;
  for (const ::PB::Registry::RegistryResponseMessage::Response &pl : response_message.payload()) {
    for (const ::PB::Registry::RegistryResponseMessage_Response_Inventory &i : pl.inventory()) {
      if (!list.empty()) list += "\n";
      list += renderer(i);
    }
    if (pl.result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
      return "Error: " + pl.result().message();
    }
  }
  return list;
}

namespace client {

const std::vector<command_info> &builtin_commands() {
  // Ordered the way the help text should read: what to look at first, then the
  // things that change state.
  static const std::vector<command_info> commands = {
      {"help", "", "show this help"},
      {"exit", "", "leave the interactive prompt"},
      {"queries", "", "list all available queries"},
      {"commands", "", "list all available queries (alias for queries)"},
      {"aliases", "", "list all available query aliases"},
      {"list", "", "list queries and aliases"},
      {"plugins", "", "list all plugins and whether they are loaded"},
      {"desc", "<query>", "describe a query and its parameters"},
      {"metrics", "[prefix]", "show the metrics collected so far"},
      {"settings", "", "dump the current settings"},
      {"exec", "<target> <command> [args]", "run a command on one module"},
      {"load", "<module>", "load a module now"},
      {"unload", "<module>", "unload a module now"},
      {"enable", "<module>", "enable a module in the configuration and save"},
      {"disable", "<module>", "disable a module in the configuration and save"},
      {"reload", "", "reload all modules"},
  };
  return commands;
}

static std::string render_help() {
  const std::string catch_all = "<any other command>";
  std::string::size_type width = catch_all.size();
  for (const command_info &c : builtin_commands()) {
    const std::string::size_type len = c.name.size() + (c.args.empty() ? 0 : c.args.size() + 1);
    if (len > width) width = len;
  }
  std::string help = "Commands:";
  for (const command_info &c : builtin_commands()) {
    const std::string usage = c.args.empty() ? c.name : c.name + " " + c.args;
    help += "\n\t" + usage + std::string(width - usage.size() + 2, ' ') + "- " + c.description;
  }
  help += "\n\t" + catch_all + std::string(width - catch_all.size() + 2, ' ') + "- run as a query";
  return help;
}

static std::vector<command_info> collect(const PB::Registry::RegistryResponseMessage &response_message) {
  std::vector<command_info> ret;
  for (const ::PB::Registry::RegistryResponseMessage::Response &pl : response_message.payload()) {
    for (const ::PB::Registry::RegistryResponseMessage_Response_Inventory &i : pl.inventory()) {
      command_info info;
      info.name = i.name();
      info.description = i.info().description();
      ret.push_back(info);
    }
  }
  return ret;
}

std::vector<command_info> cli_client::list_queries() const {
  PB::Registry::RegistryResponseMessage response_message;
  create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY, response_message);
  std::vector<command_info> ret = collect(response_message);
  // Aliases arrive under a separate inventory type but behave exactly like
  // queries at the prompt, so the caller gets one merged list.
  PB::Registry::RegistryResponseMessage alias_message;
  create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY_ALIAS, alias_message);
  const std::vector<command_info> aliases = collect(alias_message);
  ret.insert(ret.end(), aliases.begin(), aliases.end());
  return ret;
}

static bool metadata_flag(const ::PB::Registry::RegistryResponseMessage::Response::Inventory &inv, const std::string &key) {
  for (int i = 0; i < inv.info().metadata_size(); i++) {
    if (inv.info().metadata(i).key() == key) return inv.info().metadata(i).value() == "true";
  }
  return false;
}

static std::vector<module_info> collect_modules(const PB::Registry::RegistryResponseMessage &response_message) {
  std::vector<module_info> ret;
  for (const ::PB::Registry::RegistryResponseMessage::Response &pl : response_message.payload()) {
    for (const ::PB::Registry::RegistryResponseMessage_Response_Inventory &i : pl.inventory()) {
      module_info info;
      info.name = i.name();
      info.description = i.info().description();
      info.loaded = metadata_flag(i, "loaded");
      info.enabled = metadata_flag(i, "enabled");
      ret.push_back(info);
    }
  }
  return ret;
}

std::vector<module_info> cli_client::list_modules() const {
  PB::Registry::RegistryResponseMessage response_message;
  create_registry_query(handler->get_core(), "", PB::Registry::ItemType::MODULE, response_message);
  return collect_modules(response_message);
}

std::vector<module_info> cli_client::list_all_modules() const {
  PB::Registry::RegistryResponseMessage response_message;
  create_registry_query(handler->get_core(), "", PB::Registry::ItemType::MODULE, response_message, true);
  return collect_modules(response_message);
}

std::vector<std::string> cli_client::list_parameters(const std::string &query) const {
  std::vector<std::string> ret;
  if (query.empty()) return ret;
  PB::Registry::RegistryResponseMessage response_message;
  create_registry_query(handler->get_core(), query, PB::Registry::ItemType::QUERY, response_message);
  for (const ::PB::Registry::RegistryResponseMessage::Response &pl : response_message.payload()) {
    for (const ::PB::Registry::RegistryResponseMessage_Response_Inventory &i : pl.inventory()) {
      if (i.name() != query) continue;
      for (int p = 0; p < i.parameters().parameter_size(); p++) {
        ret.push_back(i.parameters().parameter(p).name());
      }
    }
  }
  return ret;
}

void cli_client::handle_command(const std::string &command) {
  if (command == "plugins") {
    PB::Registry::RegistryResponseMessage response_message;
    create_registry_query(handler->get_core(), "", PB::Registry::ItemType::MODULE, response_message);
    std::string list = render_list(response_message, &render_plugin);
    handler->output_message(list.empty() ? "Nothing found" : list);
  } else if (command == "help") {
    handler->output_message(render_help());
  } else if (command == "reload") {
    if (!handler->get_core()->reload("delayed,service")) {
      NSC_LOG_ERROR("Failed to reload modules");
    }
  } else if (command.size() > 6 && command.substr(0, 6) == "enable") {
    std::string name = command.substr(7);
    bool has_errors = false;
    {
      PB::Settings::SettingsRequestMessage srm;
      PB::Settings::SettingsRequestMessage::Request *r = srm.add_payload();
      r->mutable_update()->mutable_node()->set_path("/modules");
      r->mutable_update()->mutable_node()->set_key(name);
      r->mutable_update()->mutable_node()->set_value("enabled");
      r->set_plugin_id(handler->get_plugin_id());
      std::string response;
      handler->get_core()->settings_query(srm.SerializeAsString(), response);
      PB::Settings::SettingsResponseMessage response_message;
      response_message.ParseFromString(response);
      for (int i = 0; i < response_message.payload_size(); i++) {
        if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
          handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
          has_errors = true;
        }
      }
    }
    {
      PB::Settings::SettingsRequestMessage srm;
      PB::Settings::SettingsRequestMessage::Request *r = srm.add_payload();
      r->mutable_control()->set_command(PB::Settings::Command::SAVE);
      r->set_plugin_id(handler->get_plugin_id());
      std::string response;
      handler->get_core()->settings_query(srm.SerializeAsString(), response);
      PB::Settings::SettingsResponseMessage response_message;
      response_message.ParseFromString(response);
      for (int i = 0; i < response_message.payload_size(); i++) {
        if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
          handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
          has_errors = true;
        }
      }
    }
    if (!has_errors) handler->output_message(name + " enabled successfully...");
  } else if (command.size() > 7 && command.substr(0, 7) == "disable") {
    std::string name = command.substr(8);
    bool has_errors = false;
    {
      PB::Settings::SettingsRequestMessage srm;
      PB::Settings::SettingsRequestMessage::Request *r = srm.add_payload();
      r->mutable_update()->mutable_node()->set_path("/modules");
      r->mutable_update()->mutable_node()->set_key(name);
      r->mutable_update()->mutable_node()->set_value("disabled");
      r->set_plugin_id(handler->get_plugin_id());
      std::string response;
      handler->get_core()->settings_query(srm.SerializeAsString(), response);
      PB::Settings::SettingsResponseMessage response_message;
      response_message.ParseFromString(response);
      for (int i = 0; i < response_message.payload_size(); i++) {
        if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
          handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
          has_errors = true;
        }
      }
    }
    {
      PB::Settings::SettingsRequestMessage srm;
      PB::Settings::SettingsRequestMessage::Request *r = srm.add_payload();
      r->mutable_control()->set_command(PB::Settings::Command::SAVE);
      r->set_plugin_id(handler->get_plugin_id());
      std::string response;
      handler->get_core()->settings_query(srm.SerializeAsString(), response);
      PB::Settings::SettingsResponseMessage response_message;
      response_message.ParseFromString(response);
      for (int i = 0; i < response_message.payload_size(); i++) {
        if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
          handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
          has_errors = true;
        }
      }
    }
    if (!has_errors) handler->output_message(name + " disabled successfully...");
  } else if (command.size() > 4 && command.substr(0, 4) == "load") {
    PB::Registry::RegistryRequestMessage rrm;
    PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
    std::string name = command.substr(5);
    payload->mutable_control()->set_type(PB::Registry::ItemType::MODULE);
    payload->mutable_control()->set_command(PB::Registry::Command::LOAD);
    payload->mutable_control()->set_name(name);
    std::string pb_response, json_response;
    handler->get_core()->registry_query(rrm.SerializeAsString(), pb_response);
    PB::Registry::RegistryResponseMessage response_message;
    response_message.ParseFromString(pb_response);
    bool has_errors = false;
    for (int i = 0; i < response_message.payload_size(); i++) {
      if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
        handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
        has_errors = true;
      }
    }
    if (!has_errors) handler->output_message(name + " loaded successfully...");
  } else if (command.size() > 6 && command.substr(0, 6) == "unload") {
    PB::Registry::RegistryRequestMessage rrm;
    PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
    std::string name = command.substr(7);
    payload->mutable_control()->set_type(PB::Registry::ItemType::MODULE);
    payload->mutable_control()->set_command(PB::Registry::Command::UNLOAD);
    payload->mutable_control()->set_name(name);
    std::string pb_response, json_response;
    handler->get_core()->registry_query(rrm.SerializeAsString(), pb_response);
    PB::Registry::RegistryResponseMessage response_message;
    response_message.ParseFromString(pb_response);
    bool has_errors = false;
    for (int i = 0; i < response_message.payload_size(); i++) {
      if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
        handler->output_message("Failed to unload module: " + response_message.payload(i).result().message());
        has_errors = true;
      }
    }
    if (!has_errors) handler->output_message(name + " unloaded successfully...");
  } else if (command == "queries" || command == "commands") {
    PB::Registry::RegistryResponseMessage response_message;
    create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY, response_message);
    std::string list = render_list(response_message, &render_query);
    handler->output_message(list.empty() ? "Nothing found" : list);
  } else if (command == "aliases") {
    PB::Registry::RegistryResponseMessage response_message;
    create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY_ALIAS, response_message);
    std::string list = render_list(response_message, &render_query);
    handler->output_message(list.empty() ? "Nothing found" : list);
  } else if (command.size() > 5 && command.substr(0, 4) == "desc") {
    PB::Registry::RegistryResponseMessage response_message;
    create_registry_query(handler->get_core(), command.substr(5), PB::Registry::ItemType::QUERY, response_message);
    std::string data = render_list(response_message, &render_command);
    handler->output_message(data.empty() ? "Command not found" : data);
  } else if (command == "list") {
    PB::Registry::RegistryResponseMessage response_message;
    create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY, response_message);
    std::string list = render_list(response_message, &render_query);
    create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY_ALIAS, response_message);
    list = render_list(response_message, &render_query);
    handler->output_message(list.empty() ? "Nothing found" : list);
  } else if (command.size() >= 7 && command.substr(0, 7) == "metrics") {
    for (const metrics::metrics_store::values_map::value_type &v : metrics_store.get(command.substr(7))) {
      handler->output_message(v.first + "=" + v.second);
    }
  } else if (command.size() > 4 && command.substr(0, 4) == "exec") {
    try {
      std::list<std::string> args;
      str::utils::parse_command(command, args);
      if (args.size() < 3) {
        handler->output_message("Usage: exec <target> <command> [args]");
        return;
      }
      args.pop_front();
      std::string target = args.front();
      args.pop_front();
      std::string cmd = args.front();
      args.pop_front();
      std::list<std::string> result;
      nscapi::core_helper helper(handler->get_core(), handler->get_plugin_id());
      helper.exec_simple_command(target, cmd, args, result);
      for (const std::string &s : result) handler->output_message(s);
    } catch (const std::exception &e) {
      handler->output_message("Exception: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      handler->output_message("Unknown exception");
    }
  } else if (command.size() >= 8 && command.substr(0, 8) == "settings") {
    namespace pf = nscapi::protobuf::functions;

    pf::settings_query q(handler->get_plugin_id());
    q.list("/", true);

    handler->get_core()->settings_query(q.request(), q.response());
    if (!q.validate_response()) {
      handler->output_message("ERROR: " + q.get_response_error());
    } else {
      for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
        std::string tmp;
        tmp += val.path();
        tmp += "/" + val.key();
        tmp += "=" + val.get_string();
        handler->output_message(tmp);
      }
    }
  } else if (!command.empty()) {
    try {
      std::list<std::string> args;
      str::utils::parse_command(command, args);
      std::string cmd = args.front();
      args.pop_front();
      nscapi::core_helper helper(handler->get_core(), handler->get_plugin_id());
      std::string response;
      if (!helper.simple_query(cmd, args, response)) {
        NSC_LOG_ERROR("Failed to execute command: " + cmd);
      }
      if (!response.empty()) {
        try {
          PB::Commands::QueryResponseMessage message;
          message.ParseFromString(response);

          for (const PB::Commands::QueryResponseMessage::Response &payload : message.payload()) {
            for (const PB::Commands::QueryResponseMessage::Response::Line &l : payload.lines()) {
              handler->output_message(nscapi::plugin_helper::translateReturn(payload.result()) + ": " + l.message());
              const std::string perf = nscapi::protobuf::functions::build_performance_data(l, nscapi::protobuf::functions::no_truncation);
              // Most checks return none, and a bare "Performance data:" line
              // after every result is just noise at the prompt.
              if (!perf.empty()) handler->output_message(" Performance data: " + perf);
            }
          }
        } catch (std::exception &e) {
          handler->output_message("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
        }
      }
    } catch (const std::exception &e) {
      handler->output_message("Exception: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      handler->output_message("Unknown exception");
    }
  }
}

void cli_client::push_metrics(const PB::Metrics::MetricsMessage &response) { metrics_store.set(response); }

}  // namespace client