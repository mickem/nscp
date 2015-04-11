#pragma once

#include <map>
#include <list>
#include <string>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/unordered_map.hpp>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>

#include <settings/client/settings_client_interface.hpp>
#include <nscapi/nscapi_settings_helper.hpp>

#include <nscapi/dll_defines.hpp>

namespace nscapi {

	namespace settings_objects {

		inline void import_string(std::string &object, const std::string &parent) {
			if (object.empty() && !parent.empty())
				object = parent;
		}


		typedef boost::unordered_map<std::string, std::string> options_map;

		struct object_instance_interface {
			typedef boost::unordered_map<std::string, std::string> options_map;

			std::string path;
			std::string alias;
			std::string value;
			std::string parent;
			options_map options;
			bool is_template;
			object_instance_interface(std::string alias, std::string path) : alias(alias), path(path), is_template(false), parent("default") {}

			const options_map& get_options() const {
				return options;
			}
			//void read_object(nscapi::settings_helper::path_extension &root_path);
			//void add_oneliner_hint(boost::shared_ptr<nscapi::settings_proxy> proxy, const bool oneliner, const bool is_sample);
			std::string to_string() const;

			bool is_default() const {
				return alias == "default";
			}

			virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample);
			virtual void post_process_object() {}

			virtual void translate(const std::string &key, const std::string &value) {
				options[key] = value;
			}

			bool has_option(std::string key) const {
				return options.find(key) != options.end();
			}
			void set_property_int(std::string key, int value) {
				translate(key, strEx::s::xtos(value));
			}
			void set_property_bool(std::string key, bool value) {
				translate(key, value ? "true" : "false");
			}
			void set_property_string(std::string key, std::string value) {
				translate(key, value);
			}


		};
		typedef boost::shared_ptr<object_instance_interface> object_instance;


		struct object_factory_interface {
			virtual object_instance create(std::string alias, std::string path) = 0;
		};

		typedef boost::shared_ptr<object_factory_interface> object_factory;

		struct object_handler : boost::noncopyable {
			typedef boost::optional<object_instance> optional_object;
			typedef boost::unordered_map<std::string, object_instance> object_map;

			object_map objects;
			object_map templates;
			object_factory factory;
			std::string path;

			void set_path(std::string path_) {
				path = path_;
			}

			void add_missing(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string alias, std::string value, bool is_template = false) {
				if (has_object(alias))
					return;
				add(proxy, alias, value, is_template);
			}

			void add_samples(boost::shared_ptr<nscapi::settings_proxy> proxy) {
				object_instance tmp = factory->create("sample", path + "/sample");
				tmp->read(proxy, false, true);
			}

			std::list<object_instance> get_object_list() const {
				std::list<object_instance> ret;
				BOOST_FOREACH(const object_map::value_type &t, objects) {
					ret.push_back(t.second);
				}
				return ret;
			}
			bool has_objects() const {
				return !objects.empty();
			}

			void ensure_default() {
				if (has_object("default"))
					return;
				add(boost::shared_ptr<nscapi::settings_proxy>(), "default", "");
			}

			object_instance add(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string alias, std::string value, bool is_template = false) {
				optional_object previous = find_object(alias);
				if (previous) {
					object_instance p = *previous;
					return p;
				}
				object_instance object = factory->create(alias, path + "/" + alias);

				//if (object->is_default())
				//	object->init_default();
				if (proxy) {
					std::list<std::string> keys = proxy->get_keys(object->path);
					object->read(proxy, keys.empty(), false);
				}
				/*
				if (!object.tpl.parent.empty() && object.tpl.parent != alias && object.tpl.parent != object.tpl.alias) {
					t_object_type parent;
					optional_object tmp = find_object(object.tpl.parent);
					if (!tmp) {
						parent = add(proxy, path, object.tpl.parent, "", true);
					} else {
						parent = *tmp;
					}
					object_reader::apply_parent(object, parent);
				}
				*/
				if (is_template || object->is_template)
					add_template(object);
				else
					add_object(object);
				return object;
			}

			void materialize() {
				ensure_default();
				// TODO: Parse all parents and build objects here
			}

			/*
			void rebuild(boost::shared_ptr<nscapi::settings_proxy> proxy) {
				std::list<t_object_type> tmp;
				BOOST_FOREACH(const typename object_list_type::value_type &t, object_list) {
					tmp.push_back(t.second);
				}
				object_list.clear();
				BOOST_FOREACH(const t_object_type &o, tmp) {
					std::string::size_type pos = o.path.tpl.find_last_of("/");
					if (pos == std::string::npos)
						continue;
					add(proxy, o.path.tpl.substr(0, pos-1), o.path.tpl.substr(pos), o.host);
				}
			}
			*/

			optional_object find_object(const std::string alias) const {
				object_map::const_iterator cit = objects.find(alias);
				if (cit != objects.end())
					return optional_object(cit->second);
				cit = templates.find(alias);
				if (cit != templates.end())
					return optional_object(cit->second);
				return optional_object();
			}

			bool has_object(std::string alias) const {
				object_map::const_iterator cit = objects.find(alias);
				if (cit != objects.end())
					return true;
				cit = templates.find(alias);
				if (cit != templates.end())
					return true;
				return false;
			}

			bool empty() const {
				return objects.empty();
			}

			void clear() {
				objects.clear();
				templates.clear();
			}


			std::string to_string() {
				std::stringstream ss;
				ss << "Objects: ";
				BOOST_FOREACH(const object_map::value_type &t, objects) {
					ss << ", " << t.first << " = {" << t.second->to_string() + "} ";
				}
				ss << "Templates: ";
				BOOST_FOREACH(const object_map::value_type &t, templates) {
					ss << ", " << t.first << " = {" << t.second->to_string() + "} ";
				}
				return ss.str();
			}

			void add_object(object_instance object) {
				object->post_process_object();
				/*
				typename object_list_type::iterator cit = templates.find(object.tpl.alias);
				if (cit != template_list.end())
					template_list.erase(cit);
				*/
				objects[object->alias] = object;
			}
			void add_template(object_instance object) {
				object_map::const_iterator cit = objects.find(object->alias);
				if (cit != objects.end())
					return;
				object->post_process_object();
				//object_reader::post_process_object(object);
				templates[object->alias] = object;
			}
		};
		/*
		struct NSCAPI_EXPORT template_object {

			template_object() : is_template(false)  {}
						
			std::string path;
			std::string alias;
			std::string value;
			std::string parent;
			bool is_template;

			bool is_default() const {
				return alias == "default";
			}


			void read_object(nscapi::settings_helper::path_extension &root_path);
			void add_oneliner_hint(boost::shared_ptr<nscapi::settings_proxy> proxy, const bool oneliner, const bool is_sample);
			std::string to_string() const;
		};
		*/
	}
}

