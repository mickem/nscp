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
#include <string>
#include <map>
#include <set>
#include <algorithm>

#include <boost/thread/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>
#include <strEx.h>
#define BUFF_LEN 4096

namespace settings {


	class settings_exception : public std::exception {
		std::string error_;
	public:
		//////////////////////////////////////////////////////////////////////////
		/// Constructor takes an error message.
		/// @param error the error message
		///
		/// @author mickem
		settings_exception(std::wstring error) : error_(utf8::cvt<std::string>(error)) {}
		settings_exception(std::string error) : error_(utf8::cvt<std::string>(error)) {}
		~settings_exception() throw() {}

		//////////////////////////////////////////////////////////////////////////
		/// Retrieve the error message from the exception.
		/// @return the error message
		///
		/// @author mickem
		std::wstring getError() const { return wwhat(); }
		std::wstring getMessage() const { return wwhat(); }
		const char* what() const throw() {
			return error_.c_str();
		}
		const std::wstring wwhat() const throw() {
			return utf8::to_unicode(error_);
		}
	};
	class KeyNotFoundException : public settings_exception {
	public:
		KeyNotFoundException(std::wstring path, std::wstring key) : settings_exception(_T("Key not found: ")+ path + _T(" ") + key) {}
		KeyNotFoundException(std::wstring path) : settings_exception(_T("Key not found: ")+ path) {}
		KeyNotFoundException(std::pair<std::wstring,std::wstring> key) : settings_exception(_T("Key not found: ")+ key.first + _T(" ") + key.second) {}
	};

	class settings_interface;
	typedef boost::shared_ptr<settings_interface> instance_ptr;
	typedef boost::shared_ptr<settings_interface> instance_raw_ptr;
	typedef std::list<std::wstring> error_list;

	class settings_core {
	public:
		typedef std::list<std::wstring> string_list;
		typedef enum {
			key_string = 100,
			key_integer = 200,
			key_bool = 300
		} key_type;
		typedef std::pair<std::wstring,std::wstring> key_path_type;
		struct key_description {
			std::wstring title;
			std::wstring description;
			key_type type;
			std::wstring defValue;
			bool advanced;
			std::set<unsigned int> plugins;
			key_description(unsigned int plugin_id, std::wstring title_, std::wstring description_, settings_core::key_type type_, std::wstring defValue_, bool advanced_) 
				: title(title_), description(description_), type(type_), defValue(defValue_), advanced(advanced_) { append_plugin(plugin_id); }
			key_description(unsigned int plugin_id) : type(settings_core::key_string), advanced(false) { append_plugin(plugin_id); }
			key_description() : type(settings_core::key_string), advanced(false) { }
			void append_plugin(unsigned int plugin_id) {
				plugins.insert(plugin_id);
			}
		};
		struct path_description {
			std::wstring title;
			std::wstring description;
			bool advanced;
			typedef std::map<std::wstring,key_description> keys_type;
			keys_type keys;
			std::set<unsigned int> plugins;
			path_description(unsigned int plugin_id, std::wstring title_, std::wstring description_, bool advanced_) : title(title_), description(description_), advanced(advanced_) { append_plugin(plugin_id); }
			path_description(unsigned int plugin_id) : advanced(false) { append_plugin(plugin_id); }
			path_description() : advanced(false) {  }
			void update(unsigned int plugin_id, std::wstring title_, std::wstring description_, bool advanced_) {
				title = title_;
				description = description_;
				advanced = advanced_;
				append_plugin(plugin_id);
			}
			void append_plugin(unsigned int plugin_id) {
				plugins.insert(plugin_id);
			}
		};

		struct mapped_key {
			mapped_key(key_path_type src_, key_path_type dst_) : src(src_), dst(dst_) {}
			key_path_type src;
			key_path_type dst;
		};
		typedef std::list<mapped_key> mapped_key_list_type;

