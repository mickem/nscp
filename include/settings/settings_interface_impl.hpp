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

#include <string>
#include <map>
#include <set>
#include <boost/thread/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <settings/settings_core.hpp>
#include <net/net.hpp>
#include <nsclient/logger/logger.hpp>

#define MUTEX_GUARD() \
	boost::unique_lock<boost::timed_mutex> mutex(mutex_, boost::get_system_time() + boost::posix_time::seconds(5)); \
	if (!mutex.owns_lock()) \
		throw settings_exception(__FILE__, __LINE__, "Failed to get mutex, cant get settings instance");

namespace settings {
	class settings_interface_impl : public settings_interface {
	protected:
		settings_core *core_;
		std::string alias_;
		std::string context_;
		net::url url_;

		typedef std::list<instance_raw_ptr> parent_list_type;
		parent_list_type children_;
		boost::timed_mutex mutex_;
	public:
		struct conainer {
		private:
			boost::optional<int> int_val;
			boost::optional<std::string> string_val;
			boost::optional<bool> bool_val;
			bool is_dirty_;
		public:
			conainer(int value, bool dirty) : int_val(value), is_dirty_(dirty) {}
			conainer(bool value, bool dirty) : bool_val(value), is_dirty_(dirty) {}
			conainer(std::string value, bool dirty) : string_val(value), is_dirty_(dirty) {}
			conainer() {}

			bool is_dirty() const { return is_dirty_; }
			std::string get_string() const {
				if (string_val)
					return *string_val;
				if (int_val)
					return str::xtos(*int_val);
				if (bool_val)
					return *bool_val ? "true" : "false";
				return "UNKNOWN TYPE";
			}
			int get_int() const {
				try {
					if (string_val)
						return str::stox<int>(*string_val);
					if (int_val)
						return *int_val;
					if (bool_val)
						return *bool_val ? 1 : 0;
					return -1;
				} catch (const std::exception&) {
					return -1;
				}
			}
			bool get_bool() const {
				if (string_val)
					return string_to_bool(*string_val);
				if (int_val)
					return *int_val == 1 ? true : false;
				if (bool_val)
					return *bool_val;
				return false;
			}
		};
		typedef settings_core::key_path_type cache_key_type;
		typedef boost::unordered_map<cache_key_type, conainer> cache_type;
		typedef boost::unordered_set<std::string> path_cache_type;
		typedef boost::unordered_set<cache_key_type> path_delete_cache_type;
		typedef boost::unordered_map<std::string, std::set<std::string> > key_cache_type;

		cache_type settings_cache_;
		path_delete_cache_type settings_delete_cache_;
		path_cache_type path_cache_;
		path_cache_type settings_delete_path_cache_;
		key_cache_type key_cache_;

		settings_interface_impl(settings_core *core, std::string alias, std::string context) : core_(core), alias_(alias), context_(context), url_(net::parse(context_)) {}

		//////////////////////////////////////////////////////////////////////////
		/// Empty all cached settings values and force a reload.
		/// Notice this does not save so anhy "active" values will be flushed and new ones read from file.
		///
		/// @author mickem
		void clear_cache() {
			MUTEX_GUARD();
			{
				settings_cache_.clear();
				settings_delete_cache_.clear();
				path_cache_.clear();
				settings_delete_path_cache_.clear();
				key_cache_.clear();
				children_.clear();
			}
			real_clear_cache();
			get_core()->set_reload(false);
		}

		//////////////////////////////////////////////////////////////////////////
		/// Set the core module to use
		///
		/// @param core The core to use
		///
		/// @author mickem
		virtual void set_core(settings_core *core) {
			MUTEX_GUARD();
			core_ = core;
		}
		settings_core* get_core() const {
			if (core_ == NULL)
				throw settings_exception(__FILE__, __LINE__, "FATAL ERROR: Settings subsystem not initialized");
			return core_;
		}
		nsclient::logging::logger_instance get_logger() const {
			return core_->get_logger();
		}

		instance_raw_ptr add_child(std::string alias, std::string context) {
			try {
				instance_raw_ptr child = get_core()->create_instance(alias, context);
				{
					MUTEX_GUARD();
					children_.push_back(child);
				}
				return child;
			} catch (const std::exception &e) {
				get_logger()->error("settings", __FILE__, __LINE__, "Failed to load child: " + utf8::utf8_from_native(e.what()));
			}
			return instance_raw_ptr();
		}

		void add_child_unsafe(std::string alias, std::string context) {
			try {
				instance_raw_ptr child = get_core()->create_instance(alias, context);
				children_.push_back(child);
			} catch (const std::exception &e) {
				get_logger()->error("settings", __FILE__, __LINE__, "Failed to load child " + context + ": " + utf8::utf8_from_native(e.what()));
			}
		}

