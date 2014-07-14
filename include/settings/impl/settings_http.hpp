#pragma once

#include <string>
#include <fstream>
#include <map>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#ifdef HAVE_LIBCRYPTOPP
#include <sha.h>
#include <hex.h>
#include <files.h>
#endif

#include <http/client.hpp>
#include <net/net.hpp>
#include <file_helpers.hpp>
#include <common.hpp>

#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>
#include <error.hpp>

namespace settings {
	class settings_http : public settings::settings_interface_impl {
	private:
		std::string url_;
		boost::filesystem::path local_file;
		net::url remote_url;

		inline nsclient::logging::logger_interface* get_logger() const {
			return nsclient::logging::logger::get_logger();
		}

	public:
		settings_http(settings::settings_core *core, std::string context) : settings::settings_interface_impl(core, context) {
			remote_url = net::parse(utf8::cvt<std::string>(context), 80);
			boost::filesystem::path path = core->expand_path(CACHE_FOLDER_KEY);
			if (!boost::filesystem::is_directory(path)) {
				if (boost::filesystem::is_regular_file(path)) 
					throw new settings_exception("Cache path not found: " + path.string());
				boost::filesystem::create_directories(path);
				if (!boost::filesystem::is_directory(path))
					throw new settings_exception("Cache path not found: " + path.string());
			}
			local_file = boost::filesystem::path(path) / "cached.ini";

			initial_load();
		}

		virtual void real_clear_cache() {
		}

		std::string hash_file(const boost::filesystem::path &file) {
			std::string result;
#ifdef HAVE_LIBCRYPTOPP
			CryptoPP::SHA1 hash;
			CryptoPP::FileSource(file.string().c_str(),true, 
				new CryptoPP::HashFilter(hash, new CryptoPP::HexEncoder(
				new CryptoPP::StringSink(result), true)));
#endif
			return result;
		}
		

		boost::filesystem::path resolve_cache_file(const net::url &url) const {
			boost::filesystem::path local_file = get_core()->expand_path(CACHE_FOLDER_KEY);
			boost::filesystem::path remote_file_name = url.path;
			local_file /= remote_file_name.filename();
			return local_file;
		}

		bool cache_remote_file(const net::url &url, const boost::filesystem::path &local_file) {
			boost::filesystem::path tmp_file = local_file.string() + ".tmp";
			std::ofstream os(tmp_file.c_str());
			std::string error;
			if (!http::client::download(url.protocol, url.host, url.path, os, error)) {
				os.close();
				throw new settings_exception("Failed to download " + tmp_file.string() + ": " + error);
			}
			os.close();
			if (!boost::filesystem::is_regular_file(tmp_file)) {
				throw new settings_exception("Failed to find cached settings: " + tmp_file.string());
			}
			if (boost::filesystem::is_regular_file(local_file)) {
				std::string old_hash = hash_file(local_file);
				std::string new_hash = hash_file(tmp_file);
				if (old_hash.empty() || old_hash != new_hash) {
					if (old_hash.empty()) {
						get_logger()->error("settings", __FILE__, __LINE__, "Compiled without cryptopp cannot detech changes");
					}
					get_logger()->debug("settings", __FILE__, __LINE__, "File has changed: " + local_file.string());
					boost::filesystem::rename(tmp_file, local_file);
					return true;
				}
			} else {
				boost::filesystem::rename(tmp_file, local_file);
			}
			return false;
		}

		void fetch_attachments(instance_raw_ptr child) {
			if (!child)
				return;
			string_list keys = child->get_keys("/attachments");
			BOOST_FOREACH(const std::string &k, keys) {
				boost::filesystem::path target = get_core()->expand_path(k);
				op_string str = child->get_string("/attachments", k);
				if (!str)
					continue;
				net::url source = net::parse(*str);
				get_logger()->debug("settings", __FILE__, __LINE__, "Found attachment: " + source.to_string() + " as " + target.string());
				cache_remote_file(source, target);
			}
		}

		void initial_load() {
			boost::filesystem::path local_file = resolve_cache_file(remote_url);
			cache_remote_file(remote_url, local_file);
			fetch_attachments(add_child("ini://" + local_file.string()));
		}

		void reload_data() {
			boost::filesystem::path local_file = resolve_cache_file(remote_url);
			if (cache_remote_file(remote_url, local_file)) {
				clear_cache();
				fetch_attachments(add_child("ini://" + local_file.string()));
				get_core()->set_reload(true);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		/// Create a new settings interface of "this kind"
		///
		/// @param context the context to use
		/// @return the newly created settings interface
		///
		/// @author mickem
		virtual settings_interface_impl* create_new_context(std::string context) {
			return new settings_http(get_core(), context);
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
			get_logger()->error("settings",__FILE__, __LINE__, "Cant save over HTTP: " + make_skey(key.first, key.second));
			throw settings_exception("Cannot save settings over HTTP");
		}

		virtual void set_real_path(std::string path) {
			get_logger()->error("settings",__FILE__, __LINE__, "Cant save over HTTP: " + path);
			throw settings_exception("Cannot save settings over HTTP");
		}
		virtual void remove_real_value(settings_core::key_path_type key) {
			get_logger()->error("settings",__FILE__, __LINE__, "Cant save over HTTP");
			throw settings_exception("Cannot save settings over HTTP");
		}
		virtual void remove_real_path(std::string path) {
			get_logger()->error("settings",__FILE__, __LINE__, "Cant save over HTTP");
			throw settings_exception("Cannot save settings over HTTP");
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
			get_logger()->error("settings",__FILE__, __LINE__, "Cannot save settings over HTTP");
			throw settings_exception("Cannot save settings over HTTP");
		}

		settings::error_list validate() {
			settings::error_list ret;
			return ret;
		}
		void ensure_exists() {
		}

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
