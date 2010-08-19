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

namespace settings {
	typedef boost::shared_ptr<settings_interface> instance_ptr;
	class settings_handler_impl : public settings_core {
	private:
		//typedef std::map<settings_core::settings_type,settings_interface*> instance_list;
		//typedef std::list<settings_interface*> instance_list;
		instance_ptr instance_;
		//instance_list instances_;
		boost::timed_mutex mutexHandler_;
		/*
		struct key_description : public settings_core::key_description {
			std::wstring title;
			std::wstring description;
			settings_core::key_type type;
			std::wstring defValue;
			bool advanced;
			key_description(std::wstring title_, std::wstring description_, settings_core::key_type type_, std::wstring defValue_, bool advanced_) 
			: title(title_), description(description_), type(type_), defValue(defValue_), advanced(advanced_) {}
			key_description() : advanced(false), type(settings_core::key_string) {}
		};
		*/
		class dummy_logger : public logger_interface {
			void err(std::wstring file, int line, std::wstring message) {}
			void warn(std::wstring file, int line, std::wstring message) {}
			void info(std::wstring file, int line, std::wstring message) {}
			void debug(std::wstring file, int line, std::wstring message) {}
		};
		typedef std::map<std::wstring, std::wstring> path_map;
		//path_map path_mappings_;
		//path_map reversed_path_mappings_;
		boost::filesystem::wpath base_path_;
		logger_interface *logger_;
		typedef std::map<std::wstring,settings_core::path_description> reg_paths_type;
		reg_paths_type registred_paths_;
		typedef std::map<key_path_type,key_path_type> mapped_paths_type;
		//mapped_paths_type mapped_paths_;
		//mapped_paths_type reversed_mapped_paths_;
		typedef settings_interface::string_list string_list;
	public:
		settings_handler_impl() : logger_(new dummy_logger()) {}
		~settings_handler_impl() {
			destroy_all_instances();
			set_logger(NULL);
		}

