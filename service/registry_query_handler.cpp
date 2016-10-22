#include "registry_query_handler.hpp"

#include <nscapi/nscapi_protobuf_functions.hpp>


#include <boost/foreach.hpp>
#include <boost/unordered_set.hpp>

#ifdef WIN32
#include <shellapi.h>
#endif


boost::optional<boost::filesystem::path> locateFileICase(const boost::filesystem::path path, const std::string filename) {
	boost::filesystem::path fullpath = path / filename;
#ifdef WIN32
	std::wstring tmp = utf8::cvt<std::wstring>(fullpath.string());
	SHFILEINFOW sfi = { 0 };
	boost::replace_all(tmp, "/", "\\");
	HRESULT hr = SHGetFileInfo(tmp.c_str(), 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME);
	if (SUCCEEDED(hr)) {
		tmp = sfi.szDisplayName;
		boost::filesystem::path rpath = path / utf8::cvt<std::string>(tmp);
		return rpath;
	}
#else
	if (boost::filesystem::is_regular_file(fullpath))
		return fullpath;
	boost::filesystem::directory_iterator it(path), eod;
	std::string tmp = boost::algorithm::to_lower_copy(filename);
	BOOST_FOREACH(boost::filesystem::path const &p, std::make_pair(it, eod)) {
		if (boost::filesystem::is_regular_file(p) && boost::algorithm::to_lower_copy(file_helpers::meta::get_filename(p)) == tmp) {
			return p;
		}
	}
#endif
	return boost::optional<boost::filesystem::path>();
}

namespace nsclient {

	namespace core {


		registry_query_handler::registry_query_handler(nsclient::core::core_interface *core, const Plugin::RegistryRequestMessage &request)
			: logger_(core->get_logger())
			, request_(request)
			, core_(core) {}



