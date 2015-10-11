/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
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
#include <strEx.h>
#include <settings/settings_core.hpp>
#include <net/net.hpp>
#include <nsclient/logger.hpp>

#define MUTEX_GUARD() \
	boost::unique_lock<boost::timed_mutex> mutex(mutex_, boost::get_system_time() + boost::posix_time::seconds(5)); \
	if (!mutex.owns_lock()) \
		throw settings_exception("Failed to get mutex, cant get settings instance");


namespace settings {
	class settings_interface_impl : public settings_interface {
	protected:
		settings_core *core_;
		typedef std::list<instance_raw_ptr> parent_list_type;
		parent_list_type children_;
		boost::timed_mutex mutex_;
	public:
		struct conainer {
			settings_core::key_type type;
			int int_val;
			std::string string_val;
			bool is_dirty_;
			conainer(int value, bool dirty) : type(settings_core::key_integer), int_val(value), is_dirty_(dirty) {}
			conainer(bool value, bool dirty) : type(settings_core::key_bool), int_val(value ? 1 : 0), is_dirty_(dirty) {}
			conainer(std::string value, bool dirty) : type(settings_core::key_string), string_val(value), is_dirty_(dirty) {}
			conainer() : type(settings_core::key_string) {}

			bool is_dirty() const { return is_dirty_; }
			std::string get_string() const {
				if (type==settings_core::key_string)
					return string_val;
				if (type==settings_core::key_integer)
					return strEx::s::xtos(int_val);
				if (type==settings_core::key_bool)
					return int_val==1?"true":"false";
				return "UNKNOWN TYPE";
			}
			int get_int() const {
				try {
					if (type==settings_core::key_string)
						return strEx::s::stox<int>(string_val);
					if (type==settings_core::key_integer)
						return int_val;
					if (type==settings_core::key_bool)
						return int_val==1?1:0;
					return -1;
				} catch (const std::exception&) {
					return -1;
				}
			}
			bool get_bool() const {
				if (type==settings_core::key_string)
					return string_to_bool(string_val);
				if (type==settings_core::key_integer)
					return int_val==1?true:false;
				if (type==settings_core::key_bool)
					return int_val==1?true:false;
				return false;
			}
		};
		typedef settings_core::key_path_type cache_key_type;
		typedef boost::unordered_map<cache_key_type,conainer> cache_type;
		typedef boost::unordered_set<std::string> path_cache_type;
		typedef boost::unordered_set<cache_key_type> path_delete_cache_type;
		typedef boost::unordered_map<std::string,std::set<std::string> > key_cache_type;

		cache_type settings_cache_;
		path_delete_cache_type settings_delete_cache_;
		path_cache_type path_cache_;
		path_cache_type settings_delete_path_cache_;
		key_cache_type key_cache_;
		std::string context_;
		net::url url_;

		//SettingsInterfaceImpl() : core_(NULL) {}
		settings_interface_impl(settings_core *core, std::string context) : core_(core), context_(context), url_(net::parse(context_)) {}