		//////////////////////////////////////////////////////////////////////////
		/// Register a path with the settings module.
		/// A registered key or path will be nicely documented in some of the settings files when converted.
		///
		/// @param path The path to register
		/// @param title The title to use
		/// @param description the description to use
		/// @param advanced advanced options will only be included if they are changed
		///
		/// @author mickem
		virtual void register_path(unsigned int plugin_id, std::wstring path, std::wstring title, std::wstring description, bool advanced = false) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Register a key with the settings module.
		/// A registered key or path will be nicely documented in some of the settings files when converted.
		///
		/// @param path The path to register
		/// @param key The key to register
		/// @param type The type of value
		/// @param title The title to use
		/// @param description the description to use
		/// @param defValue the default value
		/// @param advanced advanced options will only be included if they are changed
		///
		/// @author mickem
		virtual void register_key(unsigned int plugin_id, std::wstring path, std::wstring key, key_type type, std::wstring title, std::wstring description, std::wstring defValue, bool advanced = false) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get info about a registered key.
		/// Used when writing settings files.
		///
		/// @param path The path of the key
		/// @param key The key of the key
		/// @return the key description
		///
		/// @author mickem
		virtual key_description get_registred_key(std::wstring path, std::wstring key) = 0;

		virtual settings_core::path_description get_registred_path(const std::wstring &path) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Get all registered sections
		///
		/// @return a list of section paths
		///
		/// @author mickem
		virtual string_list get_reg_sections() = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get all keys for a registered section.
		///
		/// @param path the path to find keys under
		/// @return a list of key names
		///
		/// @author mickem
		virtual string_list get_reg_keys(std::wstring path) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Get the currently active settings interface.
		///
		/// @return the currently active interface
		///
		/// @author mickem
		virtual instance_ptr get() = 0;
		virtual instance_ptr get_no_wait() = 0;
		
		//////////////////////////////////////////////////////////////////////////
		/// Get a settings interface
		///
		/// @param type the type of settings interface to get
		/// @return the settings interface
		///
		/// @author mickem
		//virtual settings_interface* get(settings_core::settings_type type) = 0;
		// Conversion Functions
		virtual void migrate(instance_ptr from, instance_ptr to) = 0;
		virtual void migrate_to(instance_ptr to) = 0;
		virtual void migrate_from(instance_ptr from) = 0;
		virtual void migrate(std::wstring from, std::wstring to) = 0;
		virtual void migrate_to(std::wstring to) = 0;
		virtual void migrate_from(std::wstring from) = 0;

		virtual void set_primary(std::wstring context) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Validate the settings store and report all missing/invalid and superflous keys.
		///
		/// @author mickem
		virtual settings::error_list validate() = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Overwrite the (current) settings store with default values.
		///
		/// @author mickem
		virtual void update_defaults() = 0;
		virtual void remove_defaults() = 0;




		
		//////////////////////////////////////////////////////////////////////////
		/// Boot the settings subsystem from the given file (boot.ini).
		///
		/// @param file the file to use when booting.
		///
		/// @author mickem
		virtual void boot(std::wstring file = _T("boot.ini")) = 0;

		virtual std::wstring find_file(std::wstring file, std::wstring fallback) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Get a string form the boot file.
		///
		/// @param section section to read a value from.
		/// @param key the key to read.
		/// @param def a default value.
		/// @return the value of the key or the default value.
		///
		/// @author mickem
		virtual std::wstring get_boot_string(std::wstring section, std::wstring key, std::wstring def) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Create an instance of a given type.
		/// Used internally to create instances of various settings types.
		///
		/// @param context the context to use
		/// @return a new instance of given type.
		///
		/// @author mickem
		virtual instance_raw_ptr create_instance(std::wstring context) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Set the basepath for the settings subsystem.
		/// In other words set where the settings files reside
		///
		/// @param path the path to the settings files
		///
		/// @author mickem
		virtual void set_base(boost::filesystem::wpath path) = 0;

		virtual std::wstring to_string() = 0;

		virtual std::wstring expand_path(std::wstring key) = 0;

	};

	class settings_interface {
	public:
		typedef std::list<std::wstring> string_list;

