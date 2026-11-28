// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "storage_query_handler.hpp"

#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_exec.hpp>

#include <str/xtos.hpp>

#include "plugins/dll_plugin.h"

namespace nsclient {

namespace core {

storage_query_handler::storage_query_handler(nsclient::core::storage_manager_instance storage_, nsclient::core::plugin_mgr_instance plugins_,
                                             nsclient::logging::logger_instance logger_, const PB::Storage::StorageRequestMessage &request)
    : storage_(storage_), plugins_(plugins_), logger_(logger_), request_(request) {}

void storage_query_handler::parse(PB::Storage::StorageResponseMessage &response) {
  for (const PB::Storage::StorageRequestMessage::Request &r : request_.payload()) {
    // plugin_id, not id: id is a request correlation number nobody sets, and
    // reading it resolved every request to the plugin with id 0 - so every
    // module shared one owner and could overwrite another's entries.
    if (r.has_get()) {
      parse_get(r.plugin_id(), r.get(), response);
    } else if (r.has_put()) {
      parse_put(r.plugin_id(), r.put(), response);
    } else {
      LOG_ERROR_CORE("Storage query: Unsupported action");
    }
  }
}

std::string storage_query_handler::resolve_owner(const long long plugin_id) {
  if (plugin_id < 0) {
    return "";
  }
  nsclient::core::plugin_manager::plugin_type plugin = plugins_->find_plugin(static_cast<unsigned int>(plugin_id));
  if (!plugin) {
    return "";
  }
  return plugin->get_alias_or_name();
}

void storage_query_handler::parse_get(const long long plugin_id, const PB::Storage::StorageRequestMessage::Request::Get &q,
                                      PB::Storage::StorageResponseMessage &response) {
  PB::Storage::StorageResponseMessage::Response *payload = response.add_payload();
  const std::string plugin_name = resolve_owner(plugin_id);
  if (plugin_name.empty()) {
    // An unresolved caller would otherwise read the legacy entries every
    // module is about to claim (owner "" is where they are held).
    LOG_ERROR_CORE("Storage query: unknown plugin id " + str::xtos(plugin_id) + " reading " + q.context());
    payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);
    payload->mutable_result()->set_message("Unknown plugin id: " + str::xtos(plugin_id));
    return;
  }
  storage_manager::entry_list entries = storage_->get(plugin_name, q.context());
  if (entries.empty()) {
    // Nothing of its own yet: take over what an older agent stored for this
    // context before entries were kept per module.
    entries = storage_->claim_legacy(plugin_name, q.context());
  }
  for (const ::PB::Storage::Storage_Entry &e : entries) {
    payload->mutable_get()->add_entry()->CopyFrom(e);
  }
}
void storage_query_handler::parse_put(const long long plugin_id, const PB::Storage::StorageRequestMessage::Request::Put &q,
                                      PB::Storage::StorageResponseMessage &response) {
  const std::string plugin_name = resolve_owner(plugin_id);
  if (plugin_name.empty()) {
    LOG_ERROR_CORE("Storage query: unknown plugin id " + str::xtos(plugin_id) + " writing " + q.entry().context());
    PB::Storage::StorageResponseMessage::Response *payload = response.add_payload();
    payload->mutable_put()->set_context(q.entry().context());
    payload->mutable_put()->set_key(q.entry().key());
    payload->mutable_put()->set_error("Unknown plugin id");
    payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);
    payload->mutable_result()->set_message("Unknown plugin id: " + str::xtos(plugin_id));
    return;
  }
  storage_->put(plugin_name, q.entry());
  // A plugin asking for a flush wants this entry on disk now rather than at
  // the next shutdown, because a kill or a power loss in between would lose
  // it. save() is serialised against other savers and takes only a read lock
  // on the store, so this is safe to call from a plugin's own thread.
  if (q.flush()) {
    storage_->save();
  }
}

plugin_cache_item storage_query_handler::inventory_plugin_on_disk(nsclient::core::plugin_cache::plugin_cache_list_type &list, std::string plugin) {
  plugin_cache_item itm;
  try {
    bool loaded = false;
    plugin_type instance = plugins_->probe_module(plugin, loaded);
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