		//////////////////////////////////////////////////////////////////////////
		/// Empty all cached settings values and force a reload.
		/// Notice this does not save so anhy "active" values will be flushed and new ones read from file.
		///
		/// @author mickem
		void clear_cache() {
			MUTEX_GUARD();
			{
				settings_cache_.clear();
				path_cache_.clear();
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
				throw settings_exception("FATAL ERROR: Settings subsystem not initialized");
			return core_;
		}
		nsclient::logging::logger_interface* get_logger() const {
			return nsclient::logging::logger::get_logger();
		}

		instance_raw_ptr add_child(std::string context) {
			try {
				instance_raw_ptr child = get_core()->create_instance(context);
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

		void add_child_unsafe(std::string context) {
			try {
				instance_raw_ptr child = get_core()->create_instance(context);
				children_.push_back(child);
			} catch (const std::exception &e) {
				get_logger()->error("settings", __FILE__, __LINE__, "Failed to load child " + context + ": " + utf8::utf8_from_native(e.what()));
			}
		}

		virtual std::list<boost::shared_ptr<settings_interface> > get_children() {
			return children_;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual op_string get_string(std::string path, std::string key) {
			MUTEX_GUARD();
			settings_core::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				op_string val = get_real_string(lookup);
				if (!val)
					val = get_string_from_child_unsafe(lookup);
				if (val) 
					settings_cache_[lookup] = conainer(*val, false);
				return val;
			}
			return (*cit).second.get_string();
		}
		op_string get_string_from_child_unsafe(settings_core::key_path_type key) {
			for (parent_list_type::iterator it = children_.begin(); it != children_.end(); ++it) {
					op_string val = (*it)->get_string(key.first, key.second);
					if (val)
						return val;
			}
			return op_string();
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
			{
				MUTEX_GUARD();
				cache_type::const_iterator cit = settings_cache_.find(cache_key_type(path,key));
				if (cit != settings_cache_.end()) {
					if (cit->second.get_string() == value)
						return;
				}

				settings_core::key_path_type lookup(path,key);
				op_string current = get_real_string(lookup);
				if (!current)
					current = get_string_from_child_unsafe(lookup);
				if (current) {
					std::string valx = *current;
				}

				bool unchanged = (current && *current == value) || (!current && value.empty());
				settings_cache_[cache_key_type(path,key)] = conainer(value, !unchanged);
				path_cache_.insert(path);

				if (unchanged)
					return;
			}
			get_core()->set_dirty(true);
			add_key(path, key);
		}

		virtual void remove_key(std::string path, std::string key) {
			MUTEX_GUARD();
			settings_core::key_path_type lookup(path,key);
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
			MUTEX_GUARD();
			path_cache_.insert(path);
			get_core()->set_dirty(true);
		}
		virtual void add_key(std::string path, std::string key) {
			MUTEX_GUARD();
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

		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual op_int get_int(std::string path, std::string key) {
			MUTEX_GUARD();
			settings_core::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				op_int val = get_real_int(lookup);
				if (!val)
					val = get_int_from_child_unsafe(path, key);
				if (!val)
					return val;
				settings_cache_[lookup] = conainer(*val, false);
				return val;
			}
			return op_int((*cit).second.get_int());
		}
		op_int get_int_from_child_unsafe(std::string path, std::string key) {
			for (parent_list_type::iterator it = children_.begin(); it != children_.end(); ++it) {
				op_int val = (*it)->get_int(path, key);
				if (val)
					return val;
			}
			return op_int();
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist the default value will be returned
		/// 
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param def the default value to use when no value is found
		/// @return the string value
		///
		/// @author mickem
		virtual int get_int(std::string path, std::string key, int def) {
			op_int val = get_int(path, key);
			if (val)
				return *val;
			return def;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Set or update an integer value
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param value the value to set
		///
		/// @author mickem
		virtual void set_int(std::string path, std::string key, int value) {
			{
				MUTEX_GUARD();
				settings_cache_[cache_key_type(path,key)] = conainer(value, true);
				path_cache_.insert(path);
			}
			add_key(path, key);
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get the type of a key (String, int, bool)
		///
		/// @param path the path to get type for
		/// @param key the key to get the type for
		/// @return the type of the key
		///
		/// @author mickem
		virtual settings_core::key_type get_key_type(std::string path, std::string key) {
			MUTEX_GUARD();
			cache_type::iterator it = settings_cache_.find(cache_key_type(path, key));
			if (it == settings_cache_.end())
				return settings_core::key_string;
			return it->second.type;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual op_bool get_bool(std::string path, std::string key) {
			MUTEX_GUARD();
			settings_core::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				op_bool val = get_real_bool(lookup);
				if (!val)
					val = get_bool_from_child_unsafe(path, key);
				if (!val)
					return val;
				settings_cache_[lookup] = conainer(*val, false);
				return val;
			}
			return (*cit).second.get_bool();
		}
		op_bool get_bool_from_child_unsafe(std::string path, std::string key) {
			for (parent_list_type::iterator it = children_.begin(); it != children_.end(); ++it) {
				op_bool val = (*it)->get_bool(path, key);
				if (val)
					return val;
			}
			return op_bool();
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist the default value will be returned
		/// 
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param def the default value to use when no value is found
		/// @return the string value
		///
		/// @author mickem
		virtual bool get_bool(std::string path, std::string key, bool def) {
			op_bool val = get_bool(path, key);
			if (val)
				return *val;
			return def;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Set or update a boolean value
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param value the value to set
		///
		/// @author mickem
		virtual void set_bool(std::string path, std::string key, bool value) {
			{
				MUTEX_GUARD();
				settings_cache_[cache_key_type(path,key)] = conainer(value, true);
				path_cache_.insert(path);
			}
			add_key(path, key);
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
			get_section_from_child_unsafe(path, ret);
			ret.sort();
			ret.unique();
			return ret;
		}
		void get_cached_sections_unsafe(std::string path, string_list &list) {
			if (path.empty()) {
				BOOST_FOREACH(std::string s, path_cache_) {
					if (s.length() > 1) {
						std::string::size_type pos = s.find('/', 1);
						if (pos != std::string::npos)
							list.push_back(s.substr(0,pos));
						else
							list.push_back(s);
					}
				}
				// TODO add support for retrieving all key paths here!
			} else {
				std::string::size_type path_len = path.length();
				BOOST_FOREACH(std::string s, path_cache_) {
					if (s.length() > (path_len+1) && s.substr(0,path_len) == path) {
						std::string::size_type pos = s.find('/', path_len+1);
						if (pos != std::string::npos)
							list.push_back(s.substr(path_len+1,pos));
						else
							list.push_back(s.substr(path_len+1));
					}
				}
			}
		}
		void get_section_from_child_unsafe(std::string path, string_list &list) {
			for (parent_list_type::iterator it = children_.begin(); it != children_.end(); ++it) {
				string_list itm = (*it)->get_sections(path);
				list.insert(list.end(), itm.begin(), itm.end());
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
			MUTEX_GUARD();
			string_list ret;
			get_cached_keys_unsafe(path, ret);
			get_real_keys(path, ret);
			get_keys_from_child_unsafe(path, ret);
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
		void get_keys_from_child_unsafe(std::string path, string_list &list) {
			for (parent_list_type::iterator it = children_.begin(); it != children_.end(); ++it) {
				std::string str = (*it)->get_context();
				string_list itm = (*it)->get_keys(path);
				list.insert(list.end(), itm.begin(), itm.end());
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
			settings_core::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit != settings_cache_.end())
				return true;
			return has_real_key(lookup);
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
		virtual void save_to(std::string other) {
			instance_ptr i = get_core()->create_instance(other);
			save_to(i);
		}
		virtual void save_to(instance_ptr other) {
			if (!other)
				throw settings_exception("Cant migrate to NULL instance!");
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
				throw settings_exception("Failed to create new instance!");
			string_list list = get_sections(path);
			std::string subpath = path;
			// TODO: check trailing / instead!
			if (!subpath.empty())
				subpath += "/";
			for (string_list::const_iterator cit = list.begin();cit != list.end(); ++cit) {
				st_copy_section(subpath + *cit, other);
			}
			list = get_keys(path);
			for (string_list::const_iterator cit = list.begin();cit != list.end(); ++cit) {
				settings_core::key_path_type key(path, *cit);
				settings_core::key_type type = get_key_type(key.first, key.second);
				if (type ==settings_core::key_string) {
					settings_interface::op_string val = get_string(key.first, key.second);
					if (val)
						other->set_string(key.first, key.second, *val);
					else
						other->set_string(key.first, key.second, "");
				} else if (type ==settings_core::key_integer) {
					settings_interface::op_int val = get_int(key.first, key.second);
					if (val)
						other->set_int(key.first, key.second, *val);
					else
						other->set_int(key.first, key.second, 0);
				} else if (type ==settings_core::key_bool) {
					settings_interface::op_bool val = get_bool(key.first, key.second);
					if (val)
						other->set_bool(key.first, key.second, *val);
					else
						other->set_bool(key.first, key.second, false);
				} else
					throw settings_exception("Invalid type for key: " + key.first + "." + key.second);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Save the settings store
		///
		/// @author mickem
		virtual void save() {
			MUTEX_GUARD();

			BOOST_FOREACH(cache_key_type v, settings_delete_cache_) {
				remove_real_value(v);
			}
			BOOST_FOREACH(std::string v, settings_delete_path_cache_) {
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
			get_core()->set_dirty(false);
		}
		/////////////////////////////////////////////////////////////////////////
		/// Load from another settings store
		///
		/// @param other the other settings store to load from
		///
		/// @author mickem
		virtual void load_from(instance_ptr other) {
			throw settings_exception("TODO: FIX ME: load_from");
		}
		//////////////////////////////////////////////////////////////////////////
		/// Load from another context.
		/// The context is an identifier for the settings store for INI/XML it is the filename.
		///
		/// @param context the context to load from
		///
		/// @author mickem
		virtual void load_from(std::string context) {
			throw settings_exception("TODO: FIX ME: load_from");
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
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param key the key to lookup
		/// @return the int value
		///
		/// @author mickem
		virtual op_int get_real_int(settings_core::key_path_type key) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist exception will be thrown
		///
		/// @param key the key to lookup
		/// @return the boolean value
		///
		/// @author mickem
		virtual op_bool get_real_bool(settings_core::key_path_type key) = 0;

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

		//////////////////////////////////////////////////////////////////////////
		/// Create a new settings interface of "this kind"
		///
		/// @param context the context to use
		/// @return the newly created settings interface
		///
		/// @author mickem
		virtual settings_interface_impl* create_new_context(std::string context) = 0;

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
