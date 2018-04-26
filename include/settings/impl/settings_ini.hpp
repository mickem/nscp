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

#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>

#include <error/error.hpp>

#include <str/xtos.hpp>
#include <str/utils.hpp>

#include <file_helpers.hpp>

#include <simpleini/simpleini.h>


#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <string>
#include <map>


namespace settings {
	class INISettings : public settings::settings_interface_impl {
	private:
		CSimpleIni ini;
		bool is_loaded_;
		std::string filename_;

	public:
		INISettings(settings::settings_core *core, std::string alias, std::string context) : settings::settings_interface_impl(core, alias, context), ini(false, false, false), is_loaded_(false) {
			load_data();
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
			load_data();
			const wchar_t *val = ini.GetValue(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(), NULL);
			if (val == NULL)
				return op_string();
			return op_string(utf8::cvt<std::string>(val));
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

		virtual bool has_real_path(std::string path) {
			return ini.GetSectionSize(utf8::cvt<std::wstring>(path).c_str()) > 0;
		}

		std::string render_comment(const settings_core::key_description &desc) {
			std::string comment = "; ";
			if (!desc.title.empty())
				comment += desc.title + " - ";
			if (!desc.description.empty())
				comment += desc.description;
			str::utils::replace(comment, "\n", " ");
			return comment;
		}

		std::string render_comment(const settings_core::path_description &desc) {
			std::string comment = "; ";
			if (!desc.title.empty())
				comment += desc.title + " - ";
			if (!desc.description.empty())
				comment += desc.description;
			str::utils::replace(comment, "\n", " ");
			return comment;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Write a value to the resulting context.
		///
		/// @param key The key to write to
		/// @param value The value to write
		///
		/// @author mickem
		virtual void set_real_value(settings_core::key_path_type key, conainer value) {
			if (!value.is_dirty())
				return;
			try {
				const settings_core::key_description desc = get_core()->get_registred_key(key.first, key.second);
				std::string comment = render_comment(desc);
				ini.Delete(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str());
				ini.SetValue(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(), utf8::cvt<std::wstring>(value.get_string()).c_str(), utf8::cvt<std::wstring>(comment).c_str());
			} catch (settings_exception e) {
				ini.SetValue(utf8::cvt<std::wstring>(key.first).c_str(), utf8::cvt<std::wstring>(key.second).c_str(), utf8::cvt<std::wstring>(value.get_string()).c_str(), L"; Undocumented key");
			} catch (...) {
				get_logger()->error("settings", __FILE__, __LINE__, "Unknown failure when writing key: " + make_skey(key.first, key.second));
			}
		}

		virtual void set_real_path(std::string path) {
			try {
				const settings_core::path_description desc = get_core()->get_registred_path(path);
				std::string comment = render_comment(desc);
				if (!comment.empty()) {
					ini.SetValue(utf8::cvt<std::wstring>(path).c_str(), NULL, NULL, utf8::cvt<std::wstring>(comment).c_str());
				}
			} catch (settings_exception e) {
				ini.SetValue(utf8::cvt<std::wstring>(path).c_str(), NULL, NULL, L"; Undocumented section");
			} catch (...) {
				get_logger()->error("settings", __FILE__, __LINE__, "Unknown failure when writing section: " + path);
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
							key = key.substr(0, pos);
					}
					list.push_back(key);
				}
			} else {
				BOOST_FOREACH(const CSimpleIni::Entry &e, lst) {
					std::string key = utf8::cvt<std::string>(e.pItem);
					if (key.length() > path_len + 1 && key.substr(0, path_len) == path) {
						std::string::size_type pos = key.find('/', path_len + 1);
						if (pos == std::string::npos && path_len > 1) {
							if (key[path_len] == '/' || key[path_len] == '\\') {
								key = key.substr(path_len + 1);
							} else {
								key = key.substr(path_len);
							}
						} else if (pos == std::string::npos)
							key = key.substr(path_len);
						else if (path_len > 1) {
							if (key[path_len] == '/' || key[path_len] == '\\') {
								key = key.substr(path_len + 1, pos - path_len - 1);
							} else {
								key = key.substr(path_len, pos - path_len);
							}
						} else
							key = key.substr(path_len, pos - path_len);
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
			settings_interface_impl::save();


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
				} catch (const settings_exception &) {
					ret.push_back(std::string("Invalid path: ") + path);
				}
				CSimpleIni::TNamesDepend keys;
				ini.GetAllKeys(ePath.pItem, keys);
				BOOST_FOREACH(const CSimpleIni::Entry &eKey, keys) {
					std::string key = utf8::cvt<std::string>(eKey.pItem);
					try {
						get_core()->get_registred_key(path, key);
					} catch (const settings_exception &) {
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
					add_child_unsafe(file_helpers::meta::get_filename(p), "ini:///" + p.string());
				}
			}
			if (!file_exists()) {
				is_loaded_ = true;
				return;
			}
			std::string f = utf8::cvt<std::string>(get_file_name().string());
			ini.SetUnicode();
			get_logger()->debug("settings", __FILE__, __LINE__, "Loading: " + get_file_name().string());
			SI_Error rc = ini.LoadFile(f.c_str());
			if (rc < 0)
				throw_SI_error(rc, "Failed to load file");

			get_core()->register_path(999, "/includes", "INCLUDED FILES", "Files to be included in the configuration", false, false);
			CSimpleIni::TNamesDepend lst;
			ini.GetAllKeys(L"/includes", lst);
			for (CSimpleIni::TNamesDepend::const_iterator cit = lst.begin(); cit != lst.end(); ++cit) {
				std::string alias = utf8::cvt<std::string>((*cit).pItem);
				std::string child = utf8::cvt<std::string>(ini.GetValue(L"/includes", (*cit).pItem));
				get_core()->register_key(999, "/includes", utf8::cvt<std::string>((*cit).pItem), "INCLUDED FILE", "Included configuration", "", true, false);
				if (!child.empty())
					add_child_unsafe(alias, child);
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
			throw settings_exception(__FILE__, __LINE__, msg + " '" + get_context() + "': " + error_str);
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
								get_logger()->debug("settings", __FILE__, __LINE__, "Configuration file not found: " + filename_);
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
			if (tmp.size() > 1 && tmp[0] == '/') {
				if (boost::filesystem::is_regular(tmp) || boost::filesystem::is_directory(tmp))
					return true;
				tmp = tmp.substr(1);
			}
			return boost::filesystem::is_regular(tmp) || boost::filesystem::is_directory(tmp);
		}
		void ensure_exists() {
			save();
		}
		virtual std::string get_type() { return "ini"; }
	};
}
