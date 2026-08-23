// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

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

#include <config.h>

#include <file_helpers.hpp>
#include <net/http/client.hpp>
#include <net/http/http_client_protocol.hpp>
#include <net/http/proxy_config.hpp>
#include <net/net.hpp>
#include <net/socket/client.hpp>
#include <net/socket/socket_helpers.hpp>
#include <settings/settings_core.hpp>
#include <settings/settings_interface_impl.hpp>

namespace settings {
class settings_http : public settings::settings_interface_impl {
 private:
  std::string url_;
  boost::filesystem::path local_file_;
  net::url remote_url;
  instance_raw_ptr child_instance;

 public:
  // Settings urls take the same host name placeholders as the submit clients
  // (NRDP, Graphite, Syslog, ...): ${hostname}, ${host}, ${domain} and their
  // _lc/_uc variants. That is what makes one boot.ini deployable to a whole
  // fleet - every agent asks the same script for its own configuration:
  //
  //   [settings]
  //   1 = http://cfgsrv/nsclient.php?host=${hostname}
  //
  // Expanded before parsing, so a placeholder may sit anywhere in the url
  // (host, path or query), and before the query is percent-encoded, so a host
  // name that needs escaping gets escaped rather than corrupting the request.
  static net::url parse_settings_url(const std::string &url) { return net::parse(socket_helpers::expand_hostname(url)); }

  // Local path an attachment is written to, from the key it is declared under.
  // Both kinds of placeholder are resolved, host name first and path tokens
  // afterwards, so one fleet-wide configuration can give every agent its own
  // file (issue #458):
  //
  //   [/attachments]
  //   ${shared-path}/${host}-nsclient.ini = https://cfgsrv/hosts/${host}.ini
  //
  // The url on the right goes through parse_settings_url and has taken host
  // name placeholders since 0.16.1; without this the path on the left did not,
  // and an unknown token silently expands to the installation directory rather
  // than failing, so the attachment landed in one shared file with a mangled
  // name instead of a per-host one.
  //
  // The substituted values are sanitized (the _in_path variant): the target is
  // written to with the service's privileges, and the host name - which DHCP
  // can set on some systems - must not be able to smuggle a separator or a
  // ".." into it.
  static std::string resolve_attachment_target(settings_core *core, const std::string &key) {
    return core->expand_path(socket_helpers::expand_hostname_placeholders_in_path(key));
  }

  settings_http(settings::settings_core *core, std::string alias, std::string context) : settings::settings_interface_impl(core, alias, context) {
    remote_url = parse_settings_url(utf8::cvt<std::string>(context));
    boost::filesystem::path path = core->expand_path(CACHE_FOLDER);
    if (!boost::filesystem::is_directory(path)) {
      if (boost::filesystem::is_regular_file(path)) throw new settings_exception(__FILE__, __LINE__, "Cache path not found: " + path.string());
      boost::filesystem::create_directories(path);
      if (!boost::filesystem::is_directory(path)) throw new settings_exception(__FILE__, __LINE__, "Cache path not found: " + path.string());
    }
    local_file_ = boost::filesystem::path(path) / "cached.ini";

    initial_load();
  }

  bool supports_updates() override { return false; }

  virtual void real_clear_cache() {}

  static std::string hash_file(const boost::filesystem::path &file) { return hash_string(file_helpers::read_file_as_string(file)); }

  static std::string hash_string(const std::string &input) {
#ifdef USE_SSL
    unsigned char hash[SHA256_DIGEST_LENGTH];
    EVP_MD_CTX *context = EVP_MD_CTX_new();

    if (context == nullptr) {
      throw std::runtime_error("Failed to create EVP_MD_CTX");
    }

    if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1 || EVP_DigestUpdate(context, input.c_str(), input.size()) != 1 ||
        EVP_DigestFinal_ex(context, hash, nullptr) != 1) {
      EVP_MD_CTX_free(context);
      throw std::runtime_error("Failed to compute SHA-256 digest");
    }

    EVP_MD_CTX_free(context);

    std::ostringstream oss;
    for (unsigned char c : hash) {
      oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return oss.str();
#else
    // No OpenSSL: fall back to a non-cryptographic hash. hash_string is only
    // used to derive cache file names from the downloaded content, so a
    // collision-resistant digest is not required here.
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << std::hash<std::string>{}(input);
    return oss.str();
#endif
  }

