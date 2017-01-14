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

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/dll_defines.hpp>
#include <settings/client/settings_client_interface.hpp>

#include <utf8.hpp>

#include <boost/any.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>

#include <map>
#include <list>
#include <string>

#ifdef WIN32
#pragma warning( disable : 4800 )
#endif

namespace boost {
	template<>
	inline std::string lexical_cast<std::string, boost::filesystem::path>(const boost::filesystem::path& arg) {
		return utf8::cvt<std::string>(arg.string());
	}
}

namespace nscapi {
	namespace settings_helper {
		typedef boost::shared_ptr<settings_impl_interface> settings_impl_interface_ptr;

		inline std::string make_skey(std::string path, std::string key) {
			return path + "." + key;
		}

		class key_interface {
		public:
			virtual NSCAPI::settings_type get_type() const = 0;
			virtual nscapi::settings::settings_value get_default() const = 0;
			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const = 0;
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const = 0;
			virtual void notify_path(settings_impl_interface_ptr core_, std::string path) const = 0;
		};
		typedef boost::shared_ptr<key_interface> key_type;

		NSCAPI_EXPORT key_type string_key(std::string *val, std::string def);
		NSCAPI_EXPORT key_type string_key(std::string *val);
		NSCAPI_EXPORT key_type int_key(int *val, int def = 0);
		NSCAPI_EXPORT key_type size_key(std::size_t *val, std::size_t def = 0);
		NSCAPI_EXPORT key_type uint_key(unsigned int *val, unsigned int def);
		NSCAPI_EXPORT key_type uint_key(unsigned int *val);
		NSCAPI_EXPORT key_type bool_key(bool *val, bool def);
		NSCAPI_EXPORT key_type bool_key(bool *val);
		NSCAPI_EXPORT key_type path_key(std::string *val, std::string def);
		NSCAPI_EXPORT key_type path_key(std::string *val);
		NSCAPI_EXPORT key_type path_key(boost::filesystem::path *val, std::string def);
		NSCAPI_EXPORT key_type path_key(boost::filesystem::path *val);

		NSCAPI_EXPORT key_type string_fun_key(boost::function<void(std::string)> fun, std::string def);
		NSCAPI_EXPORT key_type string_fun_key(boost::function<void(std::string)> fun);
		NSCAPI_EXPORT key_type path_fun_key(boost::function<void(std::string)> fun, std::string def);
		NSCAPI_EXPORT key_type path_fun_key(boost::function<void(std::string)> fun);
		NSCAPI_EXPORT key_type bool_fun_key(boost::function<void(bool)> fun, bool def);
		NSCAPI_EXPORT key_type bool_fun_key(boost::function<void(bool)> fun);
		NSCAPI_EXPORT key_type int_fun_key(boost::function<void(int)> fun, int def);
		NSCAPI_EXPORT key_type int_fun_key(boost::function<void(int)> fun);

		NSCAPI_EXPORT key_type fun_values_path(boost::function<void(std::string, std::string)> fun);
		NSCAPI_EXPORT key_type string_map_path(std::map<std::string, std::string> *val);

		struct description_container {
			std::string icon;
			std::string title;
			std::string description;
			bool advanced;
			description_container() : advanced(false) {}

			description_container(std::string title, std::string description, bool advanced)
				: title(title)
				, description(description)
				, advanced(advanced) {}
			description_container(std::string title, std::string description, std::string icon)
				: icon(icon)
				, title(title)
				, description(description)
				, advanced(false) {}
			description_container(std::string title, std::string description)
				: title(title)
				, description(description)
				, advanced(false) {}

			description_container(const description_container& obj) {
				title = obj.title;
				icon = obj.icon;
				description = obj.description;
				advanced = obj.advanced;
			}
			description_container& operator=(const description_container& obj) {
				icon= obj.icon;
				title = obj.title;
				description = obj.description;
				advanced = obj.advanced;
				return *this;
			}
		};

		struct key_info {
			std::string path;
			std::string key_name;
			key_type key;
			description_container description;
			std::string parent;
			bool is_sample;

			key_info(std::string path_, std::string key_name_, key_type key, description_container description_)
				: path(path_)
				, key_name(key_name_)
				, key(key)
				, description(description_)
				, is_sample(false) {}
			key_info(const key_info& obj) : path(obj.path), key_name(obj.key_name), key(obj.key), description(obj.description), parent(obj.parent), is_sample(obj.is_sample) {}
			virtual key_info& operator=(const key_info& obj) {
				path = obj.path;
				key_name = obj.key_name;
				key = obj.key;
				description = obj.description;
				parent = obj.parent;
				is_sample = obj.is_sample;
				return *this;
			}
			void set_parent(std::string parent_) {
				parent = parent_;
			}
			bool has_parent() const {
				return !parent.empty();
			}
			std::string get_parent() const {
				return parent;
			}
		};
		struct path_info {
			std::string path_name;
			key_type path;
			description_container description;
			description_container subkey_description;
			bool is_sample;

