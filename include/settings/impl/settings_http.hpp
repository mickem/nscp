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
#include <fstream>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#ifdef HAVE_LIBCRYPTOPP
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-pedantic"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include <sha.h>
#include <hex.h>
#include <files.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#endif

#ifdef HAVE_MINIZ
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <miniz.c>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#endif

#include <socket/client.hpp>
#include <socket/clients/http/http_client_protocol.hpp>

#include <http/client.hpp>
#include <net/net.hpp>
#include <file_helpers.hpp>

#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>

#include <config.h>

namespace settings {
	class settings_http : public settings::settings_interface_impl {
	private:
		std::string url_;
		boost::filesystem::path local_file;
		net::url remote_url;


	public:
		settings_http(settings::settings_core *core, std::string alias, std::string context) : settings::settings_interface_impl(core, alias, context) {
			remote_url = net::parse(utf8::cvt<std::string>(context));
			boost::filesystem::path path = core->expand_path(CACHE_FOLDER);
			if (!boost::filesystem::is_directory(path)) {
				if (boost::filesystem::is_regular_file(path))
					throw new settings_exception(__FILE__, __LINE__, "Cache path not found: " + path.string());
				boost::filesystem::create_directories(path);
				if (!boost::filesystem::is_directory(path))
					throw new settings_exception(__FILE__, __LINE__, "Cache path not found: " + path.string());
			}
			local_file = boost::filesystem::path(path) / "cached.ini";

			initial_load();
		}

		virtual void real_clear_cache() {}

		std::string hash_file(const boost::filesystem::path &file) {
			std::string result;
#ifdef HAVE_LIBCRYPTOPP
			CryptoPP::SHA1 hash;
			CryptoPP::FileSource(file.string().c_str(), true,
				new CryptoPP::HashFilter(hash, new CryptoPP::HexEncoder(
					new CryptoPP::StringSink(result), true)));
#endif
			return result;
		}

		boost::filesystem::path resolve_cache_file(const net::url &url) const {
			boost::filesystem::path local_file = get_core()->expand_path(CACHE_FOLDER);
			boost::filesystem::path remote_file_name = url.path;
			local_file /= remote_file_name.filename();
			return local_file;
		}


		virtual void log_debug(std::string file, int line, std::string msg) const {}

		virtual void log_error(std::string file, int line, std::string msg) const {}
			virtual std::string expand_path(std::string path) {
				return path;
			}

