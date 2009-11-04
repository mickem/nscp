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

#include <Singleton.h>
#include <string>
#include <map>
#include <Mutex.h>
#define BUFF_LEN 4096

namespace Settings {
	class SettingsException {
		std::wstring error_;
	public:
		//////////////////////////////////////////////////////////////////////////
		/// Constructor takes an error message.
		/// @param error the error message
		///
		/// @author mickem
		SettingsException(std::wstring error) : error_(error) {}

		//////////////////////////////////////////////////////////////////////////
		/// Retrieve the error message from the exception.
		/// @return the error message
		///
		/// @author mickem
		std::wstring getError() const { return error_; }
		std::wstring getMessage() const { return error_; }
	};
	class KeyNotFoundException : public SettingsException {
	public:
		KeyNotFoundException(std::wstring path, std::wstring key) : SettingsException(_T("Key not found: ")+ path + _T(" ") + key) {}
		KeyNotFoundException(std::wstring path) : SettingsException(_T("Key not found: ")+ path) {}
		KeyNotFoundException(std::pair<std::wstring,std::wstring> key) : SettingsException(_T("Key not found: ")+ key.first + _T(" ") + key.second) {}
	};

	class LoggerInterface {
		public:
			//////////////////////////////////////////////////////////////////////////
			/// Log an ERROR message.
			///
			/// @param file the file where the event happened
			/// @param line the line where the event happened
			/// @param message the message to log
			///
			/// @author mickem
			virtual void err(std::wstring file, int line, std::wstring message) = 0;
			//////////////////////////////////////////////////////////////////////////
			/// Log an WARNING message.
			///
			/// @param file the file where the event happened
			/// @param line the line where the event happened
			/// @param message the message to log
			///
			/// @author mickem
			virtual void warn(std::wstring file, int line, std::wstring message) = 0;
			//////////////////////////////////////////////////////////////////////////
			/// Log an INFO message.
			///
			/// @param file the file where the event happened
			/// @param line the line where the event happened
			/// @param message the message to log
			///
			/// @author mickem
			virtual void info(std::wstring file, int line, std::wstring message) = 0;
			//////////////////////////////////////////////////////////////////////////
			/// Log an DEBUG message.
			///
			/// @param file the file where the event happened
			/// @param line the line where the event happened
			/// @param message the message to log
			///
			/// @author mickem
			virtual void debug(std::wstring file, int line, std::wstring message) = 0;

			void quick_debug(std::wstring message) {
				debug(__FILEW__, __LINE__, message);
			}
	};