			path_info(std::string path_name, description_container description) : path_name(path_name), description(description), is_sample(false) {}
			path_info(std::string path_name, key_type path, description_container description, description_container subkey_description) : path_name(path_name), path(path), description(description), subkey_description(subkey_description), is_sample(false) {}

			path_info(const path_info& obj) : path_name(obj.path_name), path(obj.path), description(obj.description), is_sample(obj.is_sample) {}
			virtual path_info& operator=(const path_info& obj) {
				path_name = obj.path_name;
				path = obj.path;
				description = obj.description;
				subkey_description = obj.subkey_description;
				is_sample = obj.is_sample;
				return *this;
			}
		};
		struct tpl_info {
			std::string path_name;
			description_container description;
			std::string fields;

			tpl_info(std::string path_name, description_container description, std::string fields) : path_name(path_name), description(description), fields(fields) {}

			tpl_info(const tpl_info& obj) : path_name(obj.path_name), description(obj.description), fields(obj.fields) {}
			virtual tpl_info& operator=(const tpl_info& obj) {
				path_name = obj.path_name;
				description = obj.description;
				fields = obj.fields;
				return *this;
			}
		};

		class settings_registry;
		class NSCAPI_EXPORT settings_paths_easy_init {
		public:
			settings_paths_easy_init(settings_registry* owner) : owner(owner), is_sample(false) {}
			settings_paths_easy_init(std::string path, settings_registry* owner) : path_(path), owner(owner), is_sample(false) {}
			settings_paths_easy_init(std::string path, settings_registry* owner, bool is_sample) : path_(path), owner(owner), is_sample(is_sample) {}

			settings_paths_easy_init& operator()(key_type value, std::string title, std::string description, std::string subkeytitle, std::string subkeydescription) {
				boost::shared_ptr<path_info> d(new path_info(path_, value, description_container(title, description), description_container(subkeytitle, subkeydescription)));
				add(d);
				return *this;
			}
			settings_paths_easy_init& operator()(std::string title, std::string description) {
				boost::shared_ptr<path_info> d(new path_info(path_, description_container(title, description)));
				add(d);
				return *this;
			}
			settings_paths_easy_init& operator()(std::string path, std::string title, std::string description) {
				if (!path_.empty())
					path = path_ + "/" + path;
				boost::shared_ptr<path_info> d(new path_info(path, description_container(title, description)));
				add(d);
				return *this;
			}
			settings_paths_easy_init& operator()(std::string path, key_type value, std::string title, std::string description, std::string subkeytitle, std::string subkeydescription) {
				if (!path_.empty())
					path = path_ + "/" + path;
				boost::shared_ptr<path_info> d(new path_info(path, value, description_container(title, description), description_container(subkeytitle, subkeydescription)));
				add(d);
				return *this;
			}

			void add(boost::shared_ptr<path_info> d);

		private:
			std::string path_;
			settings_registry* owner;
			bool is_sample;
		};

		class NSCAPI_EXPORT settings_tpl_easy_init {
		public:
			settings_tpl_easy_init(std::string path, settings_registry* owner) : path_(path), owner(owner) {}

			settings_tpl_easy_init& operator()(std::string path, std::string icon, std::string title, std::string desc, std::string fields) {
				if (!path_.empty())
					path = path_ + "/" + path;
				boost::shared_ptr<tpl_info> d(new tpl_info(path, description_container(title, desc, icon), fields));
				add(d);
				return *this;
			}

			void add(boost::shared_ptr<tpl_info> d);

		private:
			std::string path_;
			settings_registry* owner;
			bool is_sample;
		};


		class NSCAPI_EXPORT settings_keys_easy_init {
		public:
			settings_keys_easy_init(settings_registry* owner_) : owner(owner_), is_sample(false) {}
			settings_keys_easy_init(std::string path, settings_registry* owner_) : owner(owner_), path_(path), is_sample(false) {}
			settings_keys_easy_init(std::string path, settings_registry* owner_, bool is_sample) : owner(owner_), path_(path), is_sample(is_sample) {}
			settings_keys_easy_init(std::string path, std::string parent, settings_registry* owner_) : owner(owner_), path_(path), parent_(parent), is_sample(false) {}
			settings_keys_easy_init(std::string path, std::string parent, settings_registry* owner_, bool is_sample) : owner(owner_), path_(path), parent_(parent), is_sample(is_sample) {}