		//////////////////////////////////////////////////////////////////////////
		/// Set the basepath for the settings subsystem.
		/// In other words set where the settings files reside
		///
		/// @param path the path to the settings files
		///
		/// @author mickem
		void set_base(boost::filesystem::wpath path) {
			base_path_ = path;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Set the logging interface (will receive log messages)
		///
		/// @param logger the new logger to use
		///
		/// @author mickem
		void set_logger(logger_interface *logger) {
			logger_interface *old_logger = logger_;
			logger_ = logger;
			delete old_logger;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get the logging interface (will receive log messages)
		///
		/// @return the logger to use
		///
		/// @author mickem
		logger_interface* get_logger() {
			if (!logger_)
				throw settings_exception(_T("Failed to log message, no logger defined!"));
			return logger_;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get the basepath for the settings subsystem.
		/// In other words get where the settings files reside
		///
		/// @return the path to the settings files
		///
		/// @author mickem
		boost::filesystem::wpath get_base() {
			return base_path_;
		}


		instance_ptr get() {
			boost::unique_lock<boost::timed_mutex> mutex(mutexHandler_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!mutex.owns_lock())
				throw settings_exception(_T("Failed to get mutext, cant get settings instance"));
			if (!instance_)
				throw settings_exception(_T("Failed initialize settings instance"));
			return instance_;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Overwrite the (current) settings store with default values.
		///
		/// @author mickem
		void update_defaults() {
			get_logger()->warn(__FILEW__, __LINE__, _T("Updating settings with default values!"));
			BOOST_FOREACH(std::wstring path, get_reg_sections()) {
				get()->add_path(path);
				BOOST_FOREACH(std::wstring key, get_reg_keys(path)) {
					settings_core::key_description desc = get_registred_key(path, key);
					if (!desc.advanced) {
						if (!get()->has_key(path, key)) {
							get_logger()->debug(__FILEW__, __LINE__, _T("Adding: ") + path + _T(".") + key);
							if (desc.type == key_string)
								get()->set_string(path, key, desc.defValue);
							else if (desc.type == key_bool)
								get()->set_bool(path, key, desc.defValue==_T("true"));
							else if (desc.type == key_integer)
								get()->set_int(path, key, strEx::stoi(desc.defValue));
							else
								get_logger()->err(__FILEW__, __LINE__, _T("Unknown keytype for: ") + path + _T(".") + key);
						} else {
							std::wstring val = get()->get_string(path, key);
							get_logger()->debug(__FILEW__, __LINE__, _T("Setting old (already exists): ") + path + _T(".") + key + _T(" = ") + val);
							if (desc.type == key_string)
								get()->set_string(path, key, val);
							else if (desc.type == key_bool)
								get()->set_bool(path, key, val==_T("true"));
							else if (desc.type == key_integer)
								get()->set_int(path, key, strEx::stoi(val));
							else
								get_logger()->err(__FILEW__, __LINE__, _T("Unknown keytype for: ") + path + _T(".") + key);
						}
					} else {
						get_logger()->debug(__FILEW__, __LINE__, _T("Skipping (advanced): ") + path + _T(".") + key);
					}
				}
			}
			get_logger()->info(__FILEW__, __LINE__, _T("DONE Updating settings with default values!"));
		}
		void migrate(instance_ptr from, instance_ptr to) {
			if (!from || !to)
				throw new settings_exception(_T("Source or target is null"));
			{
#ifdef _DEBUG
				get_logger()->debug(__FILEW__, __LINE__, _T("Starting to migrate..."));
#endif
				boost::unique_lock<boost::timed_mutex> mutex(mutexHandler_, boost::get_system_time() + boost::posix_time::seconds(5));
				if (!mutex.owns_lock())
					throw settings_exception(_T("migrate_type: Failed to get mutex, cant get settings instance"));
				from->save_to(to);
#ifdef _DEBUG
				get_logger()->debug(__FILEW__, __LINE__, _T("Done migrating..."));
#endif
			}
		}
		void migrate_to(instance_ptr to) {
			migrate(instance_, to);
		}
		void migrate_from(instance_ptr from) {
			migrate(from, instance_);
		}
		void migrate_to(std::wstring to) {
			instance_ptr i = create_instance(to);
			migrate_to(i);
		}
		void migrate_from(std::wstring from) {
			instance_ptr i = create_instance(from);
			migrate_from(i);
		}
		void migrate(std::wstring from, std::wstring to) {
			instance_ptr ifrom = create_instance(from);
			instance_ptr ito = create_instance(to);
			migrate(ifrom, ito);
		}

// 		settings_core::settings_type get_settings_type() {
// 			boost::unique_lock<boost::timed_mutex> mutex(mutexHandler_, boost::get_system_time() + boost::posix_time::seconds(5));
// 			if (!mutex.owns_lock())
// 				throw settings_exception(_T("Failed to get mutext, cant get load settings"));
// 			if (instance_ == NULL)
// 				throw settings_exception(_T("No settings subsystem selected"));
// 			return instance_->get_type();
// 		}
// 		std::wstring get_settings_type_desc() {
// 			return get_type_desc(get_settings_type());
// 		}
// 		std::wstring get_type_desc(settings_core::settings_type type) {
// 			if (type == settings_core::ini_file)
// 				return _T(".INI file (nsc.ini)");
// 			if (type == settings_core::registry)
// 				return _T("registry");
// 			if (type == settings_core::xml_file)
// 				return _T(".XML file (nsc.xml)");
// 			return _T("Unknown settings type");
// 		}

		//////////////////////////////////////////////////////////////////////////
		/// Resolve a path or key to any potential mappings.
		///
		/// @param path the path to resolve
		/// @return the resolved new key
		///
		/// @author mickem
		/*
		std::wstring map_path(std::wstring path) {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw settings_exception(_T("map_path Failed to get mutext, cant get access settings"));
			path_map::const_iterator cit = path_mappings_.find(path);
			if (cit == path_mappings_.end())
				return path;
			return (*cit).second;
		}
		*/

		//////////////////////////////////////////////////////////////////////////
		/// Resolve a path or key to any potential mappings.
		///
		/// @param path the path to resolve
		/// @param key the key to resolve
		/// @return the resolved new path and key
		///
		/// @author mickem
		/*
		key_path_type map_key(key_path_type key) {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw settings_exception(_T("map_path Failed to get mutext, cant get access settings"));
			mapped_paths_type::const_iterator cit = mapped_paths_.find(key);
			if (cit != mapped_paths_.end())
				return (*cit).second;
			key.first = map_path(key.first);
			return key;
		}
		*/
		//////////////////////////////////////////////////////////////////////////
		/// Find all mapped keys given a path
		///
		/// @param path the path to resolve
		/// @return A list of resolved keys
		///
		/// @author mickem
		/*
		mapped_key_list_type find_maped_keys(std::wstring path) {
			mapped_key_list_type ret;
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw settings_exception(_T("find_maped_keys Failed to get mutext, cant get access settings"));
			for (mapped_paths_type::const_iterator cit = mapped_paths_.begin(); cit != mapped_paths_.end();++cit) {
				if ((*cit).first.first == path) {
					ret.push_back(mapped_key((*cit).first, (*cit).second));
				}
			}
			return ret;
		}
		*/

		//////////////////////////////////////////////////////////////////////////
		/// Reverse resolve a path or key to any potential mappings.
		///
		/// @param path the path to resolve
		/// @return the resolved new key
		///
		/// @author mickem
		/*
		virtual std::wstring reverse_map_path(std::wstring path) {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw settings_exception(_T("map_path Failed to get mutext, cant get access settings"));
			path_map::const_iterator cit = reversed_path_mappings_.find(path);
			if (cit == reversed_path_mappings_.end())
				return path;
			return (*cit).second;
		}
		inline void int_add_mapped(std::wstring::size_type &plen, const std::wstring &path, const std::wstring &string, string_list &list) {
			if (string.length() > plen && string.substr(0,plen) == path) {
				if (path[plen] == '/')
					plen++;
				std::wstring rest = string.substr(plen);
				std::wstring::size_type pos = rest.find('/');
				if (pos != std::wstring::npos)
					rest = rest.substr(pos);
				list.push_back(rest);
			}
		}
		inline void int_add_mapped_2(const std::wstring &string, string_list &list) {
			if (string.length() > 1) {
				std::wstring::size_type pos = string.find('/');
				if (pos != std::wstring::npos)
					list.push_back(string);
				else
					list.push_back(string.substr(0, pos));
			}
		}
		

		//////////////////////////////////////////////////////////////////////////
		/// Get all mapped sections.
		///
		/// @param path path to get mapped ssections from
		/// @return
		///
		/// @author mickem
		virtual string_list get_mapped_sections(std::wstring path) {
			string_list ret;
			if (path.empty() || path == _T("//")) {
				for (mapped_paths_type::const_iterator cit = mapped_paths_.begin(); cit != mapped_paths_.end(); ++cit) {
					int_add_mapped_2((*cit).first.first, ret);
				}
				for (path_map::const_iterator cit = path_mappings_.begin(); cit != path_mappings_.end(); ++cit) {
					int_add_mapped_2((*cit).first, ret);
				}
			} else {
				std::wstring::size_type plen = path.length();
				for (mapped_paths_type::const_iterator cit = mapped_paths_.begin(); cit != mapped_paths_.end(); ++cit) {
					int_add_mapped(plen, path, (*cit).first.first, ret);
				}
				for (path_map::const_iterator cit = path_mappings_.begin(); cit != path_mappings_.end(); ++cit) {
					int_add_mapped(plen, path, (*cit).first, ret);
				}
			}
			ret.sort();
			ret.unique();
			return ret;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Reverse resolve a path or key to any potential mappings.
		///
		/// @param key the key to resolve
		/// @return the resolved new path and key
		///
		/// @author mickem
		virtual key_path_type reverse_map_key(key_path_type key) {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw settings_exception(_T("map_path Failed to get mutext, cant get access settings"));
			mapped_paths_type::const_iterator cit = reversed_mapped_paths_.find(key);
			if (cit == reversed_mapped_paths_.end())
				return key;
			return (*cit).second;
		}





		//////////////////////////////////////////////////////////////////////////
		/// Map a path/key to another path/key.
		/// ie add_mapping("hej","san","foo","bar"); means looking up hej/san will get you the value from foo/bar
		///
		/// @param source_path The path to map (look here)
		/// @param source_key The node to map
		/// @param destination_path The path to bind to (to end up here)
		/// @param destination_key The key to bind to
		///
		/// @author mickem
		void add_mapping(std::wstring source_path, std::wstring source_key, std::wstring destination_path, std::wstring destination_key) {
			key_path_type src(source_path, source_key);
			key_path_type dst(destination_path, destination_key);
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw settings_exception(_T("add_mapping Failed to get mutext, cant get access settings"));
			mapped_paths_[src] = dst;
			mapped_paths_type::iterator cit = reversed_mapped_paths_.find(dst);
			if (cit == reversed_mapped_paths_.end())
				reversed_mapped_paths_[dst] = src;
			else
				reversed_mapped_paths_.erase(cit);
		}

		//////////////////////////////////////////////////////////////////////////
		///Map a path to another path.
		///ie add_mapping("hej","foo"); means looking up hej/xxx will get you the value from foo/xxx
		///
		///@param source_path The path to map (look here)
		///@param destination_path The path to bind to (to end up here)
		///
		///@author mickem
		void add_mapping(std::wstring source_path, std::wstring destination_path) {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw settings_exception(_T("add_mapping Failed to get mutext, cant get access settings"));
			path_mappings_[source_path] = destination_path;
			path_map::iterator cit = reversed_path_mappings_.find(destination_path);
			if (cit == reversed_path_mappings_.end())
				reversed_path_mappings_[destination_path] = source_path;
			else
				reversed_path_mappings_.erase(cit);
		}
		*/



		//////////////////////////////////////////////////////////////////////////
		/// Register a path with the settings module.
		/// A registered key or path will be nicely documented in some of the settings files when converted.
		///
		/// @param path The path to register
		/// @param type The type of value
		/// @param title The title to use
		/// @param description the description to use
		/// @param defValue the default value
		/// @param advanced advanced options will only be included if they are changed
		///
		/// @author mickem
		void register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced = false) {
			reg_paths_type::iterator it = registred_paths_.find(path);
			if (it == registred_paths_.end()) {
				registred_paths_[path] = path_description(title, description, advanced);
			} else {
				(*it).second.update(title, description, advanced);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		/// Register a key with the settings module.
		/// A registered key or path will be nicely documented in some of the settings files when converted.
		///
		/// @param path The path to register
		/// @param key The key to register
		/// @param title The title to use
		/// @param description the description to use
		/// @param defValue the default value
		/// @param advanced advanced options will only be included if they are changed
		///
		/// @author mickem
		void register_key(std::wstring path, std::wstring key, settings_core::key_type type, std::wstring title, std::wstring description, std::wstring defValue, bool advanced = false) {
			reg_paths_type::iterator it = registred_paths_.find(path);
			if (it == registred_paths_.end()) {
				registred_paths_[path] = path_description();
				registred_paths_[path].keys[key] = key_description(title, description, type, defValue, advanced);
			} else {
				(*it).second.keys[key] = key_description(title, description, type, defValue, advanced);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get info about a registered key.
		/// Used when writing settings files.
		///
		/// @param path The path of the key
		/// @param key The key of the key
		/// @return the key description
		///
		/// @author mickem
		settings_core::key_description get_registred_key(std::wstring path, std::wstring key) {
			reg_paths_type::const_iterator cit = registred_paths_.find(path);
			if (cit != registred_paths_.end()) {
				path_description::keys_type::const_iterator cit2 = (*cit).second.keys.find(key);
				if (cit2 != (*cit).second.keys.end()) {
					settings_core::key_description ret = (*cit2).second;
					return ret;
				}
			}
			throw KeyNotFoundException(path, key);
		}
		settings_core::path_description get_registred_path(std::wstring path) {
			reg_paths_type::const_iterator cit = registred_paths_.find(path);
			if (cit != registred_paths_.end()) {
				return (*cit).second;
			}
			throw KeyNotFoundException(path);
		}



		//////////////////////////////////////////////////////////////////////////
		/// Get all registered sections
		///
		/// @return a list of section paths
		///
		/// @author mickem
		string_list get_reg_sections() {
			string_list ret;
			for (reg_paths_type::const_iterator cit = registred_paths_.begin(); cit != registred_paths_.end(); ++cit) {
				ret.push_back((*cit).first);
			}
			return ret;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get all keys for a registered section.
		///
		/// @param path the path to find keys under
		/// @return a list of key names
		///
		/// @author mickem
		virtual string_list get_reg_keys(std::wstring path) {
			string_list ret;
			reg_paths_type::const_iterator cit = registred_paths_.find(path);
			if (cit != registred_paths_.end()) {
				for (path_description::keys_type::const_iterator cit2 = (*cit).second.keys.begin();cit2 != (*cit).second.keys.end(); ++cit2) {
					ret.push_back((*cit2).first);
				}
				return ret;
			}
			throw KeyNotFoundException(path, _T(""));
		}


		void set_instance(std::wstring key) {
			boost::unique_lock<boost::timed_mutex> mutex(mutexHandler_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!mutex.owns_lock())
				throw settings_exception(_T("set_instance Failed to get mutext, cant get access settings"));
			instance_ = create_instance(key);
			if (!instance_)
				throw settings_exception(_T("set_instance Failed to create instance for: ") + key);
// 			instance_list::iterator it = instances_.find(instance->get_type());
// 			if (it == instances_.end())
// 				instances_[instance->get_type()] = instance;
// 			else {
// 				settings_interface *old = (*it).second;
// 				instances_[instance->get_type()] = instance;
// 				delete old;
// 			}
			instance_->set_core(this);
		}

// 		void add_instance(settings_interface *instance) {
// 			boost::unique_lock<boost::timed_mutex> mutex(mutexHandler_, boost::get_system_time() + boost::posix_time::seconds(5));
// 			if (!mutex.owns_lock())
// 				throw settings_exception(_T("load_all_instance Failed to get mutext, cant get access settings"));
// 			instance_list::iterator it = instances_.find(instance->get_type());
// 			if (it == instances_.end())
// 				instances_[instance->get_type()] = instance;
// 			else {
// 				settings_interface *old = (*it).second;
// 				instances_[instance->get_type()] = instance;
// 				delete old;
// 			}
// 			instance->set_core(this);
// 		}

// 		void copy_type(settings_core::settings_type from, settings_core::settings_type to) {
// 			throw settings_exception(_T("copy_type not implemented"));
// 		}



	private:
		void destroy_all_instances() {
			boost::unique_lock<boost::timed_mutex> mutex(mutexHandler_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!mutex.owns_lock())
				throw settings_exception(_T("destroy_all_instances Failed to get mutext, cant get access settings"));
			instance_.reset();
// 			for (instance_list::iterator it = instances_.begin(); it != instances_.end(); ++it) {
// 				settings_interface *tmp = (*it).second;
// 				if (tmp != NULL)
// 					tmp->set_core(NULL);
// 				(*it).second = NULL;
// 				delete tmp;
// 			}
// 			instances_.clear();
		}
// 		settings_interface* instance_unsafe(settings_core::settings_type type) {
// 			instance_list::const_iterator cit = instances_.find(type);
// 			if (cit == instances_.end())
// 				throw settings_exception(_T("Failed to find settings for: ") + get_type_desc(type));
// 			return (*cit).second;
// 		}


		virtual std::wstring to_string() {
			if (instance_)
				return instance_->to_string();
			return _T("<NULL>");
		}

	};
	//typedef Singleton<SettingsHandlerImpl> SettingsHandler;
	typedef settings_interface::string_list string_list;

/*
	// Alias to make handling "compatible" with old syntax
	settings_interface* get_settings() {
		return SettingsHandler::getInstance()->get();
	}
	settings_interface* getInstance() {
		return SettingsHandler::getInstance()->get();
	}
	settings_core* get_core() {
		return SettingsHandler::getInstance();
	}
	void destroyInstance() {
		SettingsHandler::destroyInstance();
	}
*/

	class SettingsInterfaceImpl : public settings_interface {
	protected:
		settings_core *core_;
		typedef std::list<instance_ptr > parent_list_type;
		parent_list_type children_;
	public:
		static bool string_to_bool(std::wstring str) {
			return str == _T("true")||str == _T("1");
		}
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
		cache_type settings_cache_;
		path_cache_type path_cache_;
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
			settings_cache_.clear();
		}

		//////////////////////////////////////////////////////////////////////////
		/// Set the core module to use
		///
		/// @param core The core to use
		///
		/// @author mickem
		virtual void set_core(settings_core *core) {
			core_ = core;
		}
		settings_core* get_core() const {
			if (core_ == NULL)
				throw settings_exception(_T("FATAL ERROR: Settings subsystem not initialized"));
			return core_;
		}

		void add_child(instance_ptr child) {
			children_.push_back(child);
		}
		void add_child(std::wstring context) {
			add_child(get_core()->create_instance(context));
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
			settings_core::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				std::wstring val;
				try {
					val = get_real_string(lookup);
				} catch (KeyNotFoundException e) {
					val = get_string_from_child(lookup);
				}
				settings_cache_[lookup] = val;
				return val;
			}
			return (*cit).second.get_string();
		}
		std::wstring get_string_from_child(settings_core::key_path_type key) {
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
			settings_cache_[cache_key_type(path,key)] = value;
		}

		virtual void add_path(std::wstring path) {
			path_cache_.insert(path);
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
			settings_core::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				int val;
				try {
					val = get_real_int(lookup);
				} catch (KeyNotFoundException e) {
					val = get_int_from_child(path, key);
				}
				settings_cache_[lookup] = val;
				return val;
			}
			return (*cit).second.get_int();
		}
		int get_int_from_child(std::wstring path, std::wstring key) {
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
			settings_cache_[cache_key_type(path,key)] = value;
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
			throw settings_exception(_T("TODO: FIX ME: get_key_type"));
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
			settings_core::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				bool val;
				try {
					val = get_real_bool(lookup);
				} catch (KeyNotFoundException e) {
					val = get_bool_from_child(path, key);
				}
				settings_cache_[lookup] = val;
				return val;
			}
			return (*cit).second.get_bool();
		}
		bool get_bool_from_child(std::wstring path, std::wstring key) {
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
			settings_cache_[cache_key_type(path,key)] = value;
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
			get_core()->get_logger()->debug(__FILEW__, __LINE__, std::wstring(_T("Get sections for: ")) + path);
			string_list ret;
			get_real_sections(path, ret);
			get_section_from_child(path, ret);
			ret.sort();
			ret.unique();
			return ret;
		}
		void get_section_from_child(std::wstring path, string_list &list) {
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
			string_list ret;
			get_real_keys(path, ret);
			get_keys_from_child(path, ret);
			ret.sort();
			ret.unique();
			return ret;
		}
		void get_keys_from_child(std::wstring path, string_list &list) {
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
			return context_;
		}
		virtual std::wstring get_file_from_context() {
			return core_->find_file(url_.host + url_.path, DEFAULT_CONF_OLD_LOCATION);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Set the context.
		/// The context is an identifier for the settings store for INI/XML it is the filename.
		///
		/// @param context the new context
		///
		/// @author mickem
		virtual void set_context(std::wstring context) {
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
#ifdef _DEBUG
			get_core()->get_logger()->debug(__FILEW__, __LINE__, std::wstring(_T("Migrating settings...")));
#endif
			if (other == NULL)
				throw settings_exception(_T("Cant migrate to NULL instance!"));
			other->clear_cache();
			st_copy_section(_T(""), other);
			other->save();
		}
		void st_copy_section(std::wstring path, instance_ptr other) {
#ifdef _DEBUG
			get_core()->get_logger()->debug(__FILEW__, __LINE__, std::wstring(_T("st_copy_section: ") + path));
#endif
			if (other == NULL)
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
typedef settings::settings_exception settings_exception;

