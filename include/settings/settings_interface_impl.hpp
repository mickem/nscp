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

#include <types.hpp>
#include <Singleton.h>
#include <string>
#include <map>
#include <set>
#include <boost/thread/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>
#include <strEx.h>
#include <settings/settings_core.hpp>


#define MUTEX_GUARD() \
	boost::unique_lock<boost::timed_mutex> mutex(mutex_, boost::get_system_time() + boost::posix_time::seconds(5)); \
	if (!mutex.owns_lock()) \
		throw settings_exception(_T("Failed to get mutex, cant get settings instance"));


namespace settings {
	class SettingsInterfaceImpl : public settings_interface {
	protected:
		settings_core *core_;
		typedef std::list<instance_raw_ptr> parent_list_type;
		parent_list_type children_;
		boost::timed_mutex mutex_;
	public:
		struct conainer {
			settings_core::key_type type;
			int int_val;
			std::wstring string_val;
			conainer(int value) : type(settings_core::key_integer), int_val(value) {}
			conainer(bool value) : type(settings_core::key_bool), int_val(value?1:0) {}
			conainer(std::wstring value) : type(settings_core::key_string), string_val(value) {}
			conainer() : type(settings_core::key_string) {}

			std::wstring get_string() const {
				if (type==settings_core::key_string)
					return string_val;
				if (type==settings_core::key_integer)
					return strEx::itos(int_val);
				if (type==settings_core::key_bool)
					return int_val==1?_T("true"):_T("false");
				return _T("UNKNOWN TYPE");
			}
			int get_int() const {
				if (type==settings_core::key_string)
					return strEx::stoi(string_val);
				if (type==settings_core::key_integer)
					return int_val;
				if (type==settings_core::key_bool)
					return int_val==1?1:0;
				return -1;
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
		typedef std::map<cache_key_type,conainer> cache_type;
		typedef std::set<std::wstring> path_cache_type;
		typedef std::map<std::wstring,std::set<std::wstring> > key_cache_type;
		cache_type settings_cache_;
		path_cache_type path_cache_;
		key_cache_type key_cache_;
		std::wstring context_;
		net::url url_;

		//SettingsInterfaceImpl() : core_(NULL) {}
		SettingsInterfaceImpl(settings_core *core, std::wstring context) : core_(core), context_(context), url_(net::parse(context_)) {}

		//////////////////////////////////////////////////////////////////////////
		/// Empty all cached settings values and force a reload.
		/// Notice this does not save so anhy "active" values will be flushed and new ones read from file.
		///
		/// @author mickem
		void clear_cache() {
			MUTEX_GUARD();
			settings_cache_.clear();
			path_cache_.clear();
			key_cache_.clear();
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
				throw settings_exception(_T("FATAL ERROR: Settings subsystem not initialized"));
			return core_;
		}
		logger_interface* get_logger() const {
			if (core_ == NULL)
				throw settings_exception(_T("FATAL ERROR: Settings subsystem not initialized"));
			return core_->get_logger();
		}

		void add_child(std::wstring context) {
			MUTEX_GUARD();
			children_.push_back(get_core()->create_instance(context));
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual std::wstring get_string(std::wstring path, std::wstring key) {
			MUTEX_GUARD();
			settings_core::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				std::wstring val;
				try {
					val = get_real_string(lookup);
				} catch (KeyNotFoundException e) {
					val = get_string_from_child_unsafe(lookup);
				}
				settings_cache_[lookup] = val;
				return val;
			}
			return (*cit).second.get_string();
		}
		std::wstring get_string_from_child_unsafe(settings_core::key_path_type key) {
			for (parent_list_type::iterator it = children_.begin(); it != children_.end(); ++it) {
				try {
					return (*it)->get_string(key.first, key.second);
				} catch (KeyNotFoundException e) {
					continue;
				}
			}
			throw KeyNotFoundException(key);
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
		virtual std::wstring get_string(std::wstring path, std::wstring key, std::wstring def) {
			try {
				return get_string(path, key);
			} catch (KeyNotFoundException e) {
				MUTEX_GUARD();
				settings_cache_[cache_key_type(path,key)] = def;
				return def;
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Set or update a string value
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param value the value to set
		///
		/// @author mickem
		virtual void set_string(std::wstring path, std::wstring key, std::wstring value) {
			{
				MUTEX_GUARD();
				settings_cache_[cache_key_type(path,key)] = value;
			}
			add_key(path, key);
		}

		virtual void add_path(std::wstring path) {
			MUTEX_GUARD();
			path_cache_.insert(path);
		}
		virtual void add_key(std::wstring path, std::wstring key) {
			MUTEX_GUARD();
			key_cache_type::iterator it = key_cache_.find(path);
			if (it == key_cache_.end()) {
				std::set<std::wstring> s;
				s.insert(key);
				key_cache_[path] = s;
			} else {
				if ((*it).second.find(key) == (*it).second.end()) {
					(*it).second.insert(key);
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual int get_int(std::wstring path, std::wstring key) {
			MUTEX_GUARD();
			settings_core::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				int val;
				try {
					val = get_real_int(lookup);
				} catch (KeyNotFoundException e) {
					val = get_int_from_child_unsafe(path, key);
				}
				settings_cache_[lookup] = val;
				return val;
			}
			return (*cit).second.get_int();
		}
		int get_int_from_child_unsafe(std::wstring path, std::wstring key) {
			for (parent_list_type::iterator it = children_.begin(); it != children_.end(); ++it) {
				try {
					return (*it)->get_int(path, key);
				} catch (KeyNotFoundException e) {
					continue;
				}
			}
			throw KeyNotFoundException(path, key);
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
		virtual int get_int(std::wstring path, std::wstring key, int def) {
			try {
				return get_int(path, key);
			} catch (KeyNotFoundException e) {
				MUTEX_GUARD();
				settings_cache_[cache_key_type(path,key)] = def;
				return def;
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Set or update an integer value
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param value the value to set
		///
		/// @author mickem
		virtual void set_int(std::wstring path, std::wstring key, int value) {
			{
				MUTEX_GUARD();
				settings_cache_[cache_key_type(path,key)] = value;
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
		virtual settings_core::key_type get_key_type(std::wstring path, std::wstring key) {
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
		virtual bool get_bool(std::wstring path, std::wstring key) {
			MUTEX_GUARD();
			settings_core::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				bool val;
				try {
					val = get_real_bool(lookup);
				} catch (KeyNotFoundException e) {
					val = get_bool_from_child_unsafe(path, key);
				}
				settings_cache_[lookup] = val;
				return val;
			}
			return (*cit).second.get_bool();
		}
		bool get_bool_from_child_unsafe(std::wstring path, std::wstring key) {
			for (parent_list_type::iterator it = children_.begin(); it != children_.end(); ++it) {
				try {
					return (*it)->get_bool(path, key);
				} catch (KeyNotFoundException e) {
					continue;
				}
			}
			throw KeyNotFoundException(path, key);
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
		virtual bool get_bool(std::wstring path, std::wstring key, bool def) {
			try {
				return get_bool(path, key);
			} catch (KeyNotFoundException e) {
				MUTEX_GUARD();
				settings_cache_[cache_key_type(path,key)] = def;
				return def;
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Set or update a boolean value
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param value the value to set
		///
		/// @author mickem
		virtual void set_bool(std::wstring path, std::wstring key, bool value) {
			{
				MUTEX_GUARD();
				settings_cache_[cache_key_type(path,key)] = value;
			}
			add_key(path, key);
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
		virtual string_list get_sections(std::wstring path) {
			MUTEX_GUARD();
			get_core()->get_logger()->debug(__FILE__, __LINE__, std::wstring(_T("Get sections for: ")) + path);
			string_list ret;
			get_cached_sections_unsafe(path, ret);
			get_real_sections(path, ret);
			get_section_from_child_unsafe(path, ret);
			ret.sort();
			ret.unique();
			return ret;
		}
		void get_cached_sections_unsafe(std::wstring path, string_list &list) {
			if (path.empty()) {
				BOOST_FOREACH(std::wstring s, path_cache_) {
					if (s.length() > 1) {
						std::wstring::size_type pos = s.find(L'/', 1);
						if (pos != std::wstring::npos)
							list.push_back(s.substr(0,pos));
						else
							list.push_back(s);
					}
				}
			} else {
				std::wstring::size_type path_len = path.length();
				BOOST_FOREACH(std::wstring s, path_cache_) {
					std::wstring::size_type len = s.length();
					if (s.length() > (path_len+1) && s.substr(0,path_len) == path) {
						std::wstring::size_type pos = s.find(L'/', path_len+1);
						if (pos != std::wstring::npos)
							list.push_back(s.substr(path_len+1,pos));
						else
							list.push_back(s.substr(path_len+1));
					}
				}
			}
		}
		void get_section_from_child_unsafe(std::wstring path, string_list &list) {
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
		virtual string_list get_keys(std::wstring path) {
			MUTEX_GUARD();
			string_list ret;
			get_cached_keys_unsafe(path, ret);
			get_real_keys(path, ret);
			get_keys_from_child_unsafe(path, ret);
			ret.sort();
			ret.unique();
			return ret;
		}
		void get_cached_keys_unsafe(std::wstring path, string_list &list) {
			key_cache_type::iterator it = key_cache_.find(path);
			if (it != key_cache_.end()) {
				BOOST_FOREACH(std::wstring s, (*it).second) {
					list.push_back(s);
				}
			}
		}
		void get_keys_from_child_unsafe(std::wstring path, string_list &list) {
			for (parent_list_type::iterator it = children_.begin(); it != children_.end(); ++it) {
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
		virtual bool has_section(std::wstring path) {
			throw settings_exception(_T("TODO: FIX ME: has_section"));
		}
		//////////////////////////////////////////////////////////////////////////
		/// Does the key exists?
		/// 
		/// @param path The path of the section
		/// @param key The key to check
		/// @return true/false
		///
		/// @author mickem
		virtual bool has_key(std::wstring path, std::wstring key) {
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
		virtual std::wstring get_context() {
			MUTEX_GUARD();
			return context_;
		}
		virtual std::wstring get_file_from_context() {
			return core_->find_file(url_.host + url_.path, _T(""));
		}
		//////////////////////////////////////////////////////////////////////////
		/// Set the context.
		/// The context is an identifier for the settings store for INI/XML it is the filename.
		///
		/// @param context the new context
		///
		/// @author mickem
		virtual void set_context(std::wstring context) {
			MUTEX_GUARD();
			context_ = context;
		}

		// Save/Load Functions
		//////////////////////////////////////////////////////////////////////////
		/// Reload the settings store
		///
		/// @author mickem
		virtual void reload() {
			throw settings_exception(_T("TODO: FIX ME: reload"));
		}
		//////////////////////////////////////////////////////////////////////////
		/// Copy the settings store to another settings store
		///
		/// @param other the settings store to save to
		///
		/// @author mickem
		virtual void save_to(std::wstring other) {
			instance_ptr i = get_core()->create_instance(other);
			save_to(i);
		}
		virtual void save_to(instance_ptr other) {
			if (!other)
				throw settings_exception(_T("Cant migrate to NULL instance!"));
			other->clear_cache();
			st_copy_section(_T(""), other);
			other->save();
		}
		void st_copy_section(std::wstring path, instance_ptr other) {
			if (!other)
				throw settings_exception(_T("Failed to create new instance!"));
			string_list list = get_sections(path);
			std::wstring subpath = path;
			// TODO: check trailing / instead!
			if (!subpath.empty())
				subpath += _T("/");
			for (string_list::const_iterator cit = list.begin();cit != list.end(); ++cit) {
				st_copy_section(subpath + *cit, other);
			}
			list = get_keys(path);
			for (string_list::const_iterator cit = list.begin();cit != list.end(); ++cit) {
				settings_core::key_path_type key(path, *cit);
				settings_core::key_type type = get_key_type(key.first, key.second);
				if (type ==settings_core::key_string) {
					try {
						other->set_string(key.first, key.second, get_string(key.first, key.second));
					} catch (KeyNotFoundException e) {
						other->set_string(key.first, key.second, _T(""));
					}
				} else if (type ==settings_core::key_integer)
					other->set_int(key.first, key.second, get_int(key.first, key.second));
				else if (type ==settings_core::key_bool)
					other->set_bool(key.first, key.second, get_bool(key.first, key.second));
				else
					throw settings_exception(_T("Invalid type for key: ") + key.first + _T(".") + key.second);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Save the settings store
		///
		/// @author mickem
		virtual void save() {
			MUTEX_GUARD();
			BOOST_FOREACH(std::wstring path, path_cache_) {
				set_real_path(path);
			}
			std::set<std::wstring> sections;
			for (cache_type::const_iterator cit = settings_cache_.begin(); cit != settings_cache_.end(); ++cit) {
				set_real_value((*cit).first, (*cit).second);
				sections.insert((*cit).first.first);
			}
			BOOST_FOREACH(std::wstring str, get_core()->get_reg_sections()) {
				set_real_path(str);
			}
		}
		/////////////////////////////////////////////////////////////////////////
		/// Load from another settings store
		///
		/// @param other the other settings store to load from
		///
		/// @author mickem
		virtual void load_from(instance_ptr other) {
			throw settings_exception(_T("TODO: FIX ME: load_from"));
		}
		//////////////////////////////////////////////////////////////////////////
		/// Load from another context.
		/// The context is an identifier for the settings store for INI/XML it is the filename.
		///
		/// @param context the context to load from
		///
		/// @author mickem
		virtual void load_from(std::wstring context) {
			throw settings_exception(_T("TODO: FIX ME: load_from"));
		}
		//////////////////////////////////////////////////////////////////////////
		/// Load settings from the context.
		///
		/// @author mickem
		virtual void load() {
			throw settings_exception(_T("TODO: FIX ME: load"));
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
		virtual void get_real_sections(std::wstring path, string_list &list) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get all keys given a path/section.
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_keys(std::wstring path, string_list &list) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual std::wstring get_real_string(settings_core::key_path_type key) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param key the key to lookup
		/// @return the int value
		///
		/// @author mickem
		virtual int get_real_int(settings_core::key_path_type key) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist exception will be thrown
		///
		/// @param key the key to lookup
		/// @return the boolean value
		///
		/// @author mickem
		virtual bool get_real_bool(settings_core::key_path_type key) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_value(settings_core::key_path_type key, conainer value) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_path(std::wstring path) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Check if a key exists
		///
		/// @param key the key to lookup
		/// @return true/false if the key exists.
		///
		/// @author mickem
		virtual bool has_real_key(settings_core::key_path_type key) = 0;
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
		virtual SettingsInterfaceImpl* create_new_context(std::wstring context) = 0;


		virtual std::wstring to_string() {
			std::wstring ret = get_info();
			if (!children_.empty()) {
				ret += _T("parents = [");
				BOOST_FOREACH(parent_list_type::value_type i, children_) {
					ret += i->to_string();
				}
				ret += _T("]");
			}
			return ret + _T("}");
		}
	};
}