	class SettingsInterface;
	class SettingsCore {
	public:
		typedef std::list<std::wstring> string_list;
		typedef enum {
			registry,
			old_ini_file,
			ini_file,
			xml_file,
		} settings_type;
		typedef enum {
			key_string = 100,
			key_integer = 200,
			key_bool = 300,
		} key_type;
		typedef std::pair<std::wstring,std::wstring> key_path_type;
		struct key_description {
			std::wstring title;
			std::wstring description;
			key_type type;
			std::wstring defValue;
			bool advanced;
			key_description(std::wstring title_, std::wstring description_, SettingsCore::key_type type_, std::wstring defValue_, bool advanced_) 
				: title(title_), description(description_), type(type_), defValue(defValue_), advanced(advanced_) {}
			key_description() : advanced(false), type(SettingsCore::key_string) {}
		};
		struct mapped_key {
			mapped_key(key_path_type src_, key_path_type dst_) : src(src_), dst(dst_) {}
			key_path_type src;
			key_path_type dst;
		};
		typedef std::list<mapped_key> mapped_key_list_type;
		//////////////////////////////////////////////////////////////////////////
		/// Convert a string to a settings type
		///
		/// @param key the string representing a settings type
		/// @return the settings type
		///
		/// @author mickem
		static settings_type string_to_type(std::wstring key) {
			if (key == _T("ini"))
				return ini_file;
			if (key == _T("registry"))
				return registry;
			if (key == _T("xml"))
				return xml_file;
			return old_ini_file;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Convert a string to a settings type
		///
		/// @param key the string representing a settings type
		/// @return the settings type
		///
		/// @author mickem
		static std::wstring type_to_string(settings_type type) {
			if (type == ini_file)
				return _T("ini");
			if (type == registry)
				return _T("registry");
			if (type == xml_file)
				return _T("xml");
			return _T("old");
		}

		//////////////////////////////////////////////////////////////////////////
		/// Resolve a path or key to any potential mappings.
		///
		/// @param path the path to resolve
		/// @return the resolved new key
		///
		/// @author mickem
		//virtual std::wstring map_path(std::wstring path) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Resolve a path or key to any potential mappings.
		///
		/// @param key the key to resolve
		/// @return the resolved new path and key
		///
		/// @author mickem
		//virtual key_path_type map_key(key_path_type key) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Find all mapped keys given a path
		///
		/// @param path the path to resolve
		/// @return A list of resolved keys
		///
		/// @author mickem
		//virtual mapped_key_list_type find_maped_keys(std::wstring path) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Get all mapped sections.
		///
		/// @param path path to get mapped sections from
		/// @return
		///
		/// @author mickem
		//virtual string_list get_mapped_sections(std::wstring path) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Reverse resolve a path or key to any potential mappings.
		///
		/// @param path the path to resolve
		/// @return the resolved new key
		///
		/// @author mickem
		//virtual std::wstring reverse_map_path(std::wstring path) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Reverse resolve a path or key to any potential mappings.
		///
		/// @param key the key to resolve
		/// @return the resolved new path and key
		///
		/// @author mickem
		//virtual key_path_type reverse_map_key(key_path_type key) = 0;

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
		//virtual void add_mapping(std::wstring source_path, std::wstring source_key, std::wstring destination_path, std::wstring destination_key) = 0;

		//////////////////////////////////////////////////////////////////////////
		///Map a path to another path.
		///ie add_mapping("hej","foo"); means looking up hej/xxx will get you the value from foo/xxx
		///
		///@param source_path The path to map (look here)
		///@param destination_path The path to bind to (to end up here)
		///
		///@author mickem
		//virtual void add_mapping(std::wstring source_path, std::wstring destination_path) = 0;

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
		virtual void register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced = false) = 0;

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
		virtual void register_key(std::wstring path, std::wstring key, key_type type, std::wstring title, std::wstring description, std::wstring defValue, bool advanced = false) = 0;
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
		virtual SettingsInterface* get() = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get a settings interface
		///
		/// @param type the type of settings interface to get
		/// @return the settings interface
		///
		/// @author mickem
		virtual SettingsInterface* get(SettingsCore::settings_type type) = 0;
		// Conversion Functions
		virtual void migrate_type(SettingsCore::settings_type from, SettingsCore::settings_type to) = 0;
		virtual void migrate_to(SettingsCore::settings_type to) = 0;
		virtual void migrate_from(SettingsCore::settings_type from) = 0;
		virtual void copy_type(SettingsCore::settings_type from, SettingsCore::settings_type to) = 0;
		virtual SettingsCore::settings_type get_settings_type() = 0;
		virtual std::wstring get_settings_type_desc() = 0;
		virtual std::wstring get_type_desc(SettingsCore::settings_type type) = 0;
		virtual bool has_type(settings_type type) = 0;
		virtual void set_type(settings_type type) = 0;
		//virtual void add_instance(settings_type type) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Overwrite the (current) settings store with default values.
		///
		/// @author mickem
		virtual void update_defaults() = 0;




		
		//////////////////////////////////////////////////////////////////////////
		/// Boot the settings subsystem from the given file (boot.ini).
		///
		/// @param file the file to use when booting.
		///
		/// @author mickem
		virtual void boot(std::wstring file = _T("boot.ini")) = 0;

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
		/// @param type the type to create
		/// @param context the context to use
		/// @return a new instance of given type.
		///
		/// @author mickem
		virtual SettingsInterface* create_instance(settings_type type, std::wstring context) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Set the basepath for the settings subsystem.
		/// In other words set where the settings files reside
		///
		/// @param path the path to the settings files
		///
		/// @author mickem
		virtual void set_base(std::wstring path) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Get the basepath for the settings subsystem.
		/// In other words get where the settings files reside
		///
		/// @return the path to the settings files
		///
		/// @author mickem
		virtual std::wstring get_base() = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Set the logging interface (will receive log messages)
		///
		/// @param logger the new logger to use
		///
		/// @author mickem
		virtual void set_logger(LoggerInterface *logger) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get the logging interface (will receive log messages)
		///
		/// @return the logger to use
		///
		/// @author mickem
		virtual LoggerInterface* get_logger() = 0;

	};

	class SettingsInterface {
	public:
		typedef std::list<std::wstring> string_list;