		virtual std::list<boost::shared_ptr<settings_interface> > get_children() {
			return children_;
		}

		template<class T>
		typename T::op_type getter(std::string path, std::string key) {
			MUTEX_GUARD();
			settings_core::key_path_type lookup(path, key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit != settings_cache_.end())
				return T::get_value((*cit).second);
			typename T::op_type val = T::get_real(this, lookup);
			if (!val) {
				instance_raw_ptr child = find_child_unsafe(lookup);
				if (child) {
					return T::get_from_child(child, lookup);
				}
			}
			if (val)
				settings_cache_[lookup] = conainer(*val, false);
			return val;
		}

		template<class T>
		void setter(std::string path, std::string key, typename T::type value) {
			MUTEX_GUARD();
			cache_type::const_iterator cit = settings_cache_.find(cache_key_type(path, key));
			if (cit != settings_cache_.end()) {
				if (!T::has_changed(cit->second, value))
					return;
			}

			settings_core::key_path_type lookup(path, key);
			typename T::op_type current = T::get_real(this, lookup);
			if (!current) {
				instance_raw_ptr child = find_child_unsafe(lookup);
				if (child) {
					T::set_in_child(child, lookup, value);
					return;
				}
			}

			bool unchanged = (current && *current == value) || (!current && T::is_default(value));
			settings_cache_[cache_key_type(path, key)] = conainer(value, !unchanged);
			path_cache_.insert(path);
			core_->register_path(99, path, "in flight", "TODO", true, false, false);

			if (unchanged)
				return;
			add_key_unsafe(path, key);
		}

		instance_raw_ptr find_child_unsafe(settings_core::key_path_type key) {
			BOOST_FOREACH(instance_raw_ptr child, children_) {
				if (child->has_key(key.first, key.second))
					return child;
			}
			return instance_raw_ptr();
		}

