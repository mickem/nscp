#pragma once

#include <string>
#include <map>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>

#include <error.hpp>

//#define SI_CONVERT_ICU
//#define SI_CONVERT_GENERIC
#include <simpleini/simpleini.h>

namespace settings {
	class INISettings : public settings::SettingsInterfaceImpl {
	private:
		CSimpleIni ini;
		bool is_loaded_;
		std::string filename_;

	public:
		INISettings(settings::settings_core *core, std::string context) : settings::SettingsInterfaceImpl(core, context), ini(false, false, false), is_loaded_(false) {
			load_data();
		}
		//////////////////////////////////////////////////////////////////////////
		/// Create a new settings interface of "this kind"
		///
		/// @param context the context to use
		/// @return the newly created settings interface
		///
		/// @author mickem
		virtual SettingsInterfaceImpl* create_new_context(std::string context) {
			return new INISettings(get_core(), context);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a string value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the string value
		///
		/// @author mickem
		virtual std::string get_real_string(settings_core::key_path_type key) {
			load_data();
			const wchar_t *val = ini.GetValue(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(), NULL);
			if (val == NULL)
				throw KeyNotFoundException(key);
			return utf8::cvt<std::string>(val);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get an integer value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the int value
		///
		/// @author mickem
		virtual int get_real_int(settings_core::key_path_type key) {
			std::string str = get_real_string(key);
			return strEx::s::stox<int>(str);
		}
		//////////////////////////////////////////////////////////////////////////
		/// Get a boolean value if it does not exist exception will be thrown
		///
		/// @param path the path to look up
		/// @param key the key to lookup
		/// @return the boolean value
		///
		/// @author mickem
		virtual bool get_real_bool(settings_core::key_path_type key) {
			std::string str = get_real_string(key);
			return SettingsInterfaceImpl::string_to_bool(str);
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
			return ini.GetValue(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str()) != NULL;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_value(settings_core::key_path_type key, conainer value) {
			try {
				const settings_core::key_description desc = get_core()->get_registred_key(key.first, key.second);
				std::string comment = "; ";
				if (!desc.title.empty())
					comment += desc.title + " - ";
				if (!desc.description.empty())
					comment += desc.description;
				strEx::replace(comment, "\n", " ");
				
				ini.Delete(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str());
				ini.SetValue(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(), utf8::cvt<std::wstring>(value.get_string()).c_str(), utf8::cvt<std::wstring>(comment).c_str());
			} catch (KeyNotFoundException e) {
				ini.SetValue(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(), utf8::cvt<std::wstring>(value.get_string()).c_str(), L"; Undocumented key");
			} catch (settings_exception e) {
				nsclient::logging::logger::get_logger()->error("settings",__FILE__, __LINE__, "Failed to write key: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				nsclient::logging::logger::get_logger()->error("settings",__FILE__, __LINE__, "Unknown failure when writing key: " + make_skey(key.first, key.second));
			}
		}

		virtual void set_real_path(std::string path) {
			try {
				const settings_core::path_description desc = get_core()->get_registred_path(path);
				if (!desc.description.empty()) {
					std::string comment = "; " + desc.description;
					ini.SetValue(utf8::cvt<std::wstring>(path).c_str(), NULL, NULL, utf8::cvt<std::wstring>(comment).c_str());
				}
			} catch (KeyNotFoundException e) {
				ini.SetValue(utf8::cvt<std::wstring>(path).c_str(), NULL, NULL, L"; Undocumented section");
			} catch (settings_exception e) {
				nsclient::logging::logger::get_logger()->error("settings",__FILE__, __LINE__, "Failed to write section: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				nsclient::logging::logger::get_logger()->error("settings",__FILE__, __LINE__, "Unknown failure when writing section: " + make_skey(path));
			}
		}

		virtual void remove_real_value(settings_core::key_path_type key) {
			ini.Delete(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(), true);
		}
		virtual void remove_real_path(std::string path) {
			ini.Delete(utf8::cvt<std::wstring>(path).c_str(), NULL, true);
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
		virtual void get_real_sections(std::string path, string_list &list) {
			CSimpleIni::TNamesDepend lst;
			std::string::size_type path_len = path.length();
			ini.GetAllSections(lst);
			if (path.empty()) {
				BOOST_FOREACH(const CSimpleIni::Entry &e, lst) {
					std::string key = utf8::cvt<std::string>(e.pItem);
					if (key.length() > 1) {
						std::string::size_type pos = key.find('/', 1);
						if (pos != std::string::npos)
							key = key.substr(0,pos);
					}
					list.push_back(key);
				}
			} else {
				BOOST_FOREACH(const CSimpleIni::Entry &e, lst) {
					std::string key = utf8::cvt<std::string>(e.pItem);
					if (key.length() > path_len+1 && key.substr(0,path_len) == path) {
						std::string::size_type pos = key.find('/', path_len+1);
						if (pos == std::string::npos)
							key = key.substr(path_len+1);
						else
							key = key.substr(path_len+1, pos-path_len-1);
						list.push_back(key);
					}
				}
			}
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
		virtual void get_real_keys(std::string path, string_list &list) {
			load_data();
			CSimpleIni::TNamesDepend lst;
			ini.GetAllKeys(utf8::cvt<std::wstring>(path).c_str(), lst);
			BOOST_FOREACH(const CSimpleIni::Entry &e, lst) {
				list.push_back(utf8::cvt<std::string>(e.pItem));
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Save the settings store
		///
		/// @author mickem
		virtual void save() {
			SettingsInterfaceImpl::save();
			SI_Error rc = ini.SaveFile(get_file_name().string().c_str());
			if (rc < 0)
				throw_SI_error(rc, "Failed to save file");
		}

		settings::error_list validate() {
			settings::error_list ret;
			CSimpleIni::TNamesDepend sections;
			ini.GetAllSections(sections);
			BOOST_FOREACH(const CSimpleIni::Entry &ePath, sections) {
				std::string path = utf8::cvt<std::string>(ePath.pItem);
				try {
					get_core()->get_registred_path(path);
				} catch (const KeyNotFoundException &) {
					ret.push_back(std::string("Invalid path: ") + path);
				}
				CSimpleIni::TNamesDepend keys;
				ini.GetAllKeys(ePath.pItem, keys);
				BOOST_FOREACH(const CSimpleIni::Entry &eKey, keys) {
					std::string key = utf8::cvt<std::string>(eKey.pItem);
					try {
						get_core()->get_registred_key(path, key);
					} catch (const KeyNotFoundException &) {
						ret.push_back(std::string("Invalid key: ") + settings::key_to_string(path, key));
					}
				}
			}

			return ret;
		}
		virtual void real_clear_cache() {
			is_loaded_ = false;
			load_data();
		}
	private:
		void load_data() {
			if (is_loaded_)
				return;
			if (boost::filesystem::is_directory(get_file_name())) {

				boost::filesystem::directory_iterator it(get_file_name()), eod;

				BOOST_FOREACH(boost::filesystem::path const &p, std::make_pair(it, eod)) {
					add_child_unsafe("ini:///" + p.string());
				}
			}
			if (!file_exists()) {
				is_loaded_ = true;
				return;
			}
			std::string f = utf8::cvt<std::string>(get_file_name().string());
			ini.SetUnicode();
			nsclient::logging::logger::get_logger()->debug("settings",__FILE__, __LINE__, "Loading: " + get_file_name().string());
			SI_Error rc = ini.LoadFile(f.c_str());
			if (rc < 0)
				throw_SI_error(rc, "Failed to load file");

			get_core()->register_path(999, "/includes", "INCLUDED FILES", "Files to be included in the configuration", false, false);
			CSimpleIni::TNamesDepend lst;
			ini.GetAllKeys(L"/includes", lst);
			for (CSimpleIni::TNamesDepend::const_iterator cit = lst.begin(); cit != lst.end(); ++cit) {
				std::string child = utf8::cvt<std::string>(ini.GetValue(L"/includes", (*cit).pItem));
				get_core()->register_key(999, "/includes", utf8::cvt<std::string>((*cit).pItem), settings::settings_core::key_string, 
					"INCLUDED FILE", child, child, false, false);
				add_child_unsafe(child);
			}
			is_loaded_ = true;
		}
		void throw_SI_error(SI_Error err, std::string msg) {
			std::string error_str = "unknown error";
			if (err == SI_NOMEM)
				error_str = "Out of memory";
			if (err == SI_FAIL)
				error_str = "General failure";
			if (err == SI_FILE)
				error_str = "I/O error: " + error::lookup::last_error();
			throw settings_exception(msg + " '" + get_context() + "': " + error_str);
		}
		boost::filesystem::path get_file_name() {
			if (filename_.empty()) {
				filename_ = get_file_from_context();
				if (filename_.size() > 0) {
					if (boost::filesystem::is_regular(filename_)) {
					} else if (boost::filesystem::is_regular(filename_.substr(1))) {
						filename_ = filename_.substr(1);
					} else if (boost::filesystem::is_directory(filename_)) {
					} else if (boost::filesystem::is_directory(filename_.substr(1))) {
						filename_ = filename_.substr(1);
					} else {
						std::string tmp = core_->find_file("${exe-path}/" + filename_, "");
						if (boost::filesystem::exists(tmp)) {
							filename_ = tmp;
						} else {
							tmp = core_->find_file("${exe-path}/" + filename_.substr(1), "");
							if (boost::filesystem::exists(tmp)) {
								filename_ = tmp;
							} else {
								nsclient::logging::logger::get_logger()->info("settings", __FILE__, __LINE__, "Configuration file not found: " + filename_);
							}
						}
					}
				}
			}
			return utf8::cvt<std::string>(filename_);
		}
		bool file_exists() {
			return boost::filesystem::is_regular(get_file_name());
		}
		virtual std::string get_info() {
			return "INI settings: (" + context_ + ", " + get_file_name().string() + ")";
		}
		public:
		static bool context_exists(settings::settings_core *core, std::string key) {
			net::url url = net::parse(key);
			std::string file = url.host + url.path;
			std::string tmp = core->expand_path(file);
			if (tmp.size()>1 && tmp[0] == '/') {
				if (boost::filesystem::is_regular(tmp) || boost::filesystem::is_directory(tmp))
					return true;
				tmp = tmp.substr(1);
			}
			return boost::filesystem::is_regular(tmp) || boost::filesystem::is_directory(tmp);
		}
		void ensure_exists() {
			save();
		}


	};
}