  boost::filesystem::path resolve_cache_file(const net::url &url) const {
    boost::filesystem::path local_file = get_core()->expand_path(CACHE_FOLDER);
    boost::filesystem::path remote_file_name = url.path;
    std::string name = remote_file_name.filename().string();
    // A url can perfectly well carry no file name at all ("http://host/" or
    // "http://host/?file=x"), in which case filename() yields "", "/", "." or
    // ".." and the cache path would collapse onto the cache folder itself.
    if (name.empty() || name == "." || name == ".." || name == "/" || name == "\\") name = "cached.ini";
    if (!url.query.empty()) {
      // Two boot.ini entries may point at the same script and differ only in
      // their parameters (issue #460) - the query is then the only thing that
      // tells the two configurations apart, so it has to take part in the
      // cache file name or they overwrite each other. It cannot be appended
      // verbatim ('?' and '&' are not legal in a Windows file name), so use a
      // short digest of it instead.
      name += "-" + hash_string(url.query).substr(0, 16);
    }
    local_file /= name;
    return local_file;
  }

  // The name resolve_cache_file gave this url before the query digest was
  // added (issue #460), i.e. the url's file name alone. Empty when the url has
  // no usable file name, since no such cache file was ever written. Only used
  // to move an existing cache file forward, never to read from.
  boost::filesystem::path resolve_legacy_cache_file(const net::url &url) const {
    const std::string name = boost::filesystem::path(url.path).filename().string();
    if (name.empty() || name == "." || name == ".." || name == "/" || name == "\\") return boost::filesystem::path();
    return boost::filesystem::path(get_core()->expand_path(CACHE_FOLDER)) / name;
  }

  // Adding the query digest renames the cache file of every url this fix is
  // aimed at. That rename must not lose the cached copy: cache_remote_file
  // falls back to it when the settings server cannot be reached, so an agent
  // that upgrades while its server is down would otherwise find nothing under
  // the new name and boot with an empty configuration - having booted fine off
  // the cache the day before. Move the old file into place once, so the
  // fallback keeps working across the upgrade.
  void migrate_legacy_cache_file(const net::url &url, const boost::filesystem::path &local_file) const {
    if (url.query.empty()) return;  // Name is unchanged for query-less urls.
    boost::system::error_code ec;
    if (boost::filesystem::exists(local_file, ec)) return;
    const boost::filesystem::path legacy = resolve_legacy_cache_file(url);
    if (legacy.empty() || legacy == local_file) return;
    if (!boost::filesystem::is_regular_file(legacy, ec)) return;
    boost::filesystem::rename(legacy, local_file, ec);
    if (ec) {
      // Not fatal: without the cached copy we simply download afresh, which is
      // what happens on any first boot.
      get_logger()->warning("settings", __FILE__, __LINE__,
                            "Failed to move cached settings from '" + legacy.string() + "' to '" + local_file.string() + "': " + ec.message());
      return;
    }
    get_logger()->debug("settings", __FILE__, __LINE__, "Migrated cached settings from '" + legacy.string() + "' to '" + local_file.string() + "'");
  }

  virtual void log_debug(std::string file, int line, std::string msg) const { core_->get_logger()->debug("settings", file.c_str(), line, msg); }

  virtual void log_error(std::string file, int line, std::string msg) const { core_->get_logger()->error("settings", file.c_str(), line, msg); }
  virtual std::string expand_path(std::string path) { return path; }

