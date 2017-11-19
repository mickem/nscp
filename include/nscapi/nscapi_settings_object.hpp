/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/dll_defines.hpp>

#include <settings/client/settings_client_interface.hpp>
#include <nsclient/nsclient_exception.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
#include <boost/unordered_map.hpp>
#include <boost/make_shared.hpp>

#include <list>
#include <string>

namespace nscapi {
	namespace settings_objects {
		inline void import_string(std::string &object, const std::string &parent) {
			if (object.empty() && !parent.empty())
				object = parent;
		}

		inline std::string make_obj_path(const std::string &base_path, const std::string &alias) {
			return base_path + "/" + alias;
		}

		typedef boost::unordered_map<std::string, std::string> options_map;

		struct object_instance_interface {
			typedef boost::unordered_map<std::string, std::string> options_map;

		private:
			std::string alias;
			std::string base_path;
			std::string path;
			bool is_template_;
			std::string parent;
			std::string value;
			options_map options;

		public:


			object_instance_interface(std::string alias, std::string base_path)
				: alias(alias)
				, base_path(base_path)
				, path(make_obj_path(base_path, alias))
				, is_template_(false)
				, parent("default") {}
			object_instance_interface(const boost::shared_ptr<object_instance_interface> other, std::string alias, std::string base_path)
				: alias(alias)
				, base_path(base_path)
				, path(make_obj_path(base_path, alias))
				, is_template_(false)
				, parent(other->alias) {
				value = other->value;
				BOOST_FOREACH(const options_map::value_type &e, other->options) {
					options.insert(e);
				}
			}
			object_instance_interface(const object_instance_interface &other)
				: alias(other.alias)
				, base_path(other.base_path)
				, path(other.path)
				, is_template_(other.is_template_)
				, parent(other.parent)
				, value(other.value)
				, options(other.options) {}

			bool is_template() const {
				return is_template_;
			}
			void make_template(bool tpl) {
				is_template_ = tpl;
			}
			void setup(std::string inAlias, std::string inPath) {
				alias = inAlias;
				path = inPath + "/" + inAlias;
				base_path = inPath;
			}
			const options_map& get_options() const {
				return options;
			}
			virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
				nscapi::settings_helper::settings_registry settings(proxy);
				if (oneliner) {
					parent = "default";
					make_template(false);
					nscapi::settings_helper::path_extension root_path = settings.path(base_path);
					root_path.add_key()
						(alias, nscapi::settings_helper::string_key(&value),
							alias, std::string("To configure this create a section under: ") + path, false)
						;
				} else {
					nscapi::settings_helper::path_extension root_path = settings.path(path);
					root_path.add_key()
						("parent", nscapi::settings_helper::string_key(&parent, "default"),
							"PARENT", "The parent the target inherits from", true)

						("is template", nscapi::settings_helper::bool_key(&is_template_, false),
							"IS TEMPLATE", "Declare this object as a template (this means it will not be available as a separate object)", true)

						("alias", nscapi::settings_helper::string_key(&alias),
							"ALIAS", "The alias (service name) to report to server", true)
						;
				}
				settings.register_all();
				settings.notify();
			}
			std::string get_path() const {
				return path;
			}
			std::string get_alias() const {
				return alias;
			}
			void set_alias(const std::string &s) {
				alias = s;
			}

			std::string get_base_path() const {
				return base_path;
			}

			virtual std::string to_string() const {
				std::stringstream ss;
				ss << "{alias: " << alias
					<< ", path: " << path
					<< ", is_tpl: " << (is_template_ ? "true" : "false")
					<< ", parent: " << parent
					<< ", value: " << value
					<< ", options : { ";
				BOOST_FOREACH(options_map::value_type e, options) {
					ss << e.first << "=" << e.second << ", ";
				}
				ss << "} }";
				return ss.str();
			}
			bool is_default() const {
				return alias == "default";
			}

			// VIrtual interface

			virtual void translate(const std::string &key, const std::string &value) {
				options[key] = value;
			}

			virtual void import(boost::shared_ptr<object_instance_interface> parent) {}

			// Accessors

			bool has_option(std::string key) const {
				return options.find(key) != options.end();
			}
			void set_property_int(std::string key, int value) {
				translate(key, str::xtos(value));
			}
			void set_property_bool(std::string key, bool value) {
				translate(key, value ? "true" : "false");
			}
			void set_property_string(std::string key, std::string value) {
				translate(key, value);
			}
			int get_property_int(std::string key, int value) {
				options_map::const_iterator cit = options.find(key);
				if (cit == options.end())
					return value;
				return str::stox<int>(cit->second);
			}
			bool get_property_bool(std::string key, bool value) {
				options_map::const_iterator cit = options.find(key);
				if (cit == options.end())
					return value;
				return cit->second == "true";
			}
			std::string get_property_string(std::string key, std::string value = "") {
				options_map::const_iterator cit = options.find(key);
				if (cit == options.end())
					return value;
				return cit->second;
			}

			std::string get_value() const { return value; }
			void set_value(const std::string &new_value) {
				value = new_value;
			}
		};