			virtual ~settings_keys_easy_init() {}

			settings_keys_easy_init& operator()(std::string path, std::string key_name, key_type value, std::string title, std::string description, bool advanced = false) {
				boost::shared_ptr<key_info> d(new key_info(path, key_name, value, description_container(title, description, advanced)));
				if (!parent_.empty())
					d->set_parent(parent_);
				add(d);
				return *this;
			}

			settings_keys_easy_init& operator()(std::string key_name, key_type value, std::string title, std::string description, bool advanced = false) {
				boost::shared_ptr<key_info> d(new key_info(path_, key_name, value, description_container(title, description, advanced)));
				if (!parent_.empty())
					d->set_parent(parent_);
				add(d);
				return *this;
			}

			void add(boost::shared_ptr<key_info> d);

		private:
			settings_registry* owner;
			std::string path_;
			std::string parent_;
			bool is_sample;
		};

		class path_extension {
		public:
			path_extension(settings_registry * owner, std::string path) : owner_(owner), path_(path), is_sample(false) {}

			settings_keys_easy_init add_key_to_path(std::string path) {
				return settings_keys_easy_init(get_path(path), owner_, is_sample);
			}
			settings_keys_easy_init add_key() {
				return settings_keys_easy_init(path_, owner_, is_sample);
			}
			settings_paths_easy_init add_path(std::string path = "") {
				return settings_paths_easy_init(get_path(path), owner_, is_sample);
			}
			inline std::string get_path(std::string path) {
				if (!path.empty())
					return path_ + "/" + path;
				return path_;
			}
			void set_sample() {
				is_sample = true;
			}
		private:
			settings_registry * owner_;
			std::string path_;
			bool is_sample;
		};
		class alias_extension {
		public:
			alias_extension(settings_registry * owner, std::string alias) : owner_(owner), alias_(alias) {}
			alias_extension(const alias_extension &other) : owner_(other.owner_), alias_(other.alias_), parent_(other.parent_) {}
			alias_extension& operator = (const alias_extension& other) {
				owner_ = other.owner_;
				alias_ = other.alias_;
				parent_ = other.parent_;
				return *this;
			}

			settings_keys_easy_init add_key_to_path(std::string path) {
				return settings_keys_easy_init(get_path(path), parent_, owner_);
			}
			settings_paths_easy_init add_path(std::string path) {
				return settings_paths_easy_init(get_path(path), owner_);
			}
			inline std::string get_path(std::string path = "") {
				if (path.empty())
					return "/" + alias_;
				return path + "/" + alias_;
			}

			settings_keys_easy_init add_key_to_settings(std::string path = "") {
				return settings_keys_easy_init(get_settings_path(path), parent_, owner_);
			}
			settings_paths_easy_init add_path_to_settings(std::string path = "") {
				return settings_paths_easy_init(get_settings_path(path), owner_);
			}
			settings_tpl_easy_init add_templates(std::string path = "") {
				return settings_tpl_easy_init(get_settings_path(path), owner_);
			}
			inline std::string get_settings_path(std::string path) {
				if (path.empty())
					return "/settings/" + alias_;
				return "/settings/" + alias_ + "/" + path;
			}

			alias_extension add_parent(std::string parent_path) {
				set_parent_path(parent_path);
				return *this;
			}

			static std::string get_alias(std::string cur, std::string def) {
				if (cur.empty())
					return def;
				else
					return cur;
			}
			static std::string get_alias(std::string prefix, std::string cur, std::string def) {
				if (!prefix.empty())
					prefix += "/";
				if (cur.empty())
					return prefix + def;
				else
					return prefix + cur;
			}
			void set_alias(std::string cur, std::string def) {
				alias_ = get_alias(cur, def);
			}
			void set_alias(std::string prefix, std::string cur, std::string def) {
				alias_ = get_alias(prefix, cur, def);
			}
			void set_parent_path(std::string parent) {
				parent_ = parent;
			}

		private:
			settings_registry * owner_;
			std::string alias_;
			std::string parent_;
		};

		class settings_registry {
			typedef std::list<boost::shared_ptr<key_info> > key_list;
			typedef std::list<boost::shared_ptr<path_info> > path_list;
			typedef std::list<boost::shared_ptr<tpl_info> > tpl_list_type;
			key_list keys_;
			tpl_list_type tpl_;
			path_list paths_;
			settings_impl_interface_ptr core_;
			std::string alias_;
		public:
			settings_registry(settings_impl_interface_ptr core) : core_(core) {}
			virtual ~settings_registry() {}
			void add(boost::shared_ptr<key_info> info) {
				keys_.push_back(info);
			}
			void add(boost::shared_ptr<tpl_info> info) {
				tpl_.push_back(info);
			}
			void add(boost::shared_ptr<path_info> info) {
				paths_.push_back(info);
			}