		struct StringHandler {
			typedef std::string type;
			typedef boost::optional<type> op_type;
			static type get_value(const conainer &c) {
				return c.get_string();
			}
			static op_type get_real(settings_interface_impl *ptr, settings_core::key_path_type &lookup) {
				return ptr->get_real_string(lookup);
			}
			static op_type get_from_child(instance_raw_ptr child, settings_core::key_path_type &lookup) {
				return child->get_string(lookup.first, lookup.second);
			}
			static bool has_changed(const conainer &c, type val) {
				return c.get_string() != val;
			}
			static void set_in_child(instance_raw_ptr child, settings_core::key_path_type &lookup, type value) {
				child->set_string(lookup.first, lookup.second, value);
			}
			static bool is_default(const type &value) {
				return value.empty();
			}
		};

		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual op_string get_string(std::string path, std::string key) {
			return getter<StringHandler>(path, key);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist the default value will be returned
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param def the default value to use when no value is found
		/// @return the string value
		///
		/// @author mickem
		virtual std::string get_string(std::string path, std::string key, std::string def) {
			op_string val = get_string(path, key);
			if (val)
				return *val;
			return def;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Set or update a string value
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param value the value to set
		///
		/// @author mickem
		virtual void set_string(std::string path, std::string key, std::string value) {
			setter<StringHandler>(path, key, value);
		}

		virtual void remove_key(std::string path, std::string key) {
			MUTEX_GUARD();
			settings_core::key_path_type lookup(path, key);
			cache_type::iterator it = settings_cache_.find(lookup);
			if (it != settings_cache_.end()) {
				settings_cache_.erase(it);
			}
			settings_delete_cache_.insert(cache_key_type(path, key));
			get_core()->set_dirty(true);
		}
		virtual void remove_path(std::string path) {
			MUTEX_GUARD();
			path_cache_type::iterator it = path_cache_.find(path);
			if (it != path_cache_.end()) {
				path_cache_.erase(it);
			}
			settings_delete_path_cache_.insert(path);
			get_core()->set_dirty(true);
		}

		virtual void add_path(std::string path) {
			{
				MUTEX_GUARD();
				path_cache_.insert(path);
			}
			get_core()->set_dirty(true);
		}
		virtual void add_key(std::string path, std::string key) {
			MUTEX_GUARD();
			add_key_unsafe(path, key);
		}
		void add_key_unsafe(std::string path, std::string key) {
			key_cache_type::iterator it = key_cache_.find(path);
			if (it == key_cache_.end()) {
				std::set<std::string> s;
				s.insert(key);
				key_cache_[path] = s;
			} else {
				if ((*it).second.find(key) == (*it).second.end()) {
					(*it).second.insert(key);
				}
			}
			get_core()->set_dirty(true);
		}

		// Meta Functions
		//////////////////////////////////////////////////////////////////////////
		/// Get all (sub) sections (given a path).
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @return a list of sections
		///
		/// @author mickem
		virtual string_list get_sections(std::string path) {
			MUTEX_GUARD();
			string_list ret;
			get_cached_sections_unsafe(path, ret);
			get_real_sections(path, ret);
			BOOST_FOREACH(const instance_raw_ptr &c, children_) {
				string_list itm = c->get_sections(path);
				ret.insert(ret.end(), itm.begin(), itm.end());
			}
			ret.sort();
			ret.unique();
			return ret;
		}
		std::string clean_path(std::string tmp) {
			if (tmp.size() > 0 && tmp[0] == '/') {
				tmp = tmp.substr(1);
			}
			if (tmp.size() > 0 && tmp[tmp.size() - 1] == '/') {
				tmp = tmp.substr(0, tmp.size() - 1);
			}
			return tmp;
		}
		void get_cached_sections_unsafe(std::string path, string_list &list) {
			if (path.empty()) {
				BOOST_FOREACH(std::string s, path_cache_) {
					if (s.length() > 1) {
						std::string::size_type pos = s.find('/', 1);
						if (pos != std::string::npos)
							list.push_back(s.substr(0, pos-1));
						else
							list.push_back(s);
					}
				}
				// TODO add support for retrieving all key paths here!
			} else {
				std::string::size_type path_len = path.length();
				BOOST_FOREACH(std::string s, path_cache_) {
					if (s.length() > (path_len + 1) && s.substr(0, path_len) == path) {
						std::string::size_type pos = s.find('/', path_len + 1);
						std::string tmp;
						if (pos != std::string::npos) {
							tmp = s.substr(path_len, pos-1);
						} else {
							tmp = s.substr(path_len);
						}
						list.push_back(clean_path(tmp));
					}
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get all keys for a path.
		///
		/// @param path The path to get keys under
		/// @return a list of keys
		///
		/// @author mickem
		virtual string_list get_keys(std::string path) {
			if (path.size() > 0 && path[path.size() - 1] == '/') {
				path = path.substr(0, path.size() - 1);
			}
			MUTEX_GUARD();
			string_list ret;
			get_cached_keys_unsafe(path, ret);
			get_real_keys(path, ret);
			BOOST_FOREACH(const instance_raw_ptr &c, children_) {
				string_list itm = c->get_keys(path);
				ret.insert(ret.end(), itm.begin(), itm.end());
			}
			ret.sort();
			ret.unique();
			return ret;
		}
		void get_cached_keys_unsafe(std::string path, string_list &list) {
			key_cache_type::iterator it = key_cache_.find(path);
			if (it != key_cache_.end()) {
				BOOST_FOREACH(std::string s, (*it).second) {
					list.push_back(s);
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Does the section exists?
		///
		/// @param path The path of the section
		/// @return true/false
		///
		/// @author mickem
		virtual bool has_section(std::string path) {
			MUTEX_GUARD();
			path_cache_type::const_iterator cit = path_cache_.find(path);
			if (cit != path_cache_.end())
				return true;
			return has_real_path(path);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Does the key exists?
		///
		/// @param path The path of the section
		/// @param key The key to check
		/// @return true/false
		///
		/// @author mickem
		virtual bool has_key(std::string path, std::string key) {
			MUTEX_GUARD();
			settings_core::key_path_type lookup(path, key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit != settings_cache_.end())
				return true;
			if (has_real_key(lookup)) {
				return true;
			}
			instance_raw_ptr child = find_child_unsafe(lookup);
			if (child) {
				return true;
			}
			return false;
		}

		// Misc Functions
		//////////////////////////////////////////////////////////////////////////
		/// Get a context.
		/// The context is an identifier for the settings store for INI/XML it is the filename.
		///
		/// @return the context
		///
		/// @author mickem
		virtual std::string get_context() {
			MUTEX_GUARD();
			return context_;
		}
		virtual std::string get_context_unsafe() {
			return context_;
		}
		virtual std::string get_file_from_context() {
			return core_->find_file(url_.host + url_.path, "");
		}
		//////////////////////////////////////////////////////////////////////////
		/// Set the context.
		/// The context is an identifier for the settings store for INI/XML it is the filename.
		///
		/// @param context the new context
		///
		/// @author mickem
		virtual void set_context(std::string context) {
			MUTEX_GUARD();
			context_ = context;
		}

		// Save/Load Functions
		//////////////////////////////////////////////////////////////////////////
		/// Reload the settings store
		///
		/// @author mickem
		virtual void reload() {
			load();
		}
		//////////////////////////////////////////////////////////////////////////
		/// Copy the settings store to another settings store
		///
		/// @param other the settings store to save to
		///
		/// @author mickem
		virtual void save_to(std::string alias, std::string other) {
			instance_ptr i = get_core()->create_instance(alias, other);
			if (!i)
				throw settings_exception(__FILE__, __LINE__, "Failed to create new instance!");
			save_to(i);
		}
		virtual void save_to(instance_ptr other) {
			if (!other)
				throw settings_exception(__FILE__, __LINE__, "Cant migrate to NULL instance!");
			if (this->get_context() == other->get_context()) {
				get_logger()->error("settings", __FILE__, __LINE__, "Cant migrate to the same setting store: " + other->get_context());
				return;
			}
			other->clear_cache();
			st_copy_section("", other);
			other->save();
		}
		void st_copy_section(std::string path, instance_ptr other) {
			if (!other)
				throw settings_exception(__FILE__, __LINE__, "No target instance: Cant copy settings");
			get_logger()->trace("settings", __FILE__, __LINE__, "In " + alias_ + " copying section " + path);

			string_list list;
			{
				MUTEX_GUARD();
				get_cached_sections_unsafe(path, list);
				get_real_sections(path, list);
			}

			std::string subpath = path;
			// TODO: check trailing / instead!
			if (!subpath.empty())
				subpath += "/";
			for (string_list::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
				st_copy_section(subpath + *cit, other);
			}
			list.clear();
			{
				MUTEX_GUARD();
				get_cached_keys_unsafe(path, list);
				get_real_keys(path, list);

			}
			BOOST_FOREACH (const std::string &key, list) {
				settings_interface::op_string val = get_string(path, key);
				if (val)
					other->set_string(path, key, *val);
				else
					other->set_string(path, key, "");
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Save the settings store
		///
		/// @author mickem
		virtual void save() {
			MUTEX_GUARD();

			BOOST_FOREACH(const cache_key_type &v, settings_delete_cache_) {
				remove_real_value(v);
			}
			BOOST_FOREACH(const std::string &v, settings_delete_path_cache_) {
				remove_real_path(v);
			}

			BOOST_FOREACH(std::string path, path_cache_) {
				set_real_path(path);
			}
			std::set<std::string> sections;
			for (cache_type::const_iterator cit = settings_cache_.begin(); cit != settings_cache_.end(); ++cit) {
				set_real_value((*cit).first, (*cit).second);
				sections.insert((*cit).first.first);
			}
			BOOST_FOREACH(instance_raw_ptr &child, children_) {
				child->save();
			}
			get_core()->set_dirty(false);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Load settings from the context.
		///
		/// @author mickem
		virtual void load() {
			MUTEX_GUARD();
			settings_delete_cache_.clear();
			settings_delete_path_cache_.clear();
			path_cache_.clear();
			key_cache_.clear();
			settings_cache_.clear();
			get_core()->set_dirty(false);
		}

		//////////////////////////////////////////////////////////////////////////
		///                       VIRTUAL FUNCTIONS                           ////
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		/// Get all (sub) sections (given a path).
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_sections(std::string path, string_list &list) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get all keys given a path/section.
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_keys(std::string path, string_list &list) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual op_string get_real_string(settings_core::key_path_type key) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_value(settings_core::key_path_type key, conainer value) = 0;

		virtual void remove_real_value(settings_core::key_path_type key) = 0;
		virtual void remove_real_path(std::string path) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_path(std::string path) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Check if a key exists
		///
		/// @param key the key to lookup
		/// @return true/false if the key exists.
		///
		/// @author mickem
		virtual bool has_real_key(settings_core::key_path_type key) = 0;
		virtual bool has_real_path(std::string path) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get the type this settings store represent.
		///
		/// @return the type of settings store
		///
		/// @author mickem
// 		virtual settings_core::settings_type get_type() = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Is this the active settings store
		///
		/// @return
		///
		/// @author mickem
// 		virtual bool is_active() = 0;

		virtual void real_clear_cache() = 0;

		virtual std::string to_string() {
			std::string ret = get_info();
			if (!children_.empty()) {
				ret += "parents = [";
				BOOST_FOREACH(parent_list_type::value_type i, children_) {
					ret += i->to_string();
				}
				ret += "]";
			}
			return ret + "}";
		}

		inline std::string make_skey(std::string path, std::string key) {
			return path + "." + key;
		}

		virtual void house_keeping() {
			BOOST_FOREACH(parent_list_type::value_type i, children_) {
				i->house_keeping();
			}
		}
	};



}