		//////////////////////////////////////////////////////////////////////////
		/// Set the core module to use
		///
		/// @param core The core to use
		///
		/// @author mickem
		virtual void set_core(settings_core *core) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Empty all cached settings values and force a reload.
		/// Notice this does not save so any "active" values will be flushed and new ones read from file.
		///
		/// @author mickem
		virtual void clear_cache() = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Get the type of a key (String, int, bool)
		///
		/// @param path the path to get type for
		/// @param key the key to get the type for
		/// @return the type of the key
		///
		/// @author mickem
		virtual settings_core::key_type get_key_type(std::wstring path, std::wstring key) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual std::wstring get_string(std::wstring path, std::wstring key) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist the default value will be returned
		/// 
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param def the default value to use when no value is found
		/// @return the string value
		///
		/// @author mickem
		virtual std::wstring get_string(std::wstring path, std::wstring key, std::wstring def) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Set or update a string value
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param value the value to set
		///
		/// @author mickem
		virtual void set_string(std::wstring path, std::wstring key, std::wstring value) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual int get_int(std::wstring path, std::wstring key) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist the default value will be returned
		/// 
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param def the default value to use when no value is found
		/// @return the string value
		///
		/// @author mickem
		virtual int get_int(std::wstring path, std::wstring key, int def) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Set or update an integer value
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param value the value to set
		///
		/// @author mickem
		virtual void set_int(std::wstring path, std::wstring key, int value) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual bool get_bool(std::wstring path, std::wstring key) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist the default value will be returned
		/// 
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param def the default value to use when no value is found
		/// @return the string value
		///
		/// @author mickem
		virtual bool get_bool(std::wstring path, std::wstring key, bool def) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Set or update a boolean value
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @param value the value to set
		///
		/// @author mickem
		virtual void set_bool(std::wstring path, std::wstring key, bool value) = 0;

		virtual void remove_key(std::wstring path, std::wstring key) = 0;
		virtual void remove_path(std::wstring path) = 0;

		// Meta Functions
		//////////////////////////////////////////////////////////////////////////
		/// Get all (sub) sections (given a path).
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @return a list of sections
		///
		/// @author mickem
		virtual string_list get_sections(std::wstring path) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get all keys for a path.
		///
		/// @param path The path to get keys under
		/// @return a list of keys
		///
		/// @author mickem
		virtual string_list get_keys(std::wstring path) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Does the section exists?
		/// 
		/// @param path The path of the section
		/// @return true/false
		///
		/// @author mickem
		virtual bool has_section(std::wstring path) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Does the key exists?
		/// 
		/// @param path The path of the section
		/// @param key The key to check
		/// @return true/false
		///
		/// @author mickem
		virtual bool has_key(std::wstring path, std::wstring key) = 0;

		virtual void add_path(std::wstring path) = 0;
		// Misc Functions
		//////////////////////////////////////////////////////////////////////////
		/// Get a context.
		/// The context is an identifier for the settings store for INI/XML it is the filename.
		///
		/// @return the context
		///
		/// @author mickem
		virtual std::wstring get_context() = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Set the context.
		/// The context is an identifier for the settings store for INI/XML it is the filename.
		///
		/// @param context the new context
		///
		/// @author mickem
		virtual void set_context(std::wstring context) = 0;

		// Save/Load Functions
		//////////////////////////////////////////////////////////////////////////
		/// Reload the settings store
		///
		/// @author mickem
		virtual void reload() = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Copy the settings store to another settings store
		///
		/// @param other the settings store to save to
		///
		/// @author mickem
		virtual void save_to(instance_ptr other) = 0;
		virtual void save_to(std::wstring other) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Save the settings store
		///
		/// @author mickem
		virtual void save() = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Load from another settings store
		///
		/// @param other the other settings store to load from
		///
		/// @author mickem
		virtual void load_from(instance_ptr other) = 0;
		virtual void load_from(std::wstring other) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Load settings from the context.
		///
		/// @author mickem
		virtual void load() = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Validate the settings store and report all missing/invalid and superflous keys.
		///
		/// @author mickem
		virtual settings::error_list validate() = 0;

		virtual std::wstring to_string() = 0;

		virtual std::wstring get_info() = 0;

		static bool string_to_bool(std::wstring str) {
			std::transform(str.begin(), str.end(), str.begin(), ::tolower);
			return str == _T("true")||str == _T("1");
		}

		virtual std::list<boost::shared_ptr<settings_interface> > get_children() = 0;
	};

}
