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
#include <settings/client/settings_client_interface.hpp>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem/path.hpp>

#include <map>
#include <list>
#include <string>


namespace nscapi {
	namespace settings_helper {
		typedef boost::shared_ptr<settings_impl_interface> settings_impl_interface_ptr;

		class key_interface {
		public:
			virtual std::string get_default() const = 0;
			virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const = 0;
			virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const = 0;
			virtual void notify_path(settings_impl_interface_ptr core_, std::string path) const = 0;
		};
		typedef boost::shared_ptr<key_interface> key_type;

		key_type string_key(std::string *val, std::string def);
		key_type string_key(std::string *val);
		key_type int_key(int *val, int def = 0);
		key_type size_key(std::size_t *val, std::size_t def = 0);
		key_type uint_key(unsigned int *val, unsigned int def);
		key_type uint_key(unsigned int *val);
		key_type bool_key(bool *val, bool def);
		key_type bool_key(bool *val);
		key_type path_key(std::string *val, std::string def);
		key_type path_key(std::string *val);
		key_type path_key(boost::filesystem::path *val, std::string def);
		key_type path_key(boost::filesystem::path *val);

		key_type string_fun_key(boost::function<void(std::string)> fun, std::string def);
		key_type string_fun_key(boost::function<void(std::string)> fun);
		key_type cstring_fun_key(boost::function<void(const char*)> fun);
		key_type cstring_fun_key(boost::function<void(const char*)> fun, std::string def);
		key_type path_fun_key(boost::function<void(std::string)> fun, std::string def);
		key_type path_fun_key(boost::function<void(std::string)> fun);
		key_type bool_fun_key(boost::function<void(bool)> fun, bool def);
		key_type bool_fun_key(boost::function<void(bool)> fun);
		key_type int_fun_key(boost::function<void(int)> fun, int def);
		key_type int_fun_key(boost::function<void(int)> fun);

		key_type fun_values_path(boost::function<void(std::string, std::string)> fun);
		key_type string_map_path(std::map<std::string, std::string> *val);

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

		class settings_registry;
		struct path_info;
		class settings_paths_easy_init {
		public:
			settings_paths_easy_init(settings_registry* owner) : owner(owner), is_sample(false) {}
			settings_paths_easy_init(std::string path, settings_registry* owner) : path_(path), owner(owner), is_sample(false) {}
			settings_paths_easy_init(std::string path, settings_registry* owner, bool is_sample) : path_(path), owner(owner), is_sample(is_sample) {}

			settings_paths_easy_init& operator()(key_type value, std::string title, std::string description, std::string subkeytitle, std::string subkeydescription);
			settings_paths_easy_init& operator()(std::string title, std::string description);
			settings_paths_easy_init& operator()(std::string path, std::string title, std::string description);
			settings_paths_easy_init& operator()(std::string path, key_type value, std::string title, std::string description);
			settings_paths_easy_init& operator()(std::string path, key_type value, std::string title, std::string description, std::string subkeytitle, std::string subkeydescription);

		private:
			void add(boost::shared_ptr<path_info> d);

			std::string path_;
			settings_registry* owner;
			bool is_sample;
		};

		struct tpl_info;
		class settings_tpl_easy_init {
		public:
			settings_tpl_easy_init(std::string path, settings_registry* owner) : path_(path), owner(owner), is_sample(false) {}

			settings_tpl_easy_init& operator()(std::string path, std::string icon, std::string title, std::string desc, std::string fields);

		private:
			void add(boost::shared_ptr<tpl_info> d);

			std::string path_;
			settings_registry* owner;
			bool is_sample;
		};

		struct key_info;
		class settings_keys_easy_init {
		public:
			settings_keys_easy_init(settings_registry* owner_) : owner(owner_), is_sample(false) {}
			settings_keys_easy_init(std::string path, settings_registry* owner_) : owner(owner_), path_(path), is_sample(false) {}
			settings_keys_easy_init(std::string path, settings_registry* owner_, bool is_sample) : owner(owner_), path_(path), is_sample(is_sample) {}
			settings_keys_easy_init(std::string path, std::string parent, settings_registry* owner_) : owner(owner_), path_(path), parent_(parent), is_sample(false) {}
			settings_keys_easy_init(std::string path, std::string parent, settings_registry* owner_, bool is_sample) : owner(owner_), path_(path), parent_(parent), is_sample(is_sample) {}

			virtual ~settings_keys_easy_init() {}

			settings_keys_easy_init& operator()(std::string path, std::string key_name, key_type value, std::string title, std::string description, bool advanced = false);

			settings_keys_easy_init& operator()(std::string key_name, key_type value, std::string title, std::string description, bool advanced = false);


		private:
			void add(boost::shared_ptr<key_info> d);

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
			std::string get_path(std::string path = "") const {
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
			std::string get_settings_path(std::string path) const {
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

			void set_static_key(std::string path, std::string key, std::string value) const {
				core_->set_string(path, key, value);
			}
			std::string get_static_string(std::string path, std::string key, std::string def_value) const {
				return core_->get_string(path, key, def_value);
			}

			void register_key(std::string path, std::string key, std::string title, std::string description, std::string defaultValue, bool advanced = false) const {
				core_->register_key(path, key, title, description, defaultValue, advanced, false);
			}
			void register_all() const;
			void clear() {
				keys_.clear();
				paths_.clear();
			}

			std::string expand_path(std::string path) const {
				return core_->expand_path(path);
			}

			void notify();
		};
	}
}