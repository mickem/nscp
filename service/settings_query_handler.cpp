#include "settings_query_handler.hpp"

#include "../libs/settings_manager/settings_manager_impl.h"

#include <nscapi/nscapi_protobuf_settings.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#ifdef HAVE_JSON_SPIRIT
#include <json_spirit.h>
#endif
#include <boost/unordered_set.hpp>

namespace nsclient {

namespace core {

settings_query_handler::settings_query_handler(nsclient::core::core_interface *core, const PB::Settings::SettingsRequestMessage &request)
    : logger_(core->get_logger()), request_(request), core_(core) {}

void settings_query_handler::settings_add_plugin_data(const std::set<unsigned int> &plugins, PB::Settings::Information *info) {
  for (const unsigned int i : plugins) {
    std::string name = core_->get_plugin_cache()->find_plugin_alias(i);
    if (name.substr(0, 21) != "Failed to find plugin") {
      info->add_plugin(name);
    }
  }
}

void settings_query_handler::parse(PB::Settings::SettingsResponseMessage &response) {
  std::string response_string;

  for (const PB::Settings::SettingsRequestMessage::Request &r : request_.payload()) {
    PB::Settings::SettingsResponseMessage::Response *rp = response.add_payload();
    try {
      if (r.has_inventory()) {
        parse_inventory(r.inventory(), rp);
      } else if (r.has_query()) {
        parse_query(r.query(), rp);
      } else if (r.has_registration()) {
        parse_registration(r.registration(), r.plugin_id(), rp);
      } else if (r.has_update()) {
        parse_update(r.update(), rp);
      } else if (r.has_control()) {
        parse_control(r.control(), rp);
      } else if (r.has_status()) {
        rp->mutable_status()->set_has_changed(settings_manager::get_core()->is_dirty());
        rp->mutable_status()->set_context(settings_manager::get_settings()->get_context());
        rp->mutable_status()->set_type(settings_manager::get_settings()->get_type());
        rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
      } else {
        rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
        rp->mutable_result()->set_message("Settings error: Invalid action");
        LOG_ERROR_CORE_STD("Settings error: Invalid action");
      }
    } catch (settings::settings_exception &e) {
      rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
      rp->mutable_result()->set_message("Settings error: " + utf8::utf8_from_native(e.what()));
      LOG_ERROR_CORE_STD("Settings error: " + utf8::utf8_from_native(e.what()));
    } catch (const std::exception &e) {
      rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
      rp->mutable_result()->set_message("Settings error: " + utf8::utf8_from_native(e.what()));
      LOG_ERROR_CORE_STD("Settings error: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
      rp->mutable_result()->set_message("Settings error");
      LOG_ERROR_CORE_STD("Settings error");
    }
  }
}

void settings_query_handler::parse_inventory(const PB::Settings::SettingsRequestMessage::Request::Inventory &q,
                                             PB::Settings::SettingsResponseMessage::Response *rp) {
  boost::optional<unsigned int> plugin_id;
  if (!q.plugin().empty()) {
    plugin_id = core_->get_plugin_cache()->find_plugin(q.plugin());
  }
  if (!q.node().key().empty()) {
    t.start("fetching key");
    auto desc = settings_manager::get_core()->get_registered_key(q.node().path(), q.node().key()).get_value_or(settings::settings_core::key_description());
    t.end();
    PB::Settings::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
    rpp->mutable_node()->CopyFrom(q.node());
    rpp->mutable_info()->set_title(desc.title);
    rpp->mutable_info()->set_description(desc.description);
  } else {
    bool fetch_samples = q.fetch_samples();
    if (q.recursive_fetch()) {
      std::string base_path;
      if (!q.node().path().empty()) base_path = q.node().path();
      t.start("fetching paths");
      std::list<std::string> list = settings_manager::get_core()->get_reg_sections(base_path, fetch_samples);
      t.end();
      for (const std::string &path : list) {
        if (q.fetch_keys()) {
          t.start("fetching keys");
          std::list<std::string> klist = settings_manager::get_core()->get_reg_keys(path, fetch_samples);
          t.end();
          boost::unordered_set<std::string> cache;
          for (const std::string &key : klist) {
            settings::settings_core::key_description desc = settings_manager::get_core()->get_registered_key(path, key).get_value_or(settings::settings_core::key_description());
            if (plugin_id && !desc.has_plugin(*plugin_id)) continue;
            PB::Settings::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
            cache.emplace(key);
            rpp->mutable_node()->set_path(path);
            rpp->mutable_node()->set_key(key);
            rpp->mutable_info()->set_title(desc.title);
            rpp->mutable_info()->set_description(desc.description);
            rpp->mutable_info()->set_advanced(desc.advanced);
            rpp->mutable_info()->set_sample(desc.is_sample);
            rpp->mutable_info()->set_default_value(desc.default_value);
            settings::settings_interface::op_string val = settings_manager::get_settings()->get_string(path, key);
            if (val) rpp->mutable_node()->set_value(*val);
            settings_add_plugin_data(desc.plugins, rpp->mutable_info());
          }
          if (!plugin_id) {
            t.start("fetching more keys");
            klist = settings_manager::get_settings()->get_keys(path);
            t.end();
            for (const std::string &key : klist) {
              if (cache.find(key) == cache.end()) {
                PB::Settings::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
                rpp->mutable_node()->set_path(path);
                rpp->mutable_node()->set_key(key);
                rpp->mutable_info()->set_advanced(true);
                rpp->mutable_info()->set_sample(false);
                rpp->mutable_info()->set_default_value("");
                settings::settings_interface::op_string val = settings_manager::get_settings()->get_string(path, key);
                if (val) rpp->mutable_node()->set_value(*val);
              }
            }
          }
        }
        if (q.fetch_paths()) {
          t.start("fetching path");
          settings::settings_core::path_description desc = settings_manager::get_core()->get_registered_path(path);
          t.end();
          PB::Settings::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
          rpp->mutable_node()->set_path(path);
          rpp->mutable_info()->set_title(desc.title);
          rpp->mutable_info()->set_description(desc.description);
          rpp->mutable_info()->set_advanced(desc.advanced);
          rpp->mutable_info()->set_sample(desc.is_sample);
          rpp->mutable_info()->set_subkey(desc.subkey.is_subkey);
          settings_add_plugin_data(desc.plugins, rpp->mutable_info());
        }
      }
    } else {
      std::string path = q.node().path();
      if (q.fetch_keys()) {
        t.start("fetching keys");
        std::list<std::string> list = settings_manager::get_core()->get_reg_keys(path, fetch_samples);
        t.end();
        boost::unordered_set<std::string> cache;
        for (const std::string &key : list) {
          t.start("fetching keys");
          settings::settings_core::key_description desc = settings_manager::get_core()->get_registered_key(path, key).get_value_or(settings::settings_core::key_description());
          if (plugin_id && !desc.has_plugin(*plugin_id)) continue;
          t.end();
          PB::Settings::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
          cache.emplace(key);
          rpp->mutable_node()->set_path(path);
          rpp->mutable_node()->set_key(key);
          rpp->mutable_info()->set_title(desc.title);
          rpp->mutable_info()->set_description(desc.description);
          rpp->mutable_info()->set_advanced(desc.advanced);
          rpp->mutable_info()->set_sample(desc.is_sample);
          rpp->mutable_info()->set_default_value(desc.default_value);
          try {
            rpp->mutable_node()->set_value(settings_manager::get_settings()->get_string(path, key, ""));
          } catch (settings::settings_exception &) {
          }
          settings_add_plugin_data(desc.plugins, rpp->mutable_info());
        }
        if (!plugin_id) {
          t.start("fetching more keys");
          list = settings_manager::get_settings()->get_keys(path);
          t.end();
          for (const std::string &key : list) {
            if (cache.find(key) == cache.end()) {
              PB::Settings::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
              rpp->mutable_node()->set_path(path);
              rpp->mutable_node()->set_key(key);
              rpp->mutable_info()->set_advanced(true);
              rpp->mutable_info()->set_sample(false);
              settings::settings_interface::op_string val = settings_manager::get_settings()->get_string(path, key);
              if (val) rpp->mutable_node()->set_value(*val);
            }
          }
        }
      }
      if (q.fetch_paths()) {
        t.start("fetching paths");
        settings::settings_core::path_description desc = settings_manager::get_core()->get_registered_path(path);
        t.end();
        PB::Settings::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
        rpp->mutable_node()->set_path(path);
        rpp->mutable_info()->set_title(desc.title);
        rpp->mutable_info()->set_description(desc.description);
        rpp->mutable_info()->set_advanced(desc.advanced);
        rpp->mutable_info()->set_sample(desc.is_sample);
        settings_add_plugin_data(desc.plugins, rpp->mutable_info());
      }
    }
    if (q.fetch_templates()) {
      t.start("fetching templates");
      for (const settings::settings_core::tpl_description &desc : settings_manager::get_core()->get_registered_templates()) {
        PB::Settings::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
        rpp->mutable_node()->set_path(desc.path);
        rpp->mutable_info()->set_title(desc.title);
        rpp->mutable_info()->set_is_template(true);
        rpp->mutable_node()->set_value(desc.data);
        rpp->mutable_info()->add_plugin(core_->get_plugin_cache()->find_plugin_alias(desc.plugin_id));
      }
      t.end();
    }
    rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  }
}

void settings_query_handler::recurse_find(PB::Settings::SettingsResponseMessage::Response::Query *rpp, const std::string base_path, bool recurse,
                                          bool fetch_keys) {
  std::string path = base_path;
  if (base_path.size() > 1 && base_path[base_path.size() - 1] == '/') {
    path = base_path.substr(0, base_path.size() - 1);
  }
  for (const std::string &child : settings_manager::get_settings()->get_sections(path)) {
    std::string child_path = settings::join_path(path, child);
    if (!fetch_keys) {
      PB::Settings::Node *node = rpp->add_nodes();
      node->set_path(child_path);
    }
    if (recurse) {
      recurse_find(rpp, child_path, true, fetch_keys);
    }
  }
  if (fetch_keys) {
    for (const std::string &key : settings_manager::get_settings()->get_keys(path)) {
      PB::Settings::Node *node = rpp->add_nodes();
      node->set_path(path);
      node->set_key(key);
      node->set_value(settings_manager::get_settings()->get_string(path, key, ""));
    }
  }
}

void settings_query_handler::parse_query(const PB::Settings::SettingsRequestMessage::Request::Query &q, PB::Settings::SettingsResponseMessage::Response *rp) {
  PB::Settings::SettingsResponseMessage::Response::Query *rpp = rp->mutable_query();
  rpp->mutable_node()->CopyFrom(q.node());
  if (!q.node().key().empty()) {
    std::string def = q.default_value().empty() ? "" : q.default_value();
    rpp->mutable_node()->set_value(settings_manager::get_settings()->get_string(q.node().path(), q.node().key(), def));
  } else {
    recurse_find(rpp, q.node().path(), q.recursive(), q.include_keys());
  }
  rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
}

void settings_query_handler::parse_registration(const PB::Settings::SettingsRequestMessage::Request::Registration &q, int plugin_id,
                                                PB::Settings::SettingsResponseMessage::Response *rp) {
  rp->mutable_registration();
  if (!q.fields().empty()) {
#ifdef HAVE_JSON_SPIRIT
    json_spirit::Object node;

    try {
      json_spirit::Value value;
      std::string data = q.fields();
      json_spirit::read_or_throw(data, value);
      if (value.isObject()) node = value.getObject();
    } catch (const std::exception &e) {
      LOG_ERROR_CORE(std::string("Failed to process fields for ") + e.what());
    } catch (const json_spirit::ParseError &e) {
      LOG_ERROR_CORE(std::string("Failed to process fields for ") + e.reason_ + " @ " + str::xtos(e.line_) + ":" + str::xtos(e.column_));
    } catch (...) {
      LOG_ERROR_CORE("Failed to process fields for ");
    }

    node.insert(json_spirit::Object::value_type("plugin", plugin_id));
    node.insert(json_spirit::Object::value_type("path", q.node().path()));
    node.insert(json_spirit::Object::value_type("title", q.info().title()));
    node.insert(json_spirit::Object::value_type("icon", q.info().icon()));
    node.insert(json_spirit::Object::value_type("description", q.info().description()));

    // node.insert(json_spirit::Object::value_type("fields", value));
    std::string tplData = json_spirit::write(node);
    settings_manager::get_core()->register_tpl(plugin_id, q.node().path(), q.info().title(), tplData);
#else
    LOG_ERROR_CORE("Not compiled with json support");
#endif
  } else if (!q.node().key().empty()) {
    settings_manager::get_core()->register_key(plugin_id, q.node().path(), q.node().key(), q.info().title(), q.info().description(), q.info().default_value(),
                                               q.info().advanced(), q.info().sample());
    if (q.info().is_sensitive()) {
      settings_manager::get_core()->add_sensitive_key(plugin_id, q.node().path(), q.node().key());
    }
  } else {
    if (q.info().subkey()) {
      settings_manager::get_core()->register_subkey(plugin_id, q.node().path(), q.info().title(), q.info().description(), q.info().advanced(),
                                                    q.info().sample());
      if (q.info().is_sensitive()) {
        LOG_ERROR_CORE("Sub-key cannot be sensitive: " + q.node().path());
      }
    } else {
      settings_manager::get_core()->register_path(plugin_id, q.node().path(), q.info().title(), q.info().description(), q.info().advanced(), q.info().sample());
      if (q.info().is_sensitive()) {
        LOG_ERROR_CORE("Paths cannot be sensitive: " + q.node().path());
      }
    }
  }
  rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
}

void settings_query_handler::parse_update(const PB::Settings::SettingsRequestMessage::Request::Update &p, PB::Settings::SettingsResponseMessage::Response *rp) {
  rp->mutable_update();
  if (!p.node().value().empty()) {
    settings_manager::get_settings()->set_string(p.node().path(), p.node().key(), p.node().value());
  } else if (!p.node().key().empty()) {
    settings_manager::get_settings()->remove_key(p.node().path(), p.node().key());
  } else {
    settings_manager::get_settings()->remove_path(p.node().path());
  }
  rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
}
void settings_query_handler::parse_control(const PB::Settings::SettingsRequestMessage::Request::Control &p,
                                           PB::Settings::SettingsResponseMessage::Response *rp) {
  rp->mutable_control();
  if (p.command() == PB::Settings::Command::LOAD) {
    if (!p.context().empty())
      settings_manager::get_core()->migrate_from("master", p.context());
    else
      settings_manager::get_settings()->load();
    settings_manager::get_settings()->reload();
    rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  } else if (p.command() == PB::Settings::Command::SAVE) {
    if (!p.context().empty())
      settings_manager::get_core()->migrate_to("master", p.context());
    else
      settings_manager::get_settings()->save(false);
    rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  } else {
    rp->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
    rp->mutable_result()->set_message("Unknown command");
  }
}

}  // namespace core
}  // namespace nsclient