  bool cache_remote_file(const net::url &url, const std::string &file) {
    bool unzip = false;
    boost::filesystem::path tmp_file = file + ".tmp";
    boost::filesystem::path local_file = file;
    if (file.size() > 6 && file.substr(0, 6) == "unzip:") {
      unzip = true;
      local_file = file.substr(6);
      tmp_file = resolve_cache_file(url);
    }

    // RAII-ish guard so we never leak the tmp file on any code path (issue #370).
    struct tmp_guard {
      boost::filesystem::path path;
      bool active = true;
      ~tmp_guard() {
        if (!active) return;
        boost::system::error_code ec;
        if (boost::filesystem::exists(path, ec)) boost::filesystem::remove(path, ec);
      }
    } guard{tmp_file};

    std::ofstream os(tmp_file.string().c_str(), std::ofstream::binary);

    try {
      std::string error;
      std::string def_port = url.protocol == "https" ? "443" : "80";

      auto tls_version = get_core()->get_tls_version();
      auto verify_mode = get_core()->get_tls_verify_mode();
      // The CA may be written as a path macro (it defaults to ${ca-path}), so
      // it has to be expanded before OpenSSL sees it.
      auto ca = get_core()->expand_path(get_core()->get_tls_ca());

      // This download becomes the agent's configuration. An unverified fetch
      // hands whoever answers for `url.host` full control of the host, so it
      // must never be the quiet path: complain on every attempt, and say what
      // to set. Proceeding rather than refusing is deliberate - `none` can now
      // only come from an operator explicitly writing it into boot.ini, and
      // silently bricking such an install on upgrade would be worse than a
      // loud log line.
      //
      // Both of these are advisories about a fetch that is still going to be
      // attempted, so they are logged as warnings rather than errors. Error
      // level means "this operation failed" to whoever is listening: the MSI
      // custom action reads back the existing configuration through this very
      // code path and treats any error-level message as a failed settings
      // read, discarding the CONFIGURATION_TYPE the operator asked for and
      // falling back to a local ini - i.e. an advisory logged as an error
      // silently broke every https:// install. The real failure, when there is
      // one, is still logged as an error by the download below.
      if (url.protocol == "https") {
        if (verify_mode.empty() || verify_mode == "none") {
          // Both spellings disable verification (an empty mode parses to
          // verify_none), but say which one is actually in the file - telling
          // an operator their boot.ini reads "verify mode = none" when it
          // reads "verify mode =" sends them looking for the wrong line.
          const std::string how = verify_mode.empty() ? "[tls] verify mode is set but empty in boot.ini, which disables verification just as 'none' does"
                                                      : "[tls] verify mode = none in boot.ini";
          get_logger()->warning("settings", __FILE__, __LINE__,
                                "INSECURE: fetching settings from " + url.to_log_safe_string() + " without verifying the server certificate (" + how +
                                    "). Anyone who can answer for this host controls this agent's entire configuration, including external script "
                                    "definitions. Set 'verify mode = peer' and point 'ca' at the issuing CA.");
        } else if (!ca.empty() && ca != "none" && !boost::filesystem::is_regular_file(ca)) {
          get_logger()->warning("settings", __FILE__, __LINE__,
                                "CA bundle '" + ca + "' not found; the settings download from " + url.to_log_safe_string() +
                                    " will fail certificate verification. Point [tls] ca in boot.ini at the CA that issued the settings server's "
                                    "certificate. (On Windows the default bundle is exported at startup, so it is absent during the very first boot.)");
        }
      }

      http::proxy_config proxy = http::parse_proxy_url(get_core()->get_proxy_url());
      const std::string no_proxy_str = get_core()->get_no_proxy();
      if (!no_proxy_str.empty()) {
        std::istringstream ss(no_proxy_str);
        std::string token;
        while (std::getline(ss, token, ',')) {
          if (!token.empty()) proxy.no_proxy.push_back(token);
        }
      }

      get_logger()->debug("settings", __FILE__, __LINE__, "Using TLS settings version: " + tls_version + ", verify: " + verify_mode + ", ca: " + ca);
      if (proxy.is_set()) {
        get_logger()->debug("settings", __FILE__, __LINE__, "Using proxy: " + get_core()->get_proxy_url());
      }
      // get_request_path(), not path: the query string is part of the resource
      // being asked for, and a settings url that selects its configuration with
      // parameters is useless without it (issue #460).
      if (!http::simple_client::download(url.protocol, url.host, url.get_port_string(def_port), url.get_request_path(), tls_version, verify_mode, ca, os,
                                         error, proxy)) {
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

        // Zip-slip guard: a malicious or compromised settings host can put
        // entries like "../../Windows/System32/foo" or "/etc/cron.d/foo"
        // into the archive. is_safe_archive_entry rejects empty / NUL-bearing
        // names and any join that escapes local_file after lexical
        // normalisation. A single bad entry fails the whole archive - a
        // hostile source should not get partial application.
        const std::string raw_name = file_stat.m_filename != nullptr ? std::string(file_stat.m_filename) : std::string();
        boost::filesystem::path tr;
        if (!file_helpers::checks::is_safe_archive_entry(local_file, raw_name, tr)) {
          get_logger()->error("settings", __FILE__, __LINE__,
                              "Refusing zip entry that would extract outside '" + local_file.string() + "': '" + raw_name + "'");
          mz_zip_reader_end(&zip_archive);
          return false;
        }

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
      mz_zip_reader_end(&zip_archive);
#endif
      // tmp_file is the downloaded zip; the guard above removes it (issue #370).
    } else {
      if (boost::filesystem::is_regular_file(local_file)) {
        std::string old_hash = hash_file(local_file);
        std::string new_hash = hash_file(tmp_file);
        if (old_hash.empty() || old_hash != new_hash) {
          if (old_hash.empty()) {
            get_logger()->error("settings", __FILE__, __LINE__, "Compiled without cryptopp cannot detect changes (assuming always changed)");
          }
          get_logger()->debug("settings", __FILE__, __LINE__, "File has changed: " + local_file.string());
          // Use remove+rename so this also works when local_file already
          // exists on Windows where rename is non-overwriting.
          boost::system::error_code ec;
          boost::filesystem::remove(local_file, ec);
          boost::filesystem::rename(tmp_file, local_file);
          guard.active = false;  // tmp_file has been moved into place
          return true;
        }
        // Hashes match: the cached file is up to date and we just leave
        // local_file alone. The guard removes the now-redundant tmp_file
        // (issue #370).
      } else {
        if (!boost::filesystem::exists(local_file.parent_path())) {
          boost::filesystem::create_directories(local_file.parent_path());
        }
        boost::filesystem::rename(tmp_file, local_file);
        guard.active = false;  // tmp_file has been moved into place
      }
    }
    return false;
  }

  void fetch_attachments(instance_raw_ptr child) {
    if (!child) return;
    string_list keys = child->get_keys("/attachments");
    for (const std::string &k : keys) {
      std::string target = resolve_attachment_target(get_core(), k);
      op_string str = child->get_string("/attachments", k);
      if (!str) continue;
      net::url source = parse_settings_url(*str);
      get_logger()->debug("settings", __FILE__, __LINE__, "Found attachment: " + source.to_log_safe_string() + " as " + target);
      cache_remote_file(source, target);
    }
  }

  void initial_load() {
    boost::filesystem::path local_file = resolve_cache_file(remote_url);
    migrate_legacy_cache_file(remote_url, local_file);
    cache_remote_file(remote_url, local_file.string());
    child_instance = add_child("remote_http_file", "ini://" + local_file.string());
    fetch_attachments(child_instance);
  }

  void reload_data() {
    boost::filesystem::path local_file = resolve_cache_file(remote_url);
    migrate_legacy_cache_file(remote_url, local_file);
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
  virtual op_string get_real_string(settings_core::key_path_type key) { return op_string(); }
  //////////////////////////////////////////////////////////////////////////
  /// Get an integer value if it does not exist exception will be thrown
  ///
  /// @param path the path to look up
  /// @param key the key to lookup
  /// @return the int value
  virtual op_int get_real_int(settings_core::key_path_type key) { return op_int(); }
  //////////////////////////////////////////////////////////////////////////
  /// Get a boolean value if it does not exist exception will be thrown
  ///
  /// @param path the path to look up
  /// @param key the key to lookup
  /// @return the boolean value
  virtual op_bool get_real_bool(settings_core::key_path_type key) { return op_bool(); }
  //////////////////////////////////////////////////////////////////////////
  /// Check if a key exists
  ///
  /// @param path the path to look up
  /// @param key the key to lookup
  /// @return true/false if the key exists.
  virtual bool has_real_key(settings_core::key_path_type key) { return false; }
  virtual bool has_real_path(std::string path) { return false; }
  //////////////////////////////////////////////////////////////////////////
  /// Write a value to the resulting context.
  ///
  /// @param key The key to write to
  /// @param value The value to write
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
  virtual void get_real_sections(std::string section, string_list &list) {
    if (child_instance) {
      auto child_list = child_instance->get_sections(section);
      list.insert(list.end(), child_list.begin(), child_list.end());
    }
  }
  //////////////////////////////////////////////////////////////////////////
  /// Get all keys given a path/section.
  /// If the path is empty all root sections will be returned
  ///
  /// @param path The path to get sections from (if empty root sections will be returned)
  /// @param list The list to append nodes to
  /// @return a list of sections
  virtual void get_real_keys(std::string path, string_list &list) {
    if (child_instance) {
      auto child_list = child_instance->get_keys(path);
      list.insert(list.end(), child_list.begin(), child_list.end());
    }
  }
  //////////////////////////////////////////////////////////////////////////
  /// Save the settings store
  virtual void save(bool _re_save_all) {
    get_logger()->error("settings", __FILE__, __LINE__, "Cannot save settings over HTTP");
    throw settings_exception(__FILE__, __LINE__, "Cannot save settings over HTTP");
  }

  settings::error_list validate() {
    settings::error_list ret;
    return ret;
  }
  void ensure_exists() {}

  virtual std::string get_type() { return "http"; }

  virtual void house_keeping() { reload_data(); }

  std::string get_file_name() {
    if (url_.empty()) {
      url_ = get_file_from_context();
    }
    return url_;
  }
  bool file_exists() { return boost::filesystem::is_regular_file(get_file_name()); }
  // get_info() is printed by `nscp settings --show` and friends, so it goes the
  // same way as the log: the context identifies the store, the query does not
  // need to be part of that and may carry a credential.
  virtual std::string get_info() { return "HTTP settings: (" + net::parse(context_).to_log_safe_string() + ", " + get_file_name() + ")"; }
  void enable_credentials() override { get_logger()->warning("settings", __FILE__, __LINE__, "Http settings is read only and does not support credentials"); }
};
}  // namespace settings