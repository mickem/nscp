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
#include <nsclient/logger.hpp>

namespace settings {
	class settings_handler_impl : public settings_core {
	private:
		typedef std::map<std::wstring, std::wstring> path_map;
		typedef std::map<std::wstring,settings_core::path_description> reg_paths_type;
		typedef std::map<key_path_type,key_path_type> mapped_paths_type;
		typedef settings_interface::string_list string_list;
		
		instance_raw_ptr instance_;
		boost::timed_mutex instance_mutex_;
		boost::filesystem::wpath base_path_;
		reg_paths_type registred_paths_;

	public:
		settings_handler_impl() {}
		~settings_handler_impl() {
			destroy_all_instances();
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
		/// Get the logging interface (will receive log messages)
		///
		/// @return the logger to use
		///
		/// @author mickem
		nsclient::logging::logger_interface* get_logger() {
			return nsclient::logging::logger::get_logger();
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
			boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!mutex.owns_lock())
				throw settings_exception(_T("Failed to get mutex, cant get settings instance"));
			if (!instance_)
				throw settings_exception(_T("Failed initialize settings instance"));
			return instance_ptr(instance_);
		}
		instance_ptr get_no_wait() {
			boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::try_to_lock);
			if (!mutex.owns_lock())
				throw settings_exception(_T("Failed to get mutex, cant get settings instance"));
			if (!instance_)
				throw settings_exception(_T("Failed initialize settings instance"));
			return instance_;
// 			return instance_ptr(instance_, mutexHandler_, 0);
// 			int i = boost::try_lock(&mutexHandler_);
// 			if (i != -1)
// 				throw settings_exception(_T("Failed to get mutex, cant get settings instance"));
// 			if (!instance_)
// 				throw settings_exception(_T("Failed initialize settings instance"));
// 			return instance_;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Overwrite the (current) settings store with default values.
		///
		/// @author mickem
		void update_defaults() {
			BOOST_FOREACH(std::wstring path, get_reg_sections()) {
				get()->add_path(path);
				BOOST_FOREACH(std::wstring key, get_reg_keys(path)) {
					settings_core::key_description desc = get_registred_key(path, key);
					if (!desc.advanced) {
						if (!get()->has_key(path, key)) {
							get_logger()->debug(__FILE__, __LINE__, _T("Adding: ") + path + _T(".") + key);
							if (desc.type == key_string)
								get()->set_string(path, key, desc.defValue);
							else if (desc.type == key_bool)
								get()->set_bool(path, key, settings::settings_interface::string_to_bool(desc.defValue));
							else if (desc.type == key_integer) {
								try {
									get()->set_int(path, key, strEx::stoi(desc.defValue));
								} catch (const std::exception &e) {
									get_logger()->error(__FILE__, __LINE__, _T("invalid default value for: ") + path + _T(".") + key);
								}
							} else
								get_logger()->error(__FILE__, __LINE__, _T("Unknown keytype for: ") + path + _T(".") + key);
						} else {
							std::wstring val = get()->get_string(path, key);
							get_logger()->debug(__FILE__, __LINE__, _T("Setting old (already exists): ") + path + _T(".") + key + _T(" = ") + val);
							if (desc.type == key_string)
								get()->set_string(path, key, val);
							else if (desc.type == key_bool)
								get()->set_bool(path, key, settings::settings_interface::string_to_bool(val));
							else if (desc.type == key_integer)
								get()->set_int(path, key, strEx::stoi(val));
							else
								get_logger()->error(__FILE__, __LINE__, _T("Unknown keytype for: ") + path + _T(".") + key);
						}
					} else {
						get_logger()->debug(__FILE__, __LINE__, _T("Skipping (advanced): ") + path + _T(".") + key);
					}
				}
			}
			get_logger()->info(__FILE__, __LINE__, _T("DONE Updating settings with default values!"));
		}
		void migrate(instance_ptr from, instance_ptr to) {
			if (!from || !to)
				throw new settings_exception(_T("Source or target is null"));
			from->save_to(to);
			set_primary(to->get_context());
		}
		void migrate_to(instance_ptr to) {
			migrate(get(), to);
		}
		void migrate_from(instance_ptr from) {
			migrate(from, get());
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
			boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!mutex.owns_lock())
				throw settings_exception(_T("set_instance Failed to get mutext, cant get access settings"));
			instance_ = create_instance(key);
			if (!instance_)
				throw settings_exception(_T("set_instance Failed to create instance for: ") + key);
			instance_->set_core(this);
		}


	private:
		void destroy_all_instances() {
			boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!mutex.owns_lock())
				throw settings_exception(_T("destroy_all_instances Failed to get mutext, cant get access settings"));
			instance_.reset();
		}

		virtual std::wstring to_string() {
			if (instance_)
				return instance_->to_string();
			return _T("<NULL>");
		}

	};
	typedef settings_interface::string_list string_list;

}


