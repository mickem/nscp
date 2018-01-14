#include "settings_query_handler.hpp"

#include "../libs/settings_manager/settings_manager_impl.h"

#include <nscapi/nscapi_protobuf_functions.hpp>

#ifdef HAVE_JSON_SPIRIT
#include <json_spirit.h>
#endif
#include <boost/foreach.hpp>
#include <boost/unordered_set.hpp>

namespace nsclient {

	namespace core {


		settings_query_handler::settings_query_handler(nsclient::core::core_interface *core, const Plugin::SettingsRequestMessage &request)
			: logger_(core->get_logger())
			, request_(request) 
			, core_(core)
		{
		}


		void settings_query_handler::settings_add_plugin_data(const std::set<unsigned int> &plugins, ::Plugin::Settings_Information* info) {
			BOOST_FOREACH(const unsigned int i, plugins) {
				info->add_plugin(core_->get_plugin_cache()->find_plugin_alias(i));
			}
		}


		void settings_query_handler::parse(Plugin::SettingsResponseMessage &response) {
			std::string response_string;

			BOOST_FOREACH(const Plugin::SettingsRequestMessage::Request &r, request_.payload()) {
				Plugin::SettingsResponseMessage::Response* rp = response.add_payload();
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
						rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
					} else {
						rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
						rp->mutable_result()->set_message("Settings error: Invalid action");
						LOG_ERROR_CORE_STD("Settings error: Invalid action");
					}
				} catch (settings::settings_exception &e) {
					rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
					rp->mutable_result()->set_message("Settings error: " + e.reason());
					LOG_ERROR_CORE_STD("Settings error: " + e.reason());
				} catch (const std::exception &e) {
					rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
					rp->mutable_result()->set_message("Settings error: " + utf8::utf8_from_native(e.what()));
					LOG_ERROR_CORE_STD("Settings error: " + utf8::utf8_from_native(e.what()));
				} catch (...) {
					rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
					rp->mutable_result()->set_message("Settings error");
					LOG_ERROR_CORE_STD("Settings error");
				}
			}
		}


		void settings_query_handler::parse_inventory(const Plugin::SettingsRequestMessage::Request::Inventory &q, Plugin::SettingsResponseMessage::Response* rp) {
			boost::optional<unsigned int> plugin_id;
			if (q.has_plugin()) {
				plugin_id = core_->get_plugin_cache()->find_plugin(q.plugin());
			}
			if (q.node().has_key()) {
				t.start("fetching key");
				settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(q.node().path(), q.node().key());
				t.end();
				Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
				rpp->mutable_node()->CopyFrom(q.node());
				rpp->mutable_info()->set_title(desc.title);
				rpp->mutable_info()->set_description(desc.description);
			} else {
				bool fetch_samples = q.fetch_samples();
				if (q.recursive_fetch()) {
					std::string base_path;
					if (q.node().has_path())
						base_path = q.node().path();
					t.start("fetching paths");
					std::list<std::string> list = settings_manager::get_core()->get_reg_sections(base_path, fetch_samples);
					t.end();
					BOOST_FOREACH(const std::string &path, list) {
						if (q.fetch_keys()) {
							t.start("fetching keys");
							std::list<std::string> klist = settings_manager::get_core()->get_reg_keys(path, fetch_samples);
							t.end();
							boost::unordered_set<std::string> cache;
							BOOST_FOREACH(const std::string &key, klist) {
								settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(path, key);
								if (plugin_id && !desc.has_plugin(*plugin_id))
									continue;
								Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
								cache.emplace(key);
								rpp->mutable_node()->set_path(path);
								rpp->mutable_node()->set_key(key);
								rpp->mutable_info()->set_title(desc.title);
								rpp->mutable_info()->set_description(desc.description);
								rpp->mutable_info()->set_advanced(desc.advanced);
								rpp->mutable_info()->set_sample(desc.is_sample);
								if (desc.defValue.string_value)
									rpp->mutable_info()->mutable_default_value()->set_string_data(*desc.defValue.string_value);
								if (desc.defValue.int_value)
									rpp->mutable_info()->mutable_default_value()->set_int_data(*desc.defValue.int_value);
								if (desc.defValue.bool_value)
									rpp->mutable_info()->mutable_default_value()->set_bool_data(*desc.defValue.bool_value);
								if (desc.type == NSCAPI::key_string) {
									settings::settings_interface::op_string val = settings_manager::get_settings()->get_string(path, key);
									if (val)
										rpp->mutable_value()->set_string_data(*val);
								} else if (desc.type == NSCAPI::key_integer) {
									settings::settings_interface::op_int val = settings_manager::get_settings()->get_int(path, key);
									if (val)
										rpp->mutable_value()->set_int_data(*val);
								} else if (desc.type == NSCAPI::key_bool) {
									settings::settings_interface::op_bool val = settings_manager::get_settings()->get_bool(path, key);
									if (val)
										rpp->mutable_value()->set_bool_data(*val);
								} else {
									LOG_ERROR_CORE("Invalid type");
								}
								settings_add_plugin_data(desc.plugins, rpp->mutable_info());
							}
							if (!plugin_id) {
								t.start("fetching more keys");
								klist = settings_manager::get_settings()->get_keys(path);
								t.end();
								BOOST_FOREACH(const std::string &key, klist) {
									if (cache.find(key) == cache.end()) {
										Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
										rpp->mutable_node()->set_path(path);
										rpp->mutable_node()->set_key(key);
										rpp->mutable_info()->set_advanced(true);
										rpp->mutable_info()->set_sample(false);
										rpp->mutable_info()->mutable_default_value()->set_string_data("");
										settings::settings_interface::op_string val = settings_manager::get_settings()->get_string(path, key);
										if (val)
											rpp->mutable_value()->set_string_data(*val);
									}
								}
							}
						}
						if (q.fetch_paths()) {
							t.start("fetching path");
							settings::settings_core::path_description desc = settings_manager::get_core()->get_registred_path(path);
							t.end();
							Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
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
						BOOST_FOREACH(const std::string &key, list) {
							t.start("fetching keys");
							settings::settings_core::key_description desc = settings_manager::get_core()->get_registred_key(path, key);
							if (plugin_id && !desc.has_plugin(*plugin_id))
								continue;
							t.end();
							Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
							cache.emplace(key);
							rpp->mutable_node()->set_path(path);
							rpp->mutable_node()->set_key(key);
							rpp->mutable_info()->set_title(desc.title);
							rpp->mutable_info()->set_description(desc.description);
							rpp->mutable_info()->set_advanced(desc.advanced);
							rpp->mutable_info()->set_sample(desc.is_sample);
							if (desc.defValue.string_value)
								rpp->mutable_info()->mutable_default_value()->set_string_data(*desc.defValue.string_value);
							if (desc.defValue.int_value)
								rpp->mutable_info()->mutable_default_value()->set_int_data(*desc.defValue.int_value);
							if (desc.defValue.bool_value)
								rpp->mutable_info()->mutable_default_value()->set_bool_data(*desc.defValue.bool_value);
							try {
								if (desc.type == NSCAPI::key_string)
									rpp->mutable_value()->set_string_data(settings_manager::get_settings()->get_string(path, key, ""));
								else if (desc.type == NSCAPI::key_integer)
									rpp->mutable_value()->set_int_data(settings_manager::get_settings()->get_int(path, key, 0));
								else if (desc.type == NSCAPI::key_bool)
									rpp->mutable_value()->set_bool_data(settings_manager::get_settings()->get_bool(path, key, false));
								else {
									LOG_ERROR_CORE("Invalid type");
								}
							} catch (settings::settings_exception &) {}
							settings_add_plugin_data(desc.plugins, rpp->mutable_info());
						}
						if (!plugin_id) {
							t.start("fetching more keys");
							list = settings_manager::get_settings()->get_keys(path);
							t.end();
							BOOST_FOREACH(const std::string &key, list) {
								if (cache.find(key) == cache.end()) {
									Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
									rpp->mutable_node()->set_path(path);
									rpp->mutable_node()->set_key(key);
									rpp->mutable_info()->set_advanced(true);
									rpp->mutable_info()->set_sample(false);
									settings::settings_interface::op_string val = settings_manager::get_settings()->get_string(path, key);
									if (val)
										rpp->mutable_value()->set_string_data(*val);
								}
							}
						}
					}
					if (q.fetch_paths()) {
						t.start("fetching paths");
						settings::settings_core::path_description desc = settings_manager::get_core()->get_registred_path(path);
						t.end();
						Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
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
					BOOST_FOREACH(const settings::settings_core::tpl_description &desc, settings_manager::get_core()->get_registred_tpls()) {
						Plugin::SettingsResponseMessage::Response::Inventory *rpp = rp->add_inventory();
						rpp->mutable_node()->set_path(desc.path);
						rpp->mutable_info()->set_title(desc.title);
						rpp->mutable_info()->set_is_template(true);
						rpp->mutable_value()->set_string_data(desc.data);
						rpp->mutable_info()->add_plugin(core_->get_plugin_cache()->find_plugin_alias(desc.plugin_id));
					}
					t.end();
				}
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			}
		}


		void settings_query_handler::parse_query(const Plugin::SettingsRequestMessage::Request::Query &q, Plugin::SettingsResponseMessage::Response* rp) {
			Plugin::SettingsResponseMessage::Response::Query *rpp = rp->mutable_query();
			rpp->mutable_node()->CopyFrom(q.node());
			if (q.node().has_key()) {
				if (q.has_type() && q.type() == Plugin::Common_DataType_STRING) {
					std::string def = q.has_default_value() && q.default_value().has_string_data() ? q.default_value().string_data() : "";
					rpp->mutable_value()->set_string_data(settings_manager::get_settings()->get_string(q.node().path(), q.node().key(), def));
				} else if (q.has_type() && q.type() == Plugin::Common_DataType_INT) {
					long long def = q.has_default_value() && q.default_value().has_int_data() ? q.default_value().int_data() : 0;
					rpp->mutable_value()->set_int_data(settings_manager::get_settings()->get_int(q.node().path(), q.node().key(), def));
				} else if (q.has_type() && q.type() == Plugin::Common_DataType_BOOL) {
					bool def = q.has_default_value() && q.default_value().has_bool_data() ? q.default_value().bool_data() : false;
					rpp->mutable_value()->set_bool_data(settings_manager::get_settings()->get_bool(q.node().path(), q.node().key(), def));
				} else {
					std::string def = q.has_default_value() && q.default_value().has_string_data() ? q.default_value().string_data() : "";
					rpp->mutable_value()->set_string_data(settings_manager::get_settings()->get_string(q.node().path(), q.node().key(), def));
				}
			} else {
				::Plugin::Common::AnyDataType *value = rpp->mutable_value();
				if (q.has_recursive() && q.recursive()) {
					BOOST_FOREACH(const std::string &key, settings_manager::get_settings()->get_sections(q.node().path())) {
						value->add_list_data(key);
					}
				} else {
					BOOST_FOREACH(const std::string &key, settings_manager::get_settings()->get_keys(q.node().path())) {
						value->add_list_data(key);
					}
				}
			}
			rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
		}


		void settings_query_handler::parse_registration(const Plugin::SettingsRequestMessage::Request::Registration &q, int plugin_id, Plugin::SettingsResponseMessage::Response* rp) {
			rp->mutable_registration();
			if (q.has_fields()) {
#ifdef HAVE_JSON_SPIRIT
				json_spirit::Object node;

				try {
					json_spirit::Value value;
					std::string data = q.fields();
					json_spirit::read_or_throw(data, value);
					if (value.isObject())
						node = value.getObject();
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

				//node.insert(json_spirit::Object::value_type("fields", value));
				std::string tplData = json_spirit::write(node);
				settings_manager::get_core()->register_tpl(plugin_id, q.node().path(), q.info().title(), tplData);
#else
				LOG_ERROR_CORE("Not compiled with json support");
#endif
			} else if (q.node().has_key()) {
				nscapi::settings::settings_value defValue;
				if (q.info().default_value().has_string_data())
					defValue = nscapi::settings::settings_value::make_string(q.info().default_value().string_data());
				else if (q.info().default_value().has_int_data())
					defValue = nscapi::settings::settings_value::make_int(q.info().default_value().int_data());
				else if (q.info().default_value().has_bool_data())
					defValue = nscapi::settings::settings_value::make_bool(q.info().default_value().bool_data());
				settings_manager::get_core()->register_key(plugin_id, q.node().path(), q.node().key(), settings::settings_core::key_string, q.info().title(), q.info().description(), defValue, q.info().advanced(), q.info().sample());
			} else {
				if (q.info().subkey()) {
					settings_manager::get_core()->register_subkey(plugin_id, q.node().path(), q.info().title(), q.info().description(), q.info().advanced(), q.info().sample());
				} else {
					settings_manager::get_core()->register_path(plugin_id, q.node().path(), q.info().title(), q.info().description(), q.info().advanced(), q.info().sample());
				}
			}
			rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
		}


		void settings_query_handler::parse_update(const Plugin::SettingsRequestMessage::Request::Update &p, Plugin::SettingsResponseMessage::Response* rp) {
			rp->mutable_update();
			if (p.has_value() && p.value().has_string_data()) {
				settings_manager::get_settings()->set_string(p.node().path(), p.node().key(), p.value().string_data());
			} else if (p.has_value() && p.value().has_bool_data()) {
				settings_manager::get_settings()->set_bool(p.node().path(), p.node().key(), p.value().bool_data());
			} else if (p.has_value() && p.value().has_int_data()) {
				settings_manager::get_settings()->set_int(p.node().path(), p.node().key(), p.value().int_data());
			} else {
				if (p.node().has_key()) {
					settings_manager::get_settings()->remove_key(p.node().path(), p.node().key());
				} else {
					settings_manager::get_settings()->remove_path(p.node().path());
				}
			}
			rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
		}
		void settings_query_handler::parse_control(const Plugin::SettingsRequestMessage::Request::Control &p, Plugin::SettingsResponseMessage::Response* rp) {
			rp->mutable_control();
			if (p.command() == Plugin::Settings_Command_LOAD) {
				if (p.has_context() && p.context().size() > 0)
					settings_manager::get_core()->migrate_from("master", p.context());
				else
					settings_manager::get_settings()->load();
				settings_manager::get_settings()->reload();
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			} else if (p.command() == Plugin::Settings_Command_SAVE) {
				if (p.has_context() && p.context().size() > 0)
					settings_manager::get_core()->migrate_to("master", p.context());
				else
					settings_manager::get_settings()->save();
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
			} else {
				rp->mutable_result()->set_code(Plugin::Common_Result_StatusCodeType_STATUS_OK);
				rp->mutable_result()->set_message("Unknown command");
			}
		}

	}
}


