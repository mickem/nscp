#pragma once

#include <string>
#include <map>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>
//#define SI_CONVERT_ICU
#include <simpleini/simpleini.h>
#include <error.hpp>

namespace settings {
	class settings_dummy : public settings::settings_interface_impl {
	private:

	public:
		settings_dummy(settings::settings_core *core, std::string context) : settings::settings_interface_impl(core, context) {}
		//////////////////////////////////////////////////////////////////////////
		/// Create a new settings interface of "this kind"
		///
		/// @param context the context to use
		/// @return the newly created settings interface
		///
		/// @author mickem
		virtual settings_interface_impl* create_new_context(std::string context) {
			return new settings_dummy(get_core(), context);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual op_string get_real_string(settings_core::key_path_type key) {
			return op_string();
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the int value
		///
		/// @author mickem
		virtual op_int get_real_int(settings_core::key_path_type key) {
			return op_int();
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the boolean value
		///
		/// @author mickem
		virtual op_bool get_real_bool(settings_core::key_path_type key) {
			return op_bool();
		}
		//////////////////////////////////////////////////////////////////////////
		/// Check if a key exists
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return true/false if the key exists.
		///
		/// @author mickem
		virtual bool has_real_key(settings_core::key_path_type key) {
			return false;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_value(settings_core::key_path_type key, conainer value) {
		}

		virtual void set_real_path(std::string path) {
		}
		virtual void remove_real_value(settings_core::key_path_type key) {
		}
		virtual void remove_real_path(std::string path) {
		}

		//////////////////////////////////////////////////////////////////////////
		/// Get all (sub) sections (given a path).
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_sections(std::string, string_list &) {
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get all keys given a path/section.
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_keys(std::string, string_list &) {
		}
		//////////////////////////////////////////////////////////////////////////
		/// Save the settings store
		///
		/// @author mickem
		virtual void save() {
			settings_interface_impl::save();
		}
		virtual std::string get_info() {
			return "dummy settings";
		}
		settings::error_list validate() {
			settings::error_list ret;
			return ret;
		}
		virtual std::string get_type() { return "dummy"; }

		public:
			virtual void real_clear_cache() {}
			static bool context_exists(settings::settings_core*, std::string) {
				return true;
			}
			void ensure_exists() {
			}
	};
}