		bool cache_remote_file(const net::url &url, const std::string &file) {
			bool unzip = false;
			boost::filesystem::path tmp_file = file + ".tmp";
			boost::filesystem::path local_file = file;
			if (file.size() > 6 && file.substr(0, 6) == "unzip:") {
				unzip = true;
				local_file = file.substr(6);
				tmp_file = resolve_cache_file(url);
			}

			std::ofstream os(tmp_file.string().c_str(), std::ofstream::binary);
			std::string error;

			try {
				http::packet packet("GET", url.get_host(), url.path);

				std::string def_port = url.protocol == "https"?"443":"80";

				if (!http::simple_client::download(url.protocol, url.host, url.get_port_string(def_port), url.path, os, error)) {
					os.close();
					get_logger()->error("settings", __FILE__, __LINE__, "Failed to download " + tmp_file.string() + ": " + error);
					if (boost::filesystem::is_regular_file(local_file)) {
						get_logger()->error("settings", __FILE__, __LINE__, "Using cached artifact: " + tmp_file.string());
						return true;
					}
					return false;
				}
				os.close();

				if (!boost::filesystem::is_regular_file(tmp_file)) {
					get_logger()->error("settings", __FILE__, __LINE__, "Failed to find cached settings: " + tmp_file.string());
					return false;
				}


			} catch (const socket_helpers::socket_exception &e) {
				get_logger()->error("settings", __FILE__, __LINE__, "Failed to update settings file: " + e.reason());
				return false;
			}




			if (unzip) {
#ifdef HAVE_MINIZ

				mz_zip_archive zip_archive;
				mz_bool status;

				// Now try to open the archive.
				memset(&zip_archive, 0, sizeof(zip_archive));

				status = mz_zip_reader_init_file(&zip_archive, tmp_file.string().c_str(), 0);
				if (!status) {
					printf("mz_zip_reader_init_file() failed!\n");
					return false;
				}

				// Get and print information about each file in the archive.
				for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++) {
					mz_zip_archive_file_stat file_stat;
					if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
						printf("mz_zip_reader_file_stat() failed!\n");
						mz_zip_reader_end(&zip_archive);
						return false;
					}

					boost::filesystem::path tr = local_file / file_stat.m_filename;

					if (!boost::filesystem::exists(tr)) {

						if (!boost::filesystem::exists(tr.parent_path())) {
							boost::filesystem::create_directories(tr.parent_path());
						}
						get_logger()->error("settings", __FILE__, __LINE__, "Unzip to:: " + tr.string());
						if (!mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
							mz_zip_reader_extract_to_file(&zip_archive, i, tr.string().c_str(), 0);
						}
					}
				}
#endif
			} else {
				if (boost::filesystem::is_regular_file(local_file)) {
					std::string old_hash = hash_file(local_file);
					std::string new_hash = hash_file(tmp_file);
					if (old_hash.empty() || old_hash != new_hash) {
						if (old_hash.empty()) {
							get_logger()->error("settings", __FILE__, __LINE__, "Compiled without cryptopp cannot detect changes (assuming always changed)");
						}
						get_logger()->debug("settings", __FILE__, __LINE__, "File has changed: " + local_file.string());
						boost::filesystem::rename(tmp_file, local_file);
						return true;
					}
				} else {
					if (!boost::filesystem::exists(local_file.parent_path())) {
						boost::filesystem::create_directories(local_file.parent_path());
					}
					boost::filesystem::rename(tmp_file, local_file);
				}
			}
			return false;
		}

		void fetch_attachments(instance_raw_ptr child) {
			if (!child)
				return;
			string_list keys = child->get_keys("/attachments");
			BOOST_FOREACH(const std::string &k, keys) {
				std::string target = get_core()->expand_path(k);
				op_string str = child->get_string("/attachments", k);
				if (!str)
					continue;
				net::url source = net::parse(*str);
				get_logger()->debug("settings", __FILE__, __LINE__, "Found attachment: " + source.to_string() + " as " + target);
				cache_remote_file(source, target);
			}
		}

		void initial_load() {
			boost::filesystem::path local_file = resolve_cache_file(remote_url);
			cache_remote_file(remote_url, local_file.string());
			fetch_attachments(add_child("remote_http_file", "ini://" + local_file.string()));
		}

		void reload_data() {
			boost::filesystem::path local_file = resolve_cache_file(remote_url);
			if (cache_remote_file(remote_url, local_file.string())) {
				clear_cache();
				fetch_attachments(add_child("remote_http_file", "ini://" + local_file.string()));
				get_core()->set_reload(true);
			}
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
		virtual bool has_real_path(std::string path) {
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
			get_logger()->error("settings", __FILE__, __LINE__, "Cant save over HTTP: " + make_skey(key.first, key.second));
			throw settings_exception(__FILE__, __LINE__, "Cannot save settings over HTTP");
		}

		virtual void set_real_path(std::string path) {
			get_logger()->error("settings", __FILE__, __LINE__, "Cant save over HTTP: " + path);
			throw settings_exception(__FILE__, __LINE__, "Cannot save settings over HTTP");
		}
		virtual void remove_real_value(settings_core::key_path_type key) {
			get_logger()->error("settings", __FILE__, __LINE__, "Cant save over HTTP");
			throw settings_exception(__FILE__, __LINE__, "Cannot save settings over HTTP");
		}
		virtual void remove_real_path(std::string path) {
			get_logger()->error("settings", __FILE__, __LINE__, "Cant save over HTTP");
			throw settings_exception(__FILE__, __LINE__, "Cannot save settings over HTTP");
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
		virtual void get_real_sections(std::string, string_list &) {}
		//////////////////////////////////////////////////////////////////////////
		/// Get all keys given a path/section.
		/// If the path is empty all root sections will be returned
		///
		/// @param path The path to get sections from (if empty root sections will be returned)
		/// @param list The list to append nodes to
		/// @return a list of sections
		///
		/// @author mickem
		virtual void get_real_keys(std::string, string_list &) {}
		//////////////////////////////////////////////////////////////////////////
		/// Save the settings store
		///
		/// @author mickem
		virtual void save() {
			get_logger()->error("settings", __FILE__, __LINE__, "Cannot save settings over HTTP");
			throw settings_exception(__FILE__, __LINE__, "Cannot save settings over HTTP");
		}

		settings::error_list validate() {
			settings::error_list ret;
			return ret;
		}
		void ensure_exists() {}

		virtual std::string get_type() { return "http"; }

		virtual void house_keeping() {
			reload_data();
		}

	private:

		std::string get_file_name() {
			if (url_.empty()) {
				url_ = get_file_from_context();
			}
			return url_;
		}
		bool file_exists() {
			return boost::filesystem::is_regular(get_file_name());
		}
		virtual std::string get_info() {
			return "HTTP settings: (" + context_ + ", " + get_file_name() + ")";
		}
	};
}