#include "registry_query_handler.hpp"

#include <nscapi/nscapi_protobuf_functions.hpp>

#include "plugins/dll_plugin.h"

namespace nsclient {

namespace core {

registry_query_handler::registry_query_handler(nsclient::core::path_instance path_, nsclient::core::plugin_mgr_instance plugins_,
                                               nsclient::logging::logger_instance logger_, const PB::Registry::RegistryRequestMessage &request)
    : path_(path_), plugins_(plugins_), logger_(logger_), request_(request) {}

void registry_query_handler::parse(PB::Registry::RegistryResponseMessage &response) {
  for (const PB::Registry::RegistryRequestMessage::Request &r : request_.payload()) {
    if (r.has_inventory()) {
      parse_inventory(r.inventory(), response);
    } else if (r.has_registration()) {
      parse_registration(r.registration(), response);
    } else if (r.has_control()) {
      parse_control(r.control(), response);

    } else {
      LOG_ERROR_CORE("Registration query: Unsupported action");
    }
  }
}

void registry_query_handler::inventory_queries(const PB::Registry::RegistryRequestMessage::Request::Inventory &q,
                                               PB::Registry::RegistryResponseMessage::Response *rp) {
  if (!q.name().empty()) {
    nsclient::commands::command_info info = plugins_->get_commands()->describe(q.name());
    if (!info.name.empty()) {
      PB::Registry::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
      rpp->set_name(q.name());
      rpp->set_type(PB::Registry::ItemType::COMMAND);
      rpp->mutable_info()->add_plugin(plugins_->get_plugin_cache()->find_plugin_alias(info.plugin_id));
      rpp->mutable_info()->set_title(info.name);
      rpp->mutable_info()->set_description(info.description);
      if (q.fetch_all()) {
        PB::Commands::QueryRequestMessage req;
        PB::Commands::QueryRequestMessage::Request *p = req.add_payload();
        p->set_command(q.name());
        p->add_arguments("help-pb");
        PB::Commands::QueryResponseMessage res = plugins_->execute_query(req);
        for (int i = 0; i < res.payload_size(); i++) {
          const PB::Commands::QueryResponseMessage::Response p2 = res.payload(i);
          rpp->mutable_parameters()->ParseFromString(p2.data());
        }
      }
    }
  } else {
    for (const std::string &command : plugins_->get_commands()->list_commands()) {
      nsclient::commands::command_info info = plugins_->get_commands()->describe(command);
      PB::Registry::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
      rpp->set_name(command);
      rpp->set_type(PB::Registry::ItemType::COMMAND);
      rpp->mutable_info()->add_plugin(plugins_->get_plugin_cache()->find_plugin_alias(info.plugin_id));
      rpp->mutable_info()->set_title(info.name);
      rpp->mutable_info()->set_description(info.description);
      if (q.fetch_all()) {
        std::string resp;
        PB::Commands::QueryRequestMessage req;
        PB::Commands::QueryRequestMessage::Request *p1 = req.add_payload();
        p1->set_command(command);
        p1->add_arguments("help-pb");
        PB::Commands::QueryResponseMessage res = plugins_->execute_query(req);
        for (int i = 0; i < res.payload_size(); i++) {
          const PB::Commands::QueryResponseMessage::Response p2 = res.payload(i);
          rpp->mutable_parameters()->ParseFromString(p2.data());
        }
      }
    }
  }
}

void registry_query_handler::find_plugins_on_disk(boost::unordered_set<std::string> &unique_instances,
                                                  const PB::Registry::RegistryRequestMessage::Request::Inventory &q,
                                                  PB::Registry::RegistryResponseMessage::Response *rp) {
  nsclient::core::plugin_cache::plugin_cache_list_type tmp_list;
  boost::filesystem::path pluginPath = path_->expand_path("${module-path}");
  boost::filesystem::directory_iterator end_itr;  // default construction yields past-the-end
  for (boost::filesystem::directory_iterator itr(pluginPath); itr != end_itr; ++itr) {
    if (!is_directory(itr->status())) {
      boost::filesystem::path file = itr->path().filename();
      if (nsclient::core::plugin_manager::is_module(pluginPath / file)) {
        const std::string module = nsclient::core::plugin_manager::file_to_module(file);
        if (!plugins_->get_plugin_cache()->has_module(module)) {
          plugin_cache_item itm = inventory_plugin_on_disk(tmp_list, file.string());
          if (!itm.dll.empty()) {
            std::string key = itm.dll + "::" + itm.alias;
            if (unique_instances.find(key) != unique_instances.end()) {
              continue;
            }
            unique_instances.emplace(key);
            if (q.name() != itm.dll && q.name() != itm.alias) {
              continue;
            }
            add_module(rp, itm, false);
          }
        }
      }
    }
  }
  plugins_->get_plugin_cache()->add_plugins(tmp_list);
}

void registry_query_handler::add_module(PB::Registry::RegistryResponseMessage::Response *rp, const plugin_cache_item &plugin, bool is_enabled) {
  PB::Registry::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
  rpp->set_name(plugin.dll);
  rpp->set_type(PB::Registry::ItemType::MODULE);
  rpp->set_id(plugin.alias.empty() ? plugin.dll : plugin.alias);
  rpp->mutable_info()->add_plugin(plugin.dll);
  rpp->mutable_info()->set_title(plugin.title);
  rpp->mutable_info()->set_description(plugin.desc);
  PB::Common::KeyValue *kvp = rpp->mutable_info()->add_metadata();
  kvp->set_key("plugin_id");
  kvp->set_value(str::xtos(plugin.id));
  kvp = rpp->mutable_info()->add_metadata();
  kvp->set_key("loaded");
  kvp->set_value(plugin.is_loaded ? "true" : "false");
  kvp = rpp->mutable_info()->add_metadata();
  kvp->set_key("alias");
  kvp->set_value(plugin.alias);
  kvp = rpp->mutable_info()->add_metadata();
  kvp->set_key("enabled");
  kvp->set_value(is_enabled ? "true" : "false");
}

void registry_query_handler::inventory_modules(const PB::Registry::RegistryRequestMessage::Request::Inventory &q,
                                               PB::Registry::RegistryResponseMessage::Response *rp) {
  boost::unordered_set<std::string> unique_instances;
  for (const nsclient::core::plugin_cache_item &plugin : plugins_->get_plugin_cache()->get_list()) {
    std::string key = plugin.dll + "::" + plugin.alias;
    if (unique_instances.find(key) != unique_instances.end()) {
      continue;
    }
    unique_instances.emplace(key);
    if (!q.name().empty() && q.name() != plugin.dll) {
      continue;
    }

    if (q.fetch_all() || plugin.is_loaded || (q.name() == plugin.dll)) {
      add_module(rp, plugin, plugins_->is_enabled(plugin.dll));
    }

    if (q.name() == plugin.dll) {
      return;
    }
  }
  if (!q.name().empty()) {
    nsclient::core::plugin_cache::plugin_cache_list_type tmp_list;
    plugin_cache_item itm = inventory_plugin_on_disk(tmp_list, q.name());
    if (!itm.dll.empty()) {
      add_module(rp, itm, false);
      plugins_->get_plugin_cache()->add_plugins(tmp_list);
    }
    return;
  }

  if (!plugins_->get_plugin_cache()->has_all() && q.fetch_all()) {
    find_plugins_on_disk(unique_instances, q, rp);
  }
}

void registry_query_handler::parse_inventory(const PB::Registry::RegistryRequestMessage::Request::Inventory &q,
                                             PB::Registry::RegistryResponseMessage &response) {
  PB::Registry::RegistryResponseMessage::Response *rp = response.add_payload();
  for (int i = 0; i < q.type_size(); i++) {
    PB::Registry::ItemType type = q.type(i);
    if (type == PB::Registry::ItemType::QUERY || type == PB::Registry::ItemType::ALL) {
      inventory_queries(q, rp);
    }
    if (type == PB::Registry::ItemType::QUERY_ALIAS || type == PB::Registry::ItemType::ALL) {
      for (const std::string &command : plugins_->get_commands()->list_aliases()) {
        nsclient::commands::command_info info = plugins_->get_commands()->describe(command);
        PB::Registry::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
        rpp->set_name(command);
        rpp->set_type(PB::Registry::ItemType::QUERY_ALIAS);
        rpp->mutable_info()->add_plugin(plugins_->get_plugin_cache()->find_plugin_alias(info.plugin_id));
        rpp->mutable_info()->set_title(info.name);
        rpp->mutable_info()->set_description(info.description);
      }
    }
    if (type == PB::Registry::ItemType::MODULE || type == PB::Registry::ItemType::ALL) {
      inventory_modules(q, rp);
    }
  }
  rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
}

void registry_query_handler::parse_registration(const PB::Registry::RegistryRequestMessage::Request::Registration &registration,
                                                PB::Registry::RegistryResponseMessage &response) {
  PB::Registry::RegistryResponseMessage::Response *rp = response.add_payload();
  if (registration.type() == PB::Registry::ItemType::QUERY) {
    if (registration.unregister()) {
      plugins_->get_commands()->unregister_command(registration.plugin_id(), registration.name());
      for (const std::string &alias : registration.alias()) plugins_->get_commands()->unregister_command(registration.plugin_id(), alias);
    } else {
      plugins_->get_commands()->register_command(registration.plugin_id(), registration.name(), registration.info().description());
      std::string description = "Alternative name for: " + registration.name();
      for (const std::string &alias : registration.alias()) plugins_->get_commands()->register_alias(registration.plugin_id(), alias, description);
    }
  } else if (registration.type() == PB::Registry::ItemType::QUERY_ALIAS) {
    plugins_->get_commands()->register_alias(registration.plugin_id(), registration.name(), registration.info().description());
    for (int i = 0; i < registration.alias_size(); i++) {
      plugins_->get_commands()->register_alias(registration.plugin_id(), registration.alias(i), registration.info().description());
    }
  } else if (registration.type() == PB::Registry::ItemType::HANDLER) {
    plugins_->get_channels()->register_listener(registration.plugin_id(), registration.name());
  } else if (registration.type() == PB::Registry::ItemType::EVENT) {
    plugins_->get_event_subscribers()->register_listener(registration.plugin_id(), registration.name());
  } else if (registration.type() == PB::Registry::ItemType::MODULE) {
    PB::Registry::RegistryResponseMessage::Response::Registration *rpp = rp->mutable_registration();
    int new_id = plugins_->clone_plugin(registration.plugin_id());
    if (new_id != -1) {
      rpp->set_item_id(new_id);
    }
  } else {
    LOG_ERROR_CORE("Registration query: Unsupported type");
  }
  rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
}

void registry_query_handler::parse_control(const PB::Registry::RegistryRequestMessage::Request::Control &control,
                                           PB::Registry::RegistryResponseMessage &response) {
  PB::Registry::RegistryResponseMessage::Response *rp = response.add_payload();
  rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);
  if (control.type() == PB::Registry::ItemType::MODULE) {
    if (control.command() == PB::Registry::Command::LOAD) {
      if (plugins_->load_single_plugin(control.name(), control.alias(), true)) {
        rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
      } else {
        LOG_ERROR_CORE("Failed to find: " + control.name());
      }
    } else if (control.command() == PB::Registry::Command::UNLOAD) {
      if (plugins_->remove_plugin(control.name())) {
        rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
      }
    } else if (control.command() == PB::Registry::Command::ENABLE) {
      if (plugins_->enable_plugin(control.name())) {
        rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
      }
    } else if (control.command() == PB::Registry::Command::DISABLE) {
      if (plugins_->disable_plugin(control.name())) {
        rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
      }
    } else {
      LOG_ERROR_CORE("Registration query: Invalid command");
    }
  } else {
    LOG_ERROR_CORE("Registration query: Unsupported type");
  }
}

plugin_cache_item registry_query_handler::inventory_plugin_on_disk(nsclient::core::plugin_cache::plugin_cache_list_type &list, std::string plugin) {
  plugin_cache_item itm;
  try {
    bool loaded = false;
    plugin_type instance = plugins_->only_load_module(plugin, "", loaded);
    if (!loaded) {
      return itm;
    }
    itm.dll = instance->getModule();
    itm.alias = "";
    itm.desc = instance->getDescription();
    itm.id = instance->get_id();
    itm.is_loaded = false;
    list.push_back(itm);
  } catch (const std::exception &e) {
    LOG_ERROR_CORE("Failed to load " + plugin + ": " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Failed to load " + plugin + ": UNKNOWN EXCEPTION");
  }
  return itm;
}

}  // namespace core
}  // namespace nsclient
