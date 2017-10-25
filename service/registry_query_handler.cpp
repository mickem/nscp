#include "registry_query_handler.hpp"

#include <nscapi/nscapi_protobuf_functions.hpp>


#include <boost/foreach.hpp>

namespace nsclient {

	namespace core {


		registry_query_handler::registry_query_handler(nsclient::core::path_instance path_, nsclient::core::plugin_mgr_instance plugins_, nsclient::logging::logger_instance logger_, const Plugin::RegistryRequestMessage &request)
			: path_(path_)
			, plugins_(plugins_)
			, logger_(logger_)
			, request_(request)
		{}



		void registry_query_handler::parse(Plugin::RegistryResponseMessage &response) {

			BOOST_FOREACH(const Plugin::RegistryRequestMessage::Request &r, request_.payload()) {
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

		void registry_query_handler::inventory_queries(const Plugin::RegistryRequestMessage::Request::Inventory &q, Plugin::RegistryResponseMessage::Response* rp) {
			if (q.has_name()) {
				nsclient::commands::command_info info = plugins_->get_commands()->describe(q.name());
				if (!info.name.empty()) {
					Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
					rpp->set_name(q.name());
					rpp->set_type(Plugin::Registry_ItemType_COMMAND);
					rpp->mutable_info()->add_plugin(plugins_->get_plugin_cache()->find_plugin_alias(info.plugin_id));
					rpp->mutable_info()->set_title(info.name);
					rpp->mutable_info()->set_description(info.description);
					if (q.has_fetch_all() && q.fetch_all()) {
						Plugin::QueryRequestMessage req;
						Plugin::QueryRequestMessage::Request * p = req.add_payload();
						p->set_command(q.name());
						p->add_arguments("help-pb");
						Plugin::QueryResponseMessage res = plugins_->execute_query(req);
						for (int i = 0; i < res.payload_size(); i++) {
							const Plugin::QueryResponseMessage::Response p = res.payload(i);
							rpp->mutable_parameters()->ParseFromString(p.data());
						}
					}
				}
			} else {
				BOOST_FOREACH(const std::string &command, plugins_->get_commands()->list_commands()) {
					nsclient::commands::command_info info = plugins_->get_commands()->describe(command);
					Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
					rpp->set_name(command);
					rpp->set_type(Plugin::Registry_ItemType_COMMAND);
					rpp->mutable_info()->add_plugin(plugins_->get_plugin_cache()->find_plugin_alias(info.plugin_id));
					rpp->mutable_info()->set_title(info.name);
					rpp->mutable_info()->set_description(info.description);
					if (q.has_fetch_all() && q.fetch_all()) {
						std::string resp;
						Plugin::QueryRequestMessage req;
						Plugin::QueryRequestMessage::Request * p = req.add_payload();
						p->set_command(command);
						p->add_arguments("help-pb");
						Plugin::QueryResponseMessage res = plugins_->execute_query(req);
						for (int i = 0; i < res.payload_size(); i++) {
							const Plugin::QueryResponseMessage::Response p = res.payload(i);
							rpp->mutable_parameters()->ParseFromString(p.data());
						}
					}
				}
			}
		}


		void registry_query_handler::find_plugins_on_disk(boost::unordered_set<std::string> &unique_instances, const Plugin::RegistryRequestMessage::Request::Inventory &q, Plugin::RegistryResponseMessage::Response* rp) {
			nsclient::core::plugin_cache::plugin_cache_list_type tmp_list;
			boost::filesystem::path pluginPath = path_->expand_path("${module-path}");
			boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
			for (boost::filesystem::directory_iterator itr(pluginPath); itr != end_itr; ++itr) {
				if (!is_directory(itr->status())) {
					boost::filesystem::path file = itr->path().filename();
					if (NSCPlugin::is_module(pluginPath / file)) {
						const std::string module = NSCPlugin::file_to_module(file);
						if (!plugins_->get_plugin_cache()->has_module(module)) {
							plugin_cache_item itm;
							try {
								boost::filesystem::path p = (pluginPath / file).normalize();
								LOG_DEBUG_CORE("Loading " + p.string());
								plugin_type plugin = plugin_type(new NSCPlugin(-1, p, ""));
								plugin->load_dll();
								itm.dll = plugin->getModule();
								itm.alias = itm.dll;
								itm.name = plugin->getName();
								itm.desc = plugin->getDescription();
								itm.id = plugin->get_id();
								itm.is_loaded = false;
								tmp_list.push_back(itm);
								plugin->unload_dll();
							} catch (const std::exception &e) {
								LOG_DEBUG_CORE("Failed to load " + file.string() + ": " + utf8::utf8_from_native(e.what()));
								continue;
							} catch (...) {
								LOG_DEBUG_CORE("Failed to load " + file.string() + ": UNKNOWN EXCEPTION");
								continue;
							}
							if (!itm.name.empty()) {
								std::string key = itm.dll + "::" + itm.alias;
								if (unique_instances.find(key) != unique_instances.end()) {
									continue;
								}
								unique_instances.emplace(key);
								if (q.has_name() && q.name() != itm.name) {
									continue;
								}
								Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
								rpp->set_name(itm.name);
								rpp->set_type(Plugin::Registry_ItemType_MODULE);
								rpp->mutable_info()->set_title(itm.title);
								rpp->mutable_info()->set_description(itm.desc);
								Plugin::Common::KeyValue *kvp = rpp->mutable_info()->add_metadata();
								kvp->set_key("loaded");
								kvp->set_value("false");
							}
						}
					}
				}
			}
			plugins_->get_plugin_cache()->add_plugins(tmp_list);
		}

		void registry_query_handler::inventory_modules(const Plugin::RegistryRequestMessage::Request::Inventory &q, Plugin::RegistryResponseMessage::Response* rp) {
			boost::unordered_set<std::string> unique_instances;
			BOOST_FOREACH(const nsclient::core::plugin_cache_item &plugin, plugins_->get_plugin_cache()->get_list()) {
				std::string key = plugin.dll + "::" + plugin.alias;
				if (unique_instances.find(key) != unique_instances.end()) {
					continue;
				}
				unique_instances.emplace(key);
				if (q.has_name() && q.name() != plugin.dll) {
					continue;
				}

				Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
				rpp->set_name(plugin.dll);
				rpp->set_type(Plugin::Registry_ItemType_MODULE);
				rpp->set_id(plugin.alias);
				rpp->mutable_info()->add_plugin(plugin.dll);
				rpp->mutable_info()->set_title(plugin.name);
				rpp->mutable_info()->set_description(plugin.desc);
				Plugin::Common::KeyValue *kvp = rpp->mutable_info()->add_metadata();
				kvp->set_key("plugin_id");
				kvp->set_value(str::xtos(plugin.id));
				kvp = rpp->mutable_info()->add_metadata();
				kvp->set_key("loaded");
				kvp->set_value(plugin.is_loaded ? "true" : "false");
			}
			if (!plugins_->get_plugin_cache()->has_all() && q.fetch_all()) {
				find_plugins_on_disk(unique_instances, q, rp);
			}
		}

		void registry_query_handler::parse_inventory(const Plugin::RegistryRequestMessage::Request::Inventory &q, Plugin::RegistryResponseMessage &response) {
			Plugin::RegistryResponseMessage::Response* rp = response.add_payload();
			for (int i = 0; i < q.type_size(); i++) {
				Plugin::Registry_ItemType type = q.type(i);
				if (type == Plugin::Registry_ItemType_QUERY || type == Plugin::Registry_ItemType_ALL) {
					inventory_queries(q, rp);
				}
				if (type == Plugin::Registry_ItemType_QUERY_ALIAS || type == Plugin::Registry_ItemType_ALL) {
					BOOST_FOREACH(const std::string &command, plugins_->get_commands()->list_aliases()) {
						nsclient::commands::command_info info = plugins_->get_commands()->describe(command);
						Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
						rpp->set_name(command);
						rpp->set_type(Plugin::Registry_ItemType_QUERY_ALIAS);
						rpp->mutable_info()->add_plugin(plugins_->get_plugin_cache()->find_plugin_alias(info.plugin_id));
						rpp->mutable_info()->set_title(info.name);
						rpp->mutable_info()->set_description(info.description);
					}
				}
				if (type == Plugin::Registry_ItemType_MODULE || type == Plugin::Registry_ItemType_ALL) {
					inventory_modules(q, rp);
				}
			}
			rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
		}

		void registry_query_handler::parse_registration(const Plugin::RegistryRequestMessage::Request::Registration &registration, Plugin::RegistryResponseMessage &response) {
			Plugin::RegistryResponseMessage::Response* rp = response.add_payload();
			if (registration.type() == Plugin::Registry_ItemType_QUERY) {
				if (registration.unregister()) {
					plugins_->get_commands()->unregister_command(registration.plugin_id(), registration.name());
					BOOST_FOREACH(const std::string &alias, registration.alias())
						plugins_->get_commands()->unregister_command(registration.plugin_id(), alias);
				} else {
					plugins_->get_commands()->register_command(registration.plugin_id(), registration.name(), registration.info().description());
					std::string description = "Alternative name for: " + registration.name();
					BOOST_FOREACH(const std::string &alias, registration.alias())
						plugins_->get_commands()->register_alias(registration.plugin_id(), alias, description);
				}
			} else if (registration.type() == Plugin::Registry_ItemType_QUERY_ALIAS) {
				plugins_->get_commands()->register_alias(registration.plugin_id(), registration.name(), registration.info().description());
				for (int i = 0; i < registration.alias_size(); i++) {
					plugins_->get_commands()->register_alias(registration.plugin_id(), registration.alias(i), registration.info().description());
				}
			} else if (registration.type() == Plugin::Registry_ItemType_HANDLER) {
				plugins_->get_channels()->register_listener(registration.plugin_id(), registration.name());
			} else if (registration.type() == Plugin::Registry_ItemType_EVENT) {
				plugins_->get_event_subscribers()->register_listener(registration.plugin_id(), registration.name());
			} else if (registration.type() == Plugin::Registry_ItemType_MODULE) {
				Plugin::RegistryResponseMessage::Response::Registration *rpp = rp->mutable_registration();
				unsigned int new_id = plugins_->add_plugin(registration.plugin_id());
				if (new_id != -1) {
					rpp->set_item_id(new_id);
				}
			} else {
				LOG_ERROR_CORE("Registration query: Unsupported type");
			}
			rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
		}


		void registry_query_handler::parse_control(const Plugin::RegistryRequestMessage::Request::Control &control, Plugin::RegistryResponseMessage &response) {
			Plugin::RegistryResponseMessage::Response* rp = response.add_payload();
			if (control.type() == Plugin::Registry_ItemType_MODULE) {
				if (control.command() == Plugin::Registry_Command_LOAD) {
					if (!plugins_->load_single_plugin(control.name(), control.alias(), true)) {
						LOG_ERROR_CORE("Failed to find: " + control.name());
					}
				} else if (control.command() == Plugin::Registry_Command_UNLOAD) {
					plugins_->remove_plugin(control.name());
				} else {
					LOG_ERROR_CORE("Registration query: Invalid command");
				}
			} else {
				LOG_ERROR_CORE("Registration query: Unsupported type");
			}
			rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
		}


	}
}