			settings_keys_easy_init add_key() {
				return settings_keys_easy_init(this);
			}
			settings_keys_easy_init add_key_to_path(std::string path) {
				return settings_keys_easy_init(path, this);
			}
			settings_keys_easy_init add_key_to_settings(std::string path) {
				return settings_keys_easy_init("/settings/" + path, this);
			}
			settings_paths_easy_init add_path() {
				return settings_paths_easy_init(this);
			}
			settings_paths_easy_init add_path_to_settings() {
				return settings_paths_easy_init("/settings", this);
			}

			void set_alias(std::string cur, std::string def) {
				alias_ = alias_extension::get_alias(cur, def);
			}
			void set_alias(std::string prefix, std::string cur, std::string def) {
				alias_ = alias_extension::get_alias(prefix, cur, def);
			}
			void set_alias(std::string alias) {
				alias_ = alias;
			}
			alias_extension alias() {
				return alias_extension(this, alias_);
			}
			alias_extension alias(std::string alias) {
				return alias_extension(this, alias);
			}
			alias_extension alias(std::string cur, std::string def) {
				return alias_extension(this, alias_extension::get_alias(cur, def));
			}
			alias_extension alias(std::string prefix, std::string cur, std::string def) {
				return alias_extension(this, alias_extension::get_alias(prefix, cur, def));
			}

			path_extension path(std::string path) {
				return path_extension(this, path);
			}

			void set_static_key(std::string path, std::string key, std::string value) {
				core_->set_string(path, key, value);
			}
			std::string get_static_string(std::string path, std::string key, std::string def_value) {
				return core_->get_string(path, key, def_value);
			}

			void register_key(std::string path, std::string key, int type, std::string title, std::string description, std::string defaultValue, bool advanced = false) {
				core_->register_key(path, key, type, title, description, nscapi::settings::settings_value::make_string(defaultValue), advanced, false);
			}
			void register_all() {
				BOOST_FOREACH(key_list::value_type v, keys_) {
					if (v->key) {
						if (v->has_parent()) {
							core_->register_key(v->parent, v->key_name, v->key->get_type(), v->description.title, v->description.description, v->key->get_default(), v->description.advanced, v->is_sample);
							std::string desc = v->description.description + " parent for this key is found under: " + v->parent + " this is marked as advanced in favor of the parent.";
							core_->register_key(v->path, v->key_name, v->key->get_type(), v->description.title, desc, v->key->get_default(), true, false);
						} else {
							core_->register_key(v->path, v->key_name, v->key->get_type(), v->description.title, v->description.description, v->key->get_default(), v->description.advanced, v->is_sample);
						}
					}
				}
				BOOST_FOREACH(path_list::value_type v, paths_) {
					core_->register_path(v->path_name, v->description.title, v->description.description, v->description.advanced, v->is_sample);
					if (!v->subkey_description.title.empty()) {
						BOOST_FOREACH(const std::string &s, core_->get_keys(v->path_name))
							core_->register_key(v->path_name, s, NSCAPI::key_string, v->subkey_description.title, v->subkey_description.description, "", v->description.advanced, v->is_sample);
					}
				}
				BOOST_FOREACH(tpl_list_type::value_type v, tpl_) {
					core_->register_tpl(v->path_name, v->description.title, v->description.icon, v->description.description, v->fields);
				}
			}
			void clear() {
				keys_.clear();
				paths_.clear();
			}

			std::string expand_path(std::string path) {
				return core_->expand_path(path);
			}

			void notify() {
				BOOST_FOREACH(key_list::value_type v, keys_) {
					try {
						if (v->key) {
							if (v->has_parent())
								v->key->notify(core_, v->parent, v->path, v->key_name);
							else
								v->key->notify(core_, v->path, v->key_name);
						}
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to notify " + v->key_name + ": " + utf8::utf8_from_native(e.what()));
					} catch (...) {
						core_->err(__FILE__, __LINE__, "Failed to notify " + v->key_name);
					}
				}
				BOOST_FOREACH(path_list::value_type v, paths_) {
					try {
						if (v->path)
							v->path->notify_path(core_, v->path_name);
					} catch (const std::exception &e) {
						core_->err(__FILE__, __LINE__, "Failed to notify " + v->path_name + ": " + utf8::utf8_from_native(e.what()));
					} catch (...) {
						core_->err(__FILE__, __LINE__, "Failed to notify " + v->path_name);
					}
				}
			}
		};
	}
}