		typedef boost::shared_ptr<object_instance_interface> object_instance;

		template<class T>
		struct object_factory_interface {
			typedef boost::shared_ptr<T> object_instance;
			virtual object_instance create(std::string alias, std::string path) = 0;
			virtual object_instance clone(object_instance parent, const std::string alias, const std::string path) = 0;
		};

		template<class T>
		struct simple_object_factory : public object_factory_interface<T> {
			typedef boost::shared_ptr<T> object_instance;
			object_instance create(std::string alias, std::string path) {
				return boost::make_shared<T>(alias, path);
			}
			object_instance clone(object_instance parent, const std::string alias, const std::string path) {
				object_instance inst = boost::make_shared<T>(*parent);
				if (inst) {
					inst->setup(alias, path);
				}
				return inst;
			}
		};

		template<class T, class TFactory = simple_object_factory<T> >
		struct object_handler : boost::noncopyable {
			typedef boost::shared_ptr<T> object_instance;
			typedef boost::unordered_map<std::string, object_instance> object_map;
			typedef std::list<object_instance> object_list_type;

			object_map objects;
			object_map templates;
			boost::shared_ptr<TFactory> factory;
			std::string path;

			object_handler() : factory(boost::make_shared<TFactory>()) {}
			object_handler(boost::shared_ptr<TFactory> factory) : factory(factory) {}

			void set_path(std::string path_) {
				path = path_;
			}

			void add_missing(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string alias, std::string value) {
				if (has_object(alias))
					return;
				add(proxy, alias, value);
			}

			void add_samples(boost::shared_ptr<nscapi::settings_proxy> proxy) {
				object_instance tmp = factory->create("sample", path);
				tmp->read(proxy, false, true);
			}

			std::list<object_instance> get_object_list() const {
				std::list<object_instance> ret;
				BOOST_FOREACH(const typename object_map::value_type &t, objects) {
					ret.push_back(t.second);
				}
				return ret;
			}
			std::list<std::string> get_alias_list() const {
				std::list<std::string> ret;
				BOOST_FOREACH(const typename object_map::value_type &t, objects) {
					ret.push_back(t.first);
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

			object_instance add(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string alias, std::string value, bool force_template = false) {
				bool is_template = (alias == "default") || force_template;
				object_instance previous = find_object(alias);
				if (previous) {
					return previous;
				}

				object_instance object;
				if (proxy) {
					std::list<std::string> keys = proxy->get_keys(make_obj_path(path, alias));
					std::string parent_name = proxy->get_string(make_obj_path(path, alias), "parent", "default");
					if (!parent_name.empty() && parent_name != alias) {
						object_instance parent;
						if (has_object(parent_name))
							parent = find_object(parent_name);
 						else
 							parent = add(proxy, parent_name, "", true);
						if (parent) {
							object = factory->clone(parent, alias, path);
							object->make_template(false);
						} else {
							throw nsclient::nsclient_exception("Failed to create settings object for: " + alias);
						}
					} else {
						object = factory->create(alias, path);
					}
					object->set_value(value);
					object->read(proxy, keys.empty()&&alias != "default", false);
				} else {
					object = factory->create(alias, path);
					object->set_value(value);
				}
				if (is_template || object->is_template()) {
					add_template(object);
					if (alias != object->get_alias())
						add_template(alias, object);
				} else
					add_object(object);
				return object;
			}

			void materialize() {
				ensure_default();
			}

			bool remove(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string alias) {
				proxy->remove_path(make_obj_path(path, alias));
				proxy->remove_key(path, alias);
				return remove(alias);
			}
			bool remove(const std::string alias) {
				typename object_map::const_iterator cit = objects.find(alias);
				if (cit != objects.end()) {
					objects.erase(cit);
					return true;
				}
				cit = templates.find(alias);
				if (cit != templates.end()) {
					templates.erase(cit);
					return true;
				}
				return false;
			}

			object_instance find_object(const std::string alias) const {
				typename object_map::const_iterator cit = objects.find(alias);
				if (cit != objects.end())
					return cit->second;
				cit = templates.find(alias);
				if (cit != templates.end())
					return cit->second;
				return object_instance();
			}

			bool has_object(std::string alias) const {
				typename object_map::const_iterator cit = objects.find(alias);
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
				BOOST_FOREACH(const typename object_map::value_type &t, objects) {
					ss << ", " << t.first << " = {" << t.second->to_string() + "} ";
				}
				ss << "Templates: ";
				BOOST_FOREACH(const typename object_map::value_type &t, templates) {
					ss << ", " << t.first << " = {" << t.second->to_string() + "} ";
				}
				return ss.str();
			}

			void add_object(object_instance object) {
				objects[object->get_alias()] = object;
			}
			void add_object(const std::string alias, object_instance object) {
				objects[alias] = object;
			}
			void add_template(object_instance object) {
				object->make_template(true);
				templates[object->get_alias()] = object;
			}
			void add_template(const std::string alias, object_instance object) {
				templates[alias] = object;
			}
		};
	}
}