		//////////////////////////////////////////////////////////////////////////
		/// Set the core module to use
		///
		/// @param core The core to use
		///
		/// @author mickem
		virtual void set_core(SettingsCore *core) = 0;

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
		virtual SettingsCore::key_type get_key_type(std::wstring path, std::wstring key) = 0;

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
		//////////////////////////////////////////////////////////////////////////
		/// Get the type this settings store represent.
		///
		/// @return the type of settings store
		///
		/// @author mickem
		virtual SettingsCore::settings_type get_type() = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Is this the active settings store
		///
		/// @return
		///
		/// @author mickem
		virtual bool is_active() = 0;

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
		virtual void save_to(SettingsInterface* other) = 0;
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
		virtual void load_from(SettingsInterface* other) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Load settings from the context.
		///
		/// @author mickem
		virtual void load() = 0;
	};

	class SettingsHandlerImpl : public SettingsCore {
	private:
		typedef std::map<SettingsCore::settings_type,SettingsInterface*> instance_list;
		SettingsInterface* instance_;
		instance_list instances_;
		MutexHandler mutexHandler_;
		/*
		struct key_description : public SettingsCore::key_description {
			std::wstring title;
			std::wstring description;
			SettingsCore::key_type type;
			std::wstring defValue;
			bool advanced;
			key_description(std::wstring title_, std::wstring description_, SettingsCore::key_type type_, std::wstring defValue_, bool advanced_) 
			: title(title_), description(description_), type(type_), defValue(defValue_), advanced(advanced_) {}
			key_description() : advanced(false), type(SettingsCore::key_string) {}
		};
		*/
		struct path_description {
			std::wstring title;
			std::wstring description;
			bool advanced;
			typedef std::map<std::wstring,key_description> keys_type;
			keys_type keys;
			path_description(std::wstring title_, std::wstring description_, bool advanced_) : title(title_), description(description_), advanced(advanced_) {}
			path_description() : advanced(false) {}
			void update(std::wstring title_, std::wstring description_, bool advanced_) {
				title = title_;
				description = description_;
				advanced = advanced_;
			}
		};
		class dummy_logger : public LoggerInterface {
			void err(std::wstring file, int line, std::wstring message) {}
			void warn(std::wstring file, int line, std::wstring message) {}
			void info(std::wstring file, int line, std::wstring message) {}
			void debug(std::wstring file, int line, std::wstring message) {}
		};
		typedef std::map<std::wstring, std::wstring> path_map;
		//path_map path_mappings_;
		//path_map reversed_path_mappings_;
		std::wstring base_path_;
		LoggerInterface *logger_;
		typedef std::map<std::wstring,path_description> reg_paths_type;
		reg_paths_type registred_paths_;
		typedef std::map<key_path_type,key_path_type> mapped_paths_type;
		//mapped_paths_type mapped_paths_;
		//mapped_paths_type reversed_mapped_paths_;
		typedef SettingsInterface::string_list string_list;
	public:
		SettingsHandlerImpl() : instance_(NULL), logger_(new dummy_logger()) {}
		~SettingsHandlerImpl() {
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
		void set_base(std::wstring path) {
			base_path_ = path;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Set the logging interface (will receive log messages)
		///
		/// @param logger the new logger to use
		///
		/// @author mickem
		void set_logger(LoggerInterface *logger) {
			LoggerInterface *old_logger = logger_;
			logger_ = logger;
			delete old_logger;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get the logging interface (will receive log messages)
		///
		/// @return the logger to use
		///
		/// @author mickem
		LoggerInterface* get_logger() {
			if (!logger_)
				throw SettingsException(_T("Failed to log message, no logger defined!"));
			return logger_;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get the basepath for the settings subsystem.
		/// In other words get where the settings files reside
		///
		/// @return the path to the settings files
		///
		/// @author mickem
		std::wstring get_base() {
			return base_path_;
		}


		SettingsInterface* get() {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw SettingsException(_T("Failed to get mutext, cant get settings instance"));
			if (instance_ == NULL)
				instance_ = get_default_settings_instance_unsafe();
			if (instance_ == NULL)
				throw SettingsException(_T("Failed initialize settings instance"));
			return instance_;
		}
		SettingsInterface* get(SettingsCore::settings_type type) {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw SettingsException(_T("Failed to get mutext, cant get settings instance"));
			return instance_unsafe(type);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Overwrite the (current) settings store with default values.
		///
		/// @author mickem
		void update_defaults() {
			get_logger()->warn(__FILEW__, __LINE__, _T("Updating settings with default values!"));
			string_list s = get_reg_sections();
			for (string_list::const_iterator cit = s.begin(); cit != s.end(); ++cit) {
				string_list k = get_reg_keys(*cit);
				for (string_list::const_iterator citk = k.begin(); citk != k.end(); ++citk) {
					SettingsCore::key_description desc = get_registred_key(*cit, *citk);
					if (!desc.advanced) {
						if (!get()->has_key(*cit, *citk)) {
							get_logger()->debug(__FILEW__, __LINE__, _T("Adding: ") + *cit + _T(".") + *citk);
							if (desc.type == key_string)
								get()->set_string(*cit, *citk, desc.defValue);
							else if (desc.type == key_bool)
								get()->set_bool(*cit, *citk, desc.defValue==_T("true"));
							else if (desc.type == key_integer)
								get()->set_int(*cit, *citk, strEx::stoi(desc.defValue));
							else
								throw SettingsException(_T("Unknown keytype for: ") + *cit + _T(".") + *citk);
						} else {
							get_logger()->debug(__FILEW__, __LINE__, _T("´Skipping (already exists): ") + *cit + _T(".") + *citk);
						}
					} else {
						get_logger()->debug(__FILEW__, __LINE__, _T("´Skipping (advanced): ") + *cit + _T(".") + *citk);
					}
				}
			}
			get_logger()->info(__FILEW__, __LINE__, _T("DONE Updating settings with default values!"));
		}
		void migrate_type(SettingsCore::settings_type from, SettingsCore::settings_type to) {
#ifdef _DEBUG
			get_logger()->debug(__FILEW__, __LINE__, _T("Preparing to migrate..."));
#endif
			{
				if (!has_type(from)) {
#ifdef _DEBUG
					get_logger()->debug(__FILEW__, __LINE__, _T("Migration needs source..."));
#endif
					add_type_impl(from, create_instance(from, type_to_string(from)));
				}
				if (!has_type(to)) {
#ifdef _DEBUG
					get_logger()->debug(__FILEW__, __LINE__, _T("Migration needs target..."));
#endif
					add_type_impl(to, create_instance(to, type_to_string(to)));
				}
			}
			{
#ifdef _DEBUG
				get_logger()->debug(__FILEW__, __LINE__, _T("Starting to migrate..."));
#endif
				MutexLock mutex(mutexHandler_);
				if (!mutex.hasMutex())
					throw SettingsException(_T("migrate_type: Failed to get mutext, cant get settings instance"));
				SettingsInterface* iFrom = instance_unsafe(from);
				SettingsInterface* iTo = instance_unsafe(to);
				if (iTo == NULL||iFrom == NULL)
					throw new SettingsException(_T("Failed to migrate"));
				iFrom->save_to(iTo);
#ifdef _DEBUG
				get_logger()->debug(__FILEW__, __LINE__, _T("Done migrating..."));
#endif
			}
		}
		void migrate_to(SettingsCore::settings_type to) {
			migrate_type(get_settings_type(), to);
		}
		void migrate_from(SettingsCore::settings_type from) {
			migrate_type(from, get_settings_type());
		}
		SettingsCore::settings_type get_settings_type() {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw SettingsException(_T("Failed to get mutext, cant get load settings"));
			if (instance_ == NULL)
				throw SettingsException(_T("No settings subsystem selected"));
			return instance_->get_type();
		}
		std::wstring get_settings_type_desc() {
			return get_type_desc(get_settings_type());
		}
		std::wstring get_type_desc(SettingsCore::settings_type type) {
			if (type == SettingsCore::ini_file)
				return _T(".INI file (nsc.ini)");
			if (type == SettingsCore::registry)
				return _T("registry");
			if (type == SettingsCore::xml_file)
				return _T(".XML file (nsc.xml)");
			return _T("Unknown settings type");
		}
		bool has_type(SettingsCore::settings_type type) {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw SettingsException(_T("has_type Failed to get mutext, cant get access settings"));
			instance_list::const_iterator cit = instances_.find(type);
			return cit != instances_.end();
		}
		void set_type(SettingsCore::settings_type type) {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw SettingsException(_T("set_type Failed to get mutext, cant get access settings"));
			instance_list::const_iterator cit = instances_.find(type);
			if (cit == instances_.end())
				throw SettingsException(_T("Settings was not supported: ") + get_type_desc(type));
			instance_ = (*cit).second;
		}
		void add_type_impl(SettingsCore::settings_type type, SettingsInterface* impl) {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw SettingsException(_T("add_type_impl Failed to get mutext, cant get access settings"));
			instance_list::iterator it = instances_.find(type);
			if (it == instances_.end()) {
				instances_[type] = impl;
			} else {
				SettingsInterface* old = (*it).second;
				(*it).second = impl;
				// TODO: Potential race condition (if we are using the code when we delete it)
				delete old;
			}
		}

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
				throw SettingsException(_T("map_path Failed to get mutext, cant get access settings"));
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
				throw SettingsException(_T("map_path Failed to get mutext, cant get access settings"));
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
				throw SettingsException(_T("find_maped_keys Failed to get mutext, cant get access settings"));
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
				throw SettingsException(_T("map_path Failed to get mutext, cant get access settings"));
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
				throw SettingsException(_T("map_path Failed to get mutext, cant get access settings"));
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
				throw SettingsException(_T("add_mapping Failed to get mutext, cant get access settings"));
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
				throw SettingsException(_T("add_mapping Failed to get mutext, cant get access settings"));
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
		void register_key(std::wstring path, std::wstring key, SettingsCore::key_type type, std::wstring title, std::wstring description, std::wstring defValue, bool advanced = false) {
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
		SettingsCore::key_description get_registred_key(std::wstring path, std::wstring key) {
			reg_paths_type::const_iterator cit = registred_paths_.find(path);
			if (cit != registred_paths_.end()) {
				path_description::keys_type::const_iterator cit2 = (*cit).second.keys.find(key);
				if (cit2 != (*cit).second.keys.end()) {
					SettingsCore::key_description ret = (*cit2).second;
					return ret;
				}
			}
			throw KeyNotFoundException(path, key);
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


		void add_instance(SettingsInterface *instance) {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw SettingsException(_T("load_all_instance Failed to get mutext, cant get access settings"));
			instance_list::iterator it = instances_.find(instance->get_type());
			if (it == instances_.end())
				instances_[instance->get_type()] = instance;
			else {
				SettingsInterface *old = (*it).second;
				instances_[instance->get_type()] = instance;
				delete old;
			}
			instance->set_core(this);
		}

		void copy_type(SettingsCore::settings_type from, SettingsCore::settings_type to) {
			throw SettingsException(_T("copy_type not implemented"));
		}



	private:
		void destroy_all_instances() {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw SettingsException(_T("destroy_all_instances Failed to get mutext, cant get access settings"));
			instance_ = NULL;
			for (instance_list::iterator it = instances_.begin(); it != instances_.end(); ++it) {
				SettingsInterface *tmp = (*it).second;
				if (tmp != NULL)
					tmp->set_core(NULL);
				(*it).second = NULL;
				delete tmp;
			}
			instances_.clear();
		}
		SettingsInterface *get_default_settings_instance_unsafe() {
			MutexLock mutex(mutexHandler_);
			if (!mutex.hasMutex())
				throw SettingsException(_T("destroy_all_instances Failed to get mutext, cant get access settings"));
			instance_ = NULL;
			for (instance_list::iterator it = instances_.begin(); it != instances_.end(); ++it) {
				if ( (*it).second->is_active() )
					return (*it).second;
			}
			throw SettingsException(_T("Could not find any active settings system"));
		}
		SettingsInterface* instance_unsafe(SettingsCore::settings_type type) {
			instance_list::const_iterator cit = instances_.find(type);
			if (cit == instances_.end())
				throw SettingsException(_T("Failed to find settings for: ") + get_type_desc(type));
			return (*cit).second;
		}
	};
	//typedef Singleton<SettingsHandlerImpl> SettingsHandler;
	typedef SettingsInterface::string_list string_list;

/*
	// Alias to make handling "compatible" with old syntax
	SettingsInterface* get_settings() {
		return SettingsHandler::getInstance()->get();
	}
	SettingsInterface* getInstance() {
		return SettingsHandler::getInstance()->get();
	}
	SettingsCore* get_core() {
		return SettingsHandler::getInstance();
	}
	void destroyInstance() {
		SettingsHandler::destroyInstance();
	}
*/

	class SettingsInterfaceImpl : public SettingsInterface {
		SettingsCore *core_;
		typedef std::list<SettingsInterface*> parent_list_type;
		std::list<SettingsInterface*> parents_;
	public:
		static bool string_to_bool(std::wstring str) {
			return str == _T("true")||str == _T("1");
		}
		struct conainer {
			SettingsCore::key_type type;
			int int_val;
			std::wstring string_val;
			conainer(int value) : type(SettingsCore::key_integer), int_val(value) {}
			conainer(bool value) : type(SettingsCore::key_bool), int_val(value?1:0) {}
			conainer(std::wstring value) : type(SettingsCore::key_string), string_val(value) {}
			conainer() : type(SettingsCore::key_string) {}

			std::wstring get_string() const {
				if (type==SettingsCore::key_string)
					return string_val;
				if (type==SettingsCore::key_integer)
					return strEx::itos(int_val);
				if (type==SettingsCore::key_bool)
					return int_val==1?_T("true"):_T("false");
				return _T("UNKNOWN TYPE");
			}
			int get_int() const {
				if (type==SettingsCore::key_string)
					return strEx::stoi(string_val);
				if (type==SettingsCore::key_integer)
					return int_val;
				if (type==SettingsCore::key_bool)
					return int_val==1?TRUE:FALSE;
				return -1;
			}
			bool get_bool() const {
				if (type==SettingsCore::key_string)
					return string_to_bool(string_val);
				if (type==SettingsCore::key_integer)
					return int_val==1?true:false;
				if (type==SettingsCore::key_bool)
					return int_val==1?true:false;
				return false;
			}
		};
		typedef SettingsCore::key_path_type cache_key_type;
		typedef std::map<cache_key_type,conainer> cache_type;
		cache_type settings_cache_;
		std::wstring context_;

		//SettingsInterfaceImpl() : core_(NULL) {}
		SettingsInterfaceImpl(SettingsCore *core, std::wstring context) : core_(core), context_(context) {}

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
		virtual void set_core(SettingsCore *core) {
			core_ = core;
		}
		SettingsCore* get_core() const {
			if (core_ == NULL)
				throw SettingsException(_T("FATAL ERROR: Settings subsystem not initialized"));
			return core_;
		}

		void add_parent(SettingsInterface *parent) {
			parents_.push_back(parent);
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
			SettingsCore::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				std::wstring val;
				try {
					val = get_real_string(lookup);
				} catch (KeyNotFoundException e) {
					val = get_string_from_parent(lookup);
				}
				settings_cache_[lookup] = val;
				return val;
			}
			return (*cit).second.get_string();
		}
		std::wstring get_string_from_parent(SettingsCore::key_path_type key) {
			for (parent_list_type::iterator it = parents_.begin(); it != parents_.end(); ++it) {
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

		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual int get_int(std::wstring path, std::wstring key) {
			SettingsCore::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				int val;
				try {
					val = get_real_int(lookup);
				} catch (KeyNotFoundException e) {
					val = get_int_from_parent(path, key);
				}
				settings_cache_[lookup] = val;
				return val;
			}
			return (*cit).second.get_int();
		}
		int get_int_from_parent(std::wstring path, std::wstring key) {
			for (parent_list_type::iterator it = parents_.begin(); it != parents_.end(); ++it) {
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
		virtual SettingsCore::key_type get_key_type(std::wstring path, std::wstring key) {
			throw SettingsException(_T("TODO: FIX ME: get_key_type"));
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
			SettingsCore::key_path_type lookup(path,key);
			cache_type::const_iterator cit = settings_cache_.find(lookup);
			if (cit == settings_cache_.end()) {
				bool val;
				try {
					val = get_real_bool(lookup);
				} catch (KeyNotFoundException e) {
					val = get_bool_from_parent(path, key);
				}
				settings_cache_[lookup] = val;
				return val;
			}
			return (*cit).second.get_bool();
		}
		bool get_bool_from_parent(std::wstring path, std::wstring key) {
			for (parent_list_type::iterator it = parents_.begin(); it != parents_.end(); ++it) {
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
			get_section_from_parent(path, ret);
			ret.sort();
			ret.unique();
			return ret;
		}
		void get_section_from_parent(std::wstring path, string_list &list) {
			for (parent_list_type::iterator it = parents_.begin(); it != parents_.end(); ++it) {
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
			get_keys_from_parent(path, ret);
			ret.sort();
			ret.unique();
			return ret;
		}
		void get_keys_from_parent(std::wstring path, string_list &list) {
			for (parent_list_type::iterator it = parents_.begin(); it != parents_.end(); ++it) {
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
			throw SettingsException(_T("TODO: FIX ME: has_section"));
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
			SettingsCore::key_path_type lookup(path,key);
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
			throw SettingsException(_T("TODO: FIX ME: reload"));
		}
		//////////////////////////////////////////////////////////////////////////
		/// Copy the settings store to another settings store
		///
		/// @param other the settings store to save to
		///
		/// @author mickem
		virtual void save_to(SettingsInterface* other) {
#ifdef _DEBUG
			get_core()->get_logger()->debug(__FILEW__, __LINE__, std::wstring(_T("Migrating settings...")));
#endif
			if (other == NULL)
				throw SettingsException(_T("Cant migrate to NULL instance!"));
			other->clear_cache();
			st_copy_section(_T(""), other);
			other->save();
		}
		void st_copy_section(std::wstring path, SettingsInterface *other) {
#ifdef _DEBUG
			get_core()->get_logger()->debug(__FILEW__, __LINE__, std::wstring(_T("st_copy_section: ") + path));
#endif
			if (other == NULL)
				throw SettingsException(_T("Failed to create new instance!"));
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
				SettingsCore::key_path_type key(path, *cit);
				SettingsCore::key_type type = get_key_type(key.first, key.second);
				if (type ==SettingsCore::key_string) {
					try {
						other->set_string(key.first, key.second, get_string(key.first, key.second));
					} catch (KeyNotFoundException e) {
						other->set_string(key.first, key.second, _T(""));
					}
				} else if (type ==SettingsCore::key_integer)
					other->set_int(key.first, key.second, get_int(key.first, key.second));
				else if (type ==SettingsCore::key_bool)
					other->set_bool(key.first, key.second, get_bool(key.first, key.second));
				else
					throw SettingsException(_T("Invalid type for key: ") + key.first + _T(".") + key.second);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Save the settings store
		///
		/// @author mickem
		virtual void save() {
			for (cache_type::const_iterator cit = settings_cache_.begin(); cit != settings_cache_.end(); ++cit) {
				set_real_value((*cit).first, (*cit).second);
			}
		}
		/////////////////////////////////////////////////////////////////////////
		/// Load from another settings store
		///
		/// @param other the other settings store to load from
		///
		/// @author mickem
		virtual void load_from(SettingsInterface* other) {
			throw SettingsException(_T("TODO: FIX ME: load_from"));
		}
		//////////////////////////////////////////////////////////////////////////
		/// Load from another context.
		/// The context is an identifier for the settings store for INI/XML it is the filename.
		///
		/// @param context the context to load from
		///
		/// @author mickem
		virtual void load_from(std::wstring context) {
			throw SettingsException(_T("TODO: FIX ME: load_from"));
		}
		//////////////////////////////////////////////////////////////////////////
		/// Load settings from the context.
		///
		/// @author mickem
		virtual void load() {
			throw SettingsException(_T("TODO: FIX ME: load"));
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
		virtual std::wstring get_real_string(SettingsCore::key_path_type key) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param key the key to lookup
		/// @return the int value
		///
		/// @author mickem
		virtual int get_real_int(SettingsCore::key_path_type key) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist exception will be thrown
		///
		/// @param key the key to lookup
		/// @return the boolean value
		///
		/// @author mickem
		virtual bool get_real_bool(SettingsCore::key_path_type key) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_value(SettingsCore::key_path_type key, conainer value) = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Check if a key exists
		///
		/// @param key the key to lookup
		/// @return true/false if the key exists.
		///
		/// @author mickem
		virtual bool has_real_key(SettingsCore::key_path_type key) = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Get the type this settings store represent.
		///
		/// @return the type of settings store
		///
		/// @author mickem
		virtual SettingsCore::settings_type get_type() = 0;
		//////////////////////////////////////////////////////////////////////////
		/// Is this the active settings store
		///
		/// @return
		///
		/// @author mickem
		virtual bool is_active() = 0;

		//////////////////////////////////////////////////////////////////////////
		/// Create a new settings interface of "this kind"
		///
		/// @param context the context to use
		/// @return the newly created settings interface
		///
		/// @author mickem
		virtual SettingsInterfaceImpl* create_new_context(std::wstring context) = 0;
	};
}
typedef Settings::SettingsException SettingsException;