		void registry_query_handler::parse(Plugin::RegistryResponseMessage &response) {
			nscapi::protobuf::functions::create_simple_header(response.mutable_header());

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

		void registry_query_handler::parse_inventory(const Plugin::RegistryRequestMessage::Request::Inventory &q, Plugin::RegistryResponseMessage &response) {
			Plugin::RegistryResponseMessage::Response* rp = response.add_payload();
			timer tme;
			tme.start("...");
			for (int i = 0; i < q.type_size(); i++) {
				Plugin::Registry_ItemType type = q.type(i);
				if (type == Plugin::Registry_ItemType_QUERY || type == Plugin::Registry_ItemType_ALL) {
					if (q.has_name()) {
						nsclient::commands::command_info info = core_->get_commands()->describe(q.name());
						if (!info.name.empty()) {
							Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
							rpp->set_name(q.name());
							rpp->set_type(Plugin::Registry_ItemType_COMMAND);
							rpp->mutable_info()->add_plugin(core_->get_plugin_cache()->find_plugin_alias(info.plugin_id));
							rpp->mutable_info()->set_title(info.name);
							rpp->mutable_info()->set_description(info.description);
							if (q.has_fetch_all() && q.fetch_all()) {
								Plugin::QueryRequestMessage req;
								nscapi::protobuf::functions::create_simple_header(req.mutable_header());
								Plugin::QueryRequestMessage::Request * p = req.add_payload();
								p->set_command(q.name());
								p->add_arguments("help-pb");
								Plugin::QueryResponseMessage res = core_->execute_query(req);
								for (int i = 0; i < res.payload_size(); i++) {
									const Plugin::QueryResponseMessage::Response p = res.payload(i);
									rpp->mutable_parameters()->ParseFromString(p.data());
								}
							}
						}
					} else {
						BOOST_FOREACH(const std::string &command, core_->get_commands()->list_commands()) {
							nsclient::commands::command_info info = core_->get_commands()->describe(command);
							Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
							rpp->set_name(command);
							rpp->set_type(Plugin::Registry_ItemType_COMMAND);
							rpp->mutable_info()->add_plugin(core_->get_plugin_cache()->find_plugin_alias(info.plugin_id));
							rpp->mutable_info()->set_title(info.name);
							rpp->mutable_info()->set_description(info.description);
							if (q.has_fetch_all() && q.fetch_all()) {
								std::string resp;
								Plugin::QueryRequestMessage req;
								nscapi::protobuf::functions::create_simple_header(req.mutable_header());
								Plugin::QueryRequestMessage::Request * p = req.add_payload();
								p->set_command(command);
								p->add_arguments("help-pb");
								Plugin::QueryResponseMessage res = core_->execute_query(req);
								for (int i = 0; i < res.payload_size(); i++) {
									const Plugin::QueryResponseMessage::Response p = res.payload(i);
									rpp->mutable_parameters()->ParseFromString(p.data());
								}
							}
						}
					}
				}
				if (type == Plugin::Registry_ItemType_QUERY_ALIAS || type == Plugin::Registry_ItemType_ALL) {
					BOOST_FOREACH(const std::string &command, core_->get_commands()->list_aliases()) {
						nsclient::commands::command_info info = core_->get_commands()->describe(command);
						Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
						rpp->set_name(command);
						rpp->set_type(Plugin::Registry_ItemType_QUERY_ALIAS);
						rpp->mutable_info()->add_plugin(core_->get_plugin_cache()->find_plugin_alias(info.plugin_id));
						rpp->mutable_info()->set_title(info.name);
						rpp->mutable_info()->set_description(info.description);
					}
				}
				if (type == Plugin::Registry_ItemType_MODULE || type == Plugin::Registry_ItemType_ALL) {
					boost::unordered_set<std::string> cache;
					tme.start("enumerating loaded");
					BOOST_FOREACH(const nsclient::core::plugin_cache_item &plugin, core_->get_plugin_cache()->get_list()) {
						Plugin::RegistryResponseMessage::Response::Inventory *rpp = rp->add_inventory();
						cache.emplace(plugin.dll);
						rpp->set_name(plugin.dll);
						rpp->set_type(Plugin::Registry_ItemType_MODULE);
						rpp->set_id(plugin.alias);
						rpp->mutable_info()->add_plugin(plugin.dll);
						rpp->mutable_info()->set_title(plugin.name);
						rpp->mutable_info()->set_description(plugin.desc);
						Plugin::Common::KeyValue *kvp = rpp->mutable_info()->add_metadata();
						kvp->set_key("plugin_id");
						kvp->set_value(strEx::s::xtos(plugin.id));
						kvp = rpp->mutable_info()->add_metadata();
						kvp->set_key("loaded");
						kvp->set_value(plugin.is_loaded?"true":"false");
					}
					tme.end();
					if (!core_->get_plugin_cache()->has_all()) {
						tme.start("enumerating files");
						nsclient::core::plugin_cache::plugin_cache_list_type tmp_list;
						boost::filesystem::path pluginPath = core_->expand_path("${module-path}");
						boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
						for (boost::filesystem::directory_iterator itr(pluginPath); itr != end_itr; ++itr) {
							if (!is_directory(itr->status())) {
								boost::filesystem::path file = itr->path().filename();
								if (NSCPlugin::is_module(pluginPath / file)) {
									const std::string module = NSCPlugin::file_to_module(file);
									if (!core_->get_plugin_cache()->has_module(module)) {
										plugin_cache_item itm;
										try {
											boost::filesystem::path p = (pluginPath / file).normalize();
											LOG_DEBUG_CORE("Loading " + p.string());
											plugin_type plugin = plugin_type(new NSCPlugin(-1, p, ""));
											plugin->load_dll();
											itm.dll = plugin->getModule();
											itm.name = plugin->getName();
											itm.desc = plugin->getDescription();
											itm.is_loaded = false;
											tmp_list.push_back(itm);
											plugin->unload_dll();
										} catch (const std::exception &e) {
											LOG_DEBUG_CORE("Failed to load " + file.string() + ": " + utf8::utf8_from_native(e.what()));
											continue;
										}
										if (!itm.name.empty()) {
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
						core_->get_plugin_cache()->add_plugins(tmp_list);
						tme.end();
					}
				}
			}
			tme.end();
			BOOST_FOREACH(const std::string &s, tme.get()) {
				LOG_DEBUG_CORE(s);
			}
			rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
		}

		void registry_query_handler::parse_registration(const Plugin::RegistryRequestMessage::Request::Registration &registration, Plugin::RegistryResponseMessage &response) {
			Plugin::RegistryResponseMessage::Response* rp = response.add_payload();
			if (registration.type() == Plugin::Registry_ItemType_QUERY) {
				if (registration.unregister()) {
					core_->get_commands()->unregister_command(registration.plugin_id(), registration.name());
					BOOST_FOREACH(const std::string &alias, registration.alias())
						core_->get_commands()->unregister_command(registration.plugin_id(), alias);
				} else {
					core_->get_commands()->register_command(registration.plugin_id(), registration.name(), registration.info().description());
					std::string description = "Alternative name for: " + registration.name();
					BOOST_FOREACH(const std::string &alias, registration.alias())
						core_->get_commands()->register_alias(registration.plugin_id(), alias, description);
				}
			} else if (registration.type() == Plugin::Registry_ItemType_QUERY_ALIAS) {
				core_->get_commands()->register_alias(registration.plugin_id(), registration.name(), registration.info().description());
				for (int i = 0; i < registration.alias_size(); i++) {
					core_->get_commands()->register_alias(registration.plugin_id(), registration.alias(i), registration.info().description());
				}
			} else if (registration.type() == Plugin::Registry_ItemType_HANDLER) {
				core_->get_channels()->register_listener(registration.plugin_id(), registration.name());
			} else if (registration.type() == Plugin::Registry_ItemType_ROUTER) {
				core_->get_routers()->register_listener(registration.plugin_id(), registration.name());
			} else if (registration.type() == Plugin::Registry_ItemType_MODULE) {
				Plugin::RegistryResponseMessage::Response::Registration *rpp = rp->mutable_registration();
				unsigned int new_id = core_->add_plugin(registration.plugin_id());
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
					boost::filesystem::path pluginPath = core_->expand_path("${module-path}");
					boost::optional<boost::filesystem::path> module = locateFileICase(pluginPath, NSCPlugin::get_plugin_file(control.name()));
					if (!module)
						module = locateFileICase(boost::filesystem::path("./modules"), NSCPlugin::get_plugin_file(control.name()));
					if (!module) {
						LOG_ERROR_CORE("Failed to find: " + control.name());
					} else {
						LOG_DEBUG_CORE_STD("Module name: " + module->string());

						core_->load_plugin(*module, control.alias());
					}
				} else if (control.command() == Plugin::Registry_Command_UNLOAD) {
					core_->remove_plugin(control.name());
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
