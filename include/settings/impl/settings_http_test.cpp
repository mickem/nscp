// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio.hpp>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <settings/impl/settings_http.hpp>
#include <settings/impl/settings_ini.hpp>
#include <settings/test_helpers.hpp>
#include <str/utils.hpp>
#include <thread>
#include <vector>

using settings_test::mock_settings_core;
using settings_test::temp_dir;

namespace {

// A settings_core that overrides expand_path so the CACHE_FOLDER token
// (defined as "${cache-folder}" in config.h) resolves to a real, writable
// temp directory.  Mirrors what NSCSettingsImpl does in production.
class http_test_core : public mock_settings_core {
 public:
  // These tests drive a plain-HTTP loopback server because standing up TLS for
  // every case would test asio, not settings_http. Production refuses a
  // non-https settings source unless boot.ini opts in, so opt in by default
  // here; the plaintext_* tests below pass false to pin the refusal.
  explicit http_test_core(boost::filesystem::path cache, bool allow_plaintext = true) : cache_(std::move(cache)) {
    set_allow_plaintext(allow_plaintext);
  }

  std::string expand_path(std::string key) override {
    if (key == CACHE_FOLDER) return cache_.string();
    return key;
  }

 private:
  boost::filesystem::path cache_;
};

// Records what was logged, and at which level.
//
// The level matters beyond tidiness: the MSI's ImportConfig custom action
// boots the existing configuration through settings_http and treats any
// error-level line as "this host's configuration could not be read", which
// makes it discard the operator's CONFIGURATION_TYPE. An advisory about a
// fetch that was skipped - the agent carries on with its cached copy - must
// therefore not be logged as an error, or every upgrade of a host with a
// plain-http settings url fails configuration import.
class recording_logger : public settings_test::null_logger {
 public:
  void warning(const std::string &, const char *, const int, const std::string &message) override { warnings_.push_back(message); }
  void error(const std::string &, const char *, const int, const std::string &message) override { errors_.push_back(message); }

  bool should_warning() const override { return true; }
  bool should_error() const override { return true; }

  const std::vector<std::string> &warnings() const { return warnings_; }
  const std::vector<std::string> &errors() const { return errors_; }

  static bool any_contains(const std::vector<std::string> &haystack, const std::string &needle) {
    for (const std::string &entry : haystack) {
      if (entry.find(needle) != std::string::npos) return true;
    }
    return false;
  }

 private:
  std::vector<std::string> warnings_;
  std::vector<std::string> errors_;
};

// http_test_core with a logger the test can read back.
class recording_http_core : public http_test_core {
 public:
  recording_http_core(boost::filesystem::path cache, bool allow_plaintext)
      : http_test_core(std::move(cache), allow_plaintext), recorder_(std::make_shared<recording_logger>()) {}

  nsclient::logging::logger_instance get_logger() const override { return recorder_; }
  const recording_logger &recorded() const { return *recorder_; }

 private:
  std::shared_ptr<recording_logger> recorder_;
};

// One-shot HTTP server: accepts a single connection, replies with the canned
// response, closes.  settings_http performs exactly one HTTP fetch per
// construction (add_child's create_instance returns null in our mock so
// fetch_attachments is a no-op).  Single-accept matches loopback_http_server
// in client_test.cpp and avoids destructor hangs if a test never connects.
class loopback_http {
 public:
  explicit loopback_http(std::string body) : body_(std::move(body)), port_(0) {
    std::promise<unsigned short> p;
    std::future<unsigned short> f = p.get_future();
    thread_ = std::thread([this, prom = std::move(p)]() mutable {
      try {
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, {tcp::v4(), 0});
        prom.set_value(acceptor.local_endpoint().port());
        tcp::socket socket(io);
        acceptor.accept(socket);
        boost::asio::streambuf req;
        boost::system::error_code ec;
        boost::asio::read_until(socket, req, "\r\n\r\n", ec);
        std::istream is(&req);
        std::getline(is, request_line_);
        if (!request_line_.empty() && request_line_.back() == '\r') request_line_.pop_back();
        boost::asio::write(socket, boost::asio::buffer(body_), ec);
      } catch (...) {
      }
    });
    port_ = f.get();
  }

  ~loopback_http() {
    if (thread_.joinable()) thread_.join();
  }

  unsigned short port() const { return port_; }

  // The request line ("GET /path HTTP/1.0") the client actually sent.  Joins
  // the server thread first, so this must be called after whatever drives the
  // fetch has returned.
  std::string request_line() {
    if (thread_.joinable()) thread_.join();
    return request_line_;
  }

 private:
  std::string body_;
  unsigned short port_;
  std::string request_line_;
  std::thread thread_;
};

std::string http_url(unsigned short port, const std::string &path = "/settings.ini") { return "http://127.0.0.1:" + std::to_string(port) + path; }

// A port on 127.0.0.1 that nothing listens on, obtained by binding an
// ephemeral port and closing it again. The tests below need a download to
// *fail*, and the obvious way to spell that - some low fixed port such as 1 -
// is not portable: on WSL2 a connect to an arbitrary unbound fixed port is
// swallowed rather than refused, and the three tests using it each sat in the
// TCP connect timeout for over two minutes. A port the kernel has just handed
// out and taken back is known-free to the local stack, so the connect is
// refused immediately.
unsigned short closed_port() {
  boost::asio::io_context io;
  tcp::acceptor probe(io, {tcp::v4(), 0});
  const unsigned short port = probe.local_endpoint().port();
  probe.close();
  return port;
}

std::string unreachable_url(const std::string &path) { return "http://127.0.0.1:" + std::to_string(closed_port()) + path; }

// A listener that records whether anything ever spoke HTTP to it. Unlike
// loopback_http it does not assume a client turns up: the destructor dials its
// own port so the blocking accept returns and the thread can be joined. That
// is what makes it usable to prove that a fetch never happened.
class loopback_listener {
 public:
  loopback_listener() : port_(0), served_(false) {
    std::promise<unsigned short> p;
    std::future<unsigned short> f = p.get_future();
    thread_ = std::thread([this, prom = std::move(p)]() mutable {
      try {
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, {tcp::v4(), 0});
        prom.set_value(acceptor.local_endpoint().port());
        tcp::socket socket(io);
        acceptor.accept(socket);
        boost::asio::streambuf req;
        boost::system::error_code ec;
        boost::asio::read_until(socket, req, "\r\n\r\n", ec);
        std::istream is(&req);
        std::string line;
        std::getline(is, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // The destructor's own unblocking connection sends nothing, so an
        // empty request line means no client ever asked for anything.
        served_ = !line.empty();
      } catch (...) {
      }
    });
    port_ = f.get();
  }

  ~loopback_listener() { stop(); }

  unsigned short port() const { return port_; }

  bool served() {
    stop();
    return served_;
  }

 private:
  void stop() {
    if (!thread_.joinable()) return;
    try {
      boost::asio::io_context io;
      tcp::socket probe(io);
      boost::system::error_code ec;
      probe.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port_), ec);
    } catch (...) {
    }
    thread_.join();
  }

  unsigned short port_;
  std::atomic<bool> served_;
  std::thread thread_;
};

}  // namespace

TEST(settings_http, type_is_http) {
  // Construction triggers initial_load which performs a download.  A loopback
  // server prevents the test from actually hitting the network.
  const loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port()));
  EXPECT_EQ(s.get_type(), "http");
}

TEST(settings_http, does_not_support_updates) {
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port()));
  EXPECT_FALSE(s.supports_updates());
}

TEST(settings_http, info_contains_context) {
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  const std::string url = http_url(server.port());
  settings::settings_http s(&core, "test", url);
  EXPECT_NE(s.get_info().find(url), std::string::npos);
}

TEST(settings_http, info_omits_query) {
  // get_info() is what `nscp settings --show` prints, and a settings url is
  // free to select its configuration with a parameter that is a credential.
  // Scheme, host and path identify the store; the query must not come along.
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  const std::string base = http_url(server.port());
  settings::settings_http s(&core, "test", base + "?token=s3cret");
  const std::string info = s.get_info();
  EXPECT_NE(info.find(base), std::string::npos);
  EXPECT_EQ(info.find("s3cret"), std::string::npos);
  EXPECT_EQ(info.find("token"), std::string::npos);
}

TEST(settings_http, save_throws) {
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port()));
  EXPECT_THROW(s.save(false), settings::settings_exception);
}

TEST(settings_http, set_real_value_throws) {
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port()));
  EXPECT_THROW(s.set_real_value({"/x", "y"}, settings::settings_interface_impl::conainer(std::string("v"), true)), settings::settings_exception);
}

TEST(settings_http, get_real_string_returns_empty) {
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port()));
  // settings_http always returns an empty op_string from get_real_*; values
  // are read from the cached child INI instance.
  EXPECT_FALSE(s.get_real_string({"/x", "y"}).has_value());
  EXPECT_FALSE(s.get_real_int({"/x", "y"}).has_value());
  EXPECT_FALSE(s.get_real_bool({"/x", "y"}).has_value());
  EXPECT_FALSE(s.has_real_key({"/x", "y"}));
}

TEST(settings_http, validate_returns_no_errors) {
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port()));
  EXPECT_TRUE(s.validate().empty());
}

TEST(settings_http, hash_string_is_stable) {
  // Same input → same digest, different inputs → different digests.  This is
  // a static helper so it doesn't need a settings_core.
  const std::string a = settings::settings_http::hash_string("hello");
  const std::string b = settings::settings_http::hash_string("hello");
  const std::string c = settings::settings_http::hash_string("world");
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_FALSE(a.empty());
}

TEST(settings_http, hash_string_is_hex) {
  const std::string h = settings::settings_http::hash_string("test");
  // SHA-256 → 64 hex chars.
  EXPECT_EQ(h.size(), 64u);
  for (char c : h) {
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) << "non-hex char: " << c;
  }
}

TEST(settings_http, resolve_cache_file_uses_cache_folder_and_url_filename) {
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port(), "/dir/foo.ini"));

  net::url u;
  u.path = "/dir/foo.ini";
  const auto resolved = s.resolve_cache_file(u);
  // Filename component of the URL path is what ends up in the cache directory.
  EXPECT_EQ(resolved.filename().string(), "foo.ini");
  EXPECT_EQ(resolved.parent_path(), cache.path());
}

// --- issue #460: settings urls carrying query parameters --------------------

TEST(settings_http, download_sends_the_query_string) {
  // The whole point of a "?" url in boot.ini is that the parameters select
  // which configuration the server hands back, so they have to survive onto
  // the request line.
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port(), "/nsclient.php?RootFolder=myhost/&Filename=nsclient.ini"));

  const std::string request = server.request_line();
  EXPECT_NE(request.find("/nsclient.php?RootFolder=myhost/&Filename=nsclient.ini"), std::string::npos) << "request line was: " << request;
}

TEST(settings_http, download_without_query_is_unchanged) {
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port(), "/settings.ini"));

  const std::string request = server.request_line();
  EXPECT_NE(request.find("GET /settings.ini "), std::string::npos) << "request line was: " << request;
  EXPECT_EQ(request.find('?'), std::string::npos) << "request line was: " << request;
}

TEST(settings_http, resolve_cache_file_separates_urls_differing_only_in_query) {
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port(), "/nsclient.php?host=a"));

  net::url a;
  a.path = "/nsclient.php";
  a.query = "RootFolder=host-a";
  net::url b;
  b.path = "/nsclient.php";
  b.query = "RootFolder=host-b";
  net::url plain;
  plain.path = "/nsclient.php";

  const auto ra = s.resolve_cache_file(a);
  const auto rb = s.resolve_cache_file(b);
  const auto rp = s.resolve_cache_file(plain);

  // Same script, different parameters: distinct configurations, so they must
  // not share one cache file.
  EXPECT_NE(ra, rb);
  EXPECT_NE(ra, rp);
  EXPECT_EQ(ra, s.resolve_cache_file(a)) << "cache file name must be stable across runs";
  EXPECT_EQ(ra.parent_path(), cache.path());
  // Still recognisable, and still a legal file name on every platform.
  EXPECT_EQ(ra.filename().string().find("nsclient.php"), 0u);
  EXPECT_EQ(ra.filename().string().find_first_of("?&=/\\:*\"<>|"), std::string::npos) << ra.filename().string();
  // A url without a query keeps the plain, historic name.
  EXPECT_EQ(rp.filename().string(), "nsclient.php");
}

TEST(settings_http, expands_hostname_placeholders_in_the_query) {
  // One boot.ini for a whole fleet: the script is told which host is asking.
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port(), "/nsclient.php?host=${hostname}"));

  const std::string expected = net::encode_query("host=" + boost::asio::ip::host_name());
  const std::string request = server.request_line();
  EXPECT_NE(request.find("/nsclient.php?" + expected), std::string::npos) << "request line was: " << request;
  EXPECT_EQ(request.find("${"), std::string::npos) << "request line was: " << request;
}

TEST(settings_http, expands_hostname_placeholders_outside_the_query) {
  // A placeholder is allowed anywhere in the url, not just in the parameters.
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port(), "/hosts/${host}/nsclient.ini"));

  const std::string host = str::utils::getToken(boost::asio::ip::host_name(), '.').first;
  const std::string request = server.request_line();
  EXPECT_NE(request.find("/hosts/" + host + "/nsclient.ini"), std::string::npos) << "request line was: " << request;
}

TEST(settings_http, expanded_query_drives_the_cache_file_name) {
  // The cache name is derived from the expanded url, so it is stable for a
  // given host rather than being one shared "${hostname}" bucket.
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port(), "/nsclient.php?host=${hostname}"));

  net::url expanded;
  expanded.path = "/nsclient.php";
  expanded.query = "host=" + boost::asio::ip::host_name();
  net::url literal;
  literal.path = "/nsclient.php";
  literal.query = "host=${hostname}";
  EXPECT_NE(s.resolve_cache_file(expanded), s.resolve_cache_file(literal));
  EXPECT_TRUE(boost::filesystem::is_regular_file(s.resolve_cache_file(expanded))) << "the fetch should have cached under the expanded name";
}

TEST(settings_http, migrates_the_pre_460_cache_file_to_the_new_name) {
  // Upgrade scenario: the agent already has a cache file under the old
  // query-less name, and the settings server is unreachable on this boot.  The
  // cached copy has to survive the rename, otherwise the agent that booted fine
  // yesterday comes up with no configuration at all.
  temp_dir cache;
  const std::string cached_ini = "[/settings/default]\nallowed hosts=10.0.0.1\n";
  settings_test::write_file(cache.path() / "nsclient.php", cached_ini);

  http_test_core core(cache.path());
  // Nothing listens on that port, so the download fails and only the migrated file
  // can satisfy cache_remote_file's fallback.
  settings::settings_http s(&core, "test", unreachable_url("/nsclient.php?RootFolder=myhost"));

  net::url u;
  u.path = "/nsclient.php";
  u.query = "RootFolder=myhost";
  const auto migrated = s.resolve_cache_file(u);
  ASSERT_TRUE(boost::filesystem::is_regular_file(migrated)) << migrated.string();
  EXPECT_EQ(file_helpers::read_file_as_string(migrated), cached_ini);
  // Moved, not copied - no orphan left behind under the old name.
  EXPECT_FALSE(boost::filesystem::exists(cache.path() / "nsclient.php"));
}

TEST(settings_http, migration_does_not_clobber_an_existing_cache_file) {
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", unreachable_url("/nsclient.php?RootFolder=myhost"));

  net::url u;
  u.path = "/nsclient.php";
  u.query = "RootFolder=myhost";
  const auto current = s.resolve_cache_file(u);
  settings_test::write_file(current, "current\n");
  settings_test::write_file(cache.path() / "nsclient.php", "legacy\n");

  s.migrate_legacy_cache_file(u, current);
  // Already migrated once: the new name wins and the old file is left alone.
  EXPECT_EQ(file_helpers::read_file_as_string(current), "current\n");
  EXPECT_TRUE(boost::filesystem::exists(cache.path() / "nsclient.php"));
}

TEST(settings_http, no_migration_for_a_url_without_a_query) {
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", unreachable_url("/nsclient.php"));

  net::url u;
  u.path = "/nsclient.php";
  // Same name before and after the fix, so there is nothing to move.
  EXPECT_EQ(s.resolve_cache_file(u), s.resolve_legacy_cache_file(u));
}

TEST(settings_http, resolve_cache_file_handles_url_without_a_file_name) {
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path());
  settings::settings_http s(&core, "test", http_url(server.port(), "/settings.ini"));

  net::url root;
  root.path = "/";
  root.query = "Filename=nsclient.ini";
  const auto resolved = s.resolve_cache_file(root);
  // Without this the cache path would collapse onto the cache directory itself.
  EXPECT_NE(resolved, cache.path());
  EXPECT_EQ(resolved.parent_path(), cache.path());
  EXPECT_EQ(resolved.filename().string().find("cached.ini"), 0u);
}

// --- issue #458: host name placeholders in attachment targets ---------------

namespace {
// A core whose expand_path behaves like the real path manager for the one
// token these tests care about, so the assertions show which pass resolved
// which placeholder.
class attachment_core : public mock_settings_core {
 public:
  std::string expand_path(std::string key) override {
    str::utils::replace(key, "${shared-path}", "/etc/nsclient");
    return key;
  }
};
}  // namespace

TEST(settings_http, attachment_target_expands_host_name_placeholders) {
  // The reported case: one configuration served to a whole fleet, each agent
  // writing its own file. Before this the ${host} token reached the path
  // manager, which has no answer for it and hands back the installation
  // directory - so every agent wrote the same mangled name.
  attachment_core core;
  const std::string host = str::utils::getToken(boost::asio::ip::host_name(), '.').first;

  EXPECT_EQ(settings::settings_http::resolve_attachment_target(&core, "${shared-path}/${host}-nsclient.ini"),
            "/etc/nsclient/" + host + "-nsclient.ini");
}

TEST(settings_http, attachment_target_expands_the_full_host_name) {
  attachment_core core;
  EXPECT_EQ(settings::settings_http::resolve_attachment_target(&core, "${shared-path}/${hostname}.ini"),
            "/etc/nsclient/" + boost::asio::ip::host_name() + ".ini");
}

TEST(settings_http, a_relative_attachment_target_lands_under_the_shared_path) {
  // The documented form is a bare relative name, and expanding it left it
  // relative - so the file was written relative to the service's working
  // directory: C:\Windows\System32 for a Windows service, "/" under a bare
  // init script. On Linux the shipped systemd unit happens to set
  // WorkingDirectory to the package directory, which *is* ${shared-path}, so it
  // landed correctly there by accident - and that accident is why nobody
  // noticed. Root it explicitly: same answer on unix, same answer everywhere
  // else now too.
  attachment_core core;
  // generic_string(): the join uses boost's preferred separator, which is a
  // backslash on Windows. The folder it lands in is what matters here.
  EXPECT_EQ(boost::filesystem::path(settings::settings_http::resolve_attachment_target(&core, "scripts/myscript.bat")).generic_string(),
            "/etc/nsclient/scripts/myscript.bat");
}

TEST(settings_http, an_attachment_target_written_with_a_token_is_unchanged) {
  attachment_core core;
  EXPECT_EQ(settings::settings_http::resolve_attachment_target(&core, "${shared-path}/scripts/myscript.bat"), "/etc/nsclient/scripts/myscript.bat");
}

TEST(settings_http, an_unzip_attachment_target_keeps_its_prefix_at_the_front) {
  // cache_remote_file detects the archive form with substr(0, 6), so the prefix
  // has to survive rooting at offset 0. Rooting the whole value would produce
  // "/etc/nsclient/unzip:scripts" - no longer an archive instruction, just a
  // very oddly named file for the download to land in.
  attachment_core core;
  EXPECT_EQ(boost::filesystem::path(settings::settings_http::resolve_attachment_target(&core, "unzip:scripts")).generic_string(),
            "unzip:/etc/nsclient/scripts");
}

TEST(settings_http, an_unzip_attachment_target_roots_the_destination_behind_the_prefix) {
  attachment_core core;
  const std::string target = settings::settings_http::resolve_attachment_target(&core, "unzip:scripts/bundle");
  ASSERT_EQ(target.substr(0, 6), "unzip:");
  EXPECT_EQ(boost::filesystem::path(target.substr(6)).generic_string(), "/etc/nsclient/scripts/bundle");
}

TEST(settings_http, an_unzip_attachment_target_naming_a_root_is_left_alone) {
  attachment_core core;
  EXPECT_EQ(settings::settings_http::resolve_attachment_target(&core, "unzip:/srv/elsewhere"), "unzip:/srv/elsewhere");
}

TEST(settings_http, an_absolute_attachment_target_is_left_where_the_operator_put_it) {
  // Rooting applies only to a value that names no location of its own. Pointing
  // an attachment somewhere specific stays the operator's call.
  attachment_core core;
  EXPECT_EQ(settings::settings_http::resolve_attachment_target(&core, "/srv/elsewhere/myscript.bat"), "/srv/elsewhere/myscript.bat");
}

TEST(settings_http, an_attachment_target_naming_an_unknown_token_is_reported) {
  // The caller skips just this attachment and keeps the configuration it has
  // already loaded; what must not happen is a silent write to the wrong place.
  class throwing_core : public attachment_core {
   public:
    std::string expand_path(std::string key) override {
      if (key.find("${nope}") != std::string::npos) throw nscp::paths::path_expansion_error("Unknown path token ${nope}");
      return attachment_core::expand_path(std::move(key));
    }
  } core;
  EXPECT_THROW(settings::settings_http::resolve_attachment_target(&core, "${nope}/x.ini"), nscp::paths::path_expansion_error);
}

TEST(settings_http, attachment_target_and_source_agree_on_the_host) {
  // Both halves of an attachment line have to name the same host, or the file
  // an agent downloads is not the file it writes.
  attachment_core core;
  const std::string target = settings::settings_http::resolve_attachment_target(&core, "${shared-path}/${host}.ini");
  const net::url source = settings::settings_http::parse_settings_url("https://cfgsrv/hosts/${host}.ini");

  const std::string host = str::utils::getToken(boost::asio::ip::host_name(), '.').first;
  EXPECT_EQ(target, "/etc/nsclient/" + host + ".ini");
  EXPECT_EQ(source.path, "/hosts/" + host + ".ini");
}

// --- plain http:// settings sources -----------------------------------------
//
// The remote store is the agent's entire configuration - [/modules], external
// script definitions, submit-client credentials - re-fetched at boot and on
// every housekeeping pass. Over plain http nothing authenticates the server,
// so anyone on path, or anyone who can answer for the host name via DHCP or
// DNS, owns every agent pointed at it. The fetch has to be refused outright,
// not merely warned about and then performed anyway. (The refusal itself is
// logged as a warning rather than an error - see the level test below.)

TEST(settings_http, plaintext_source_is_refused_by_default) {
  loopback_listener server;
  temp_dir cache;
  http_test_core core(cache.path(), false);
  settings::settings_http s(&core, "test", http_url(server.port()));

  EXPECT_FALSE(server.served());
}

TEST(settings_http, the_plaintext_refusal_is_a_warning_not_an_error) {
  // Refusing the fetch is not a failed configuration read: initial_load()
  // carries on with the cached copy, so the agent boots with the
  // configuration it already had. Logging it as an error would make the MSI
  // upgrade path treat the host's configuration as unreadable - and because
  // the refusal is standing policy rather than a transient fetch failure, it
  // would fail on every upgrade, forever.
  loopback_listener server;
  temp_dir cache;
  recording_http_core core(cache.path(), false);
  settings::settings_http s(&core, "test", http_url(server.port()));

  EXPECT_FALSE(server.served());
  EXPECT_TRUE(recording_logger::any_contains(core.recorded().warnings(), "Refusing to fetch settings"));
  EXPECT_FALSE(recording_logger::any_contains(core.recorded().errors(), "Refusing to fetch settings"));
}

TEST(settings_http, plaintext_source_is_fetched_when_boot_ini_allows_it) {
  // The escape hatch has to actually work: a lab or air-gapped network that
  // sets [tls] allow plaintext = true still gets its configuration.
  loopback_http server("HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
  temp_dir cache;
  http_test_core core(cache.path(), true);
  settings::settings_http s(&core, "test", http_url(server.port()));

  EXPECT_FALSE(server.request_line().empty());
}

TEST(settings_http, a_url_without_a_scheme_is_refused_like_plain_http) {
  // parse() leaves the protocol empty for "127.0.0.1:port/path", and the
  // client then opens a plain socket just the same - so the guard cannot key
  // on the literal string "http".
  loopback_listener server;
  temp_dir cache;
  http_test_core core(cache.path(), false);
  settings::settings_http s(&core, "test", "127.0.0.1:" + std::to_string(server.port()) + "/settings.ini");

  EXPECT_FALSE(server.served());
}

// --- a remote configuration that pulls in more remote files -----------------
//
// A fetched file is free to name further remote stores, and both forms end up
// below this one in the instance tree:
//
//   [/includes]    -> a nested settings_http, two levels down (our child is the
//                     INI store on our cached copy; *its* child is the include)
//   [/attachments] -> a file cache_remote_file writes next to the agent
//
// house_keeping is the only thing in the process that re-downloads anything,
// so it is the only thing that can keep either of them current. It used to
// stop at this store, which left both pinned to whatever they held at boot
// unless the top-level file happened to change - and on a server where the
// top-level file is the stable part, that is never.

namespace {

// A loopback server that outlives a single fetch. Every test below asks for
// the same url at least twice (once at construction, once per housekeeping
// pass), and each response closes the connection because execute() reads the
// body to EOF rather than honouring Content-Length.
//
// Bodies are keyed by request path and may be swapped between passes, which is
// how a test spells "the operator edited the file on the settings server". The
// per-path hit count is what proves a fetch did, or did not, happen.
class serving_http {
 public:
  serving_http() : port_(0), running_(true) {
    std::promise<unsigned short> p;
    std::future<unsigned short> f = p.get_future();
    thread_ = std::thread([this, prom = std::move(p)]() mutable {
      try {
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, {tcp::v4(), 0});
        prom.set_value(acceptor.local_endpoint().port());
        for (;;) {
          boost::system::error_code ec;
          tcp::socket socket(io);
          acceptor.accept(socket, ec);
          if (ec || !running_) return;
          serve_one(socket);
        }
      } catch (...) {
      }
    });
    port_ = f.get();
  }

  ~serving_http() { stop(); }

  serving_http(const serving_http &) = delete;
  serving_http &operator=(const serving_http &) = delete;

  unsigned short port() const { return port_; }

  void serve(const std::string &path, const std::string &body) {
    const std::lock_guard<std::mutex> lock(mutex_);
    bodies_[path] = body;
  }

  // How many times this path has been asked for since the server started.
  int hits(const std::string &path) {
    const std::lock_guard<std::mutex> lock(mutex_);
    const std::map<std::string, int>::const_iterator it = hits_.find(path);
    return it == hits_.end() ? 0 : it->second;
  }

  std::string url(const std::string &path) const { return "http://127.0.0.1:" + std::to_string(port_) + path; }

 private:
  void serve_one(tcp::socket &socket) {
    boost::system::error_code ec;
    boost::asio::streambuf request;
    boost::asio::read_until(socket, request, "\r\n\r\n", ec);
    std::istream is(&request);
    std::string line;
    std::getline(is, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();

    // "GET /fleet.ini HTTP/1.0" - the middle token is what was asked for. The
    // destructor's own unblocking connection sends nothing, so a line with no
    // space in it means there is nothing to answer.
    const std::string::size_type start = line.find(' ');
    if (start == std::string::npos) return;
    const std::string::size_type end = line.find(' ', start + 1);
    const std::string path = line.substr(start + 1, end == std::string::npos ? std::string::npos : end - start - 1);

    std::string body;
    bool found = false;
    {
      const std::lock_guard<std::mutex> lock(mutex_);
      hits_[path]++;
      const std::map<std::string, std::string>::const_iterator it = bodies_.find(path);
      if (it != bodies_.end()) {
        body = it->second;
        found = true;
      }
    }

    const std::string status = found ? "HTTP/1.0 200 OK\r\n" : "HTTP/1.0 404 Not Found\r\n";
    const std::string response = status + "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    boost::asio::write(socket, boost::asio::buffer(response), ec);
    socket.shutdown(tcp::socket::shutdown_both, ec);
  }

  void stop() {
    if (!thread_.joinable()) return;
    running_ = false;
    try {
      boost::asio::io_context io;
      tcp::socket probe(io);
      boost::system::error_code ec;
      probe.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port_), ec);
    } catch (...) {
    }
    thread_.join();
  }

  unsigned short port_;
  std::atomic<bool> running_;
  std::mutex mutex_;
  std::map<std::string, std::string> bodies_;
  std::map<std::string, int> hits_;
  std::thread thread_;
};

// A core that builds children the way NSCSettingsImpl::create_instance does,
// rather than returning the null the base mock hands back. Without it the
// instance tree stops at this store and there is nothing below it to refresh -
// which is exactly the shape these tests need to exercise.
class chaining_http_core : public http_test_core {
 public:
  chaining_http_core(boost::filesystem::path cache, boost::filesystem::path shared) : http_test_core(std::move(cache)), shared_(std::move(shared)) {}

  std::string expand_path(std::string key) override {
    str::utils::replace(key, "${shared-path}", shared_.string());
    return http_test_core::expand_path(key);
  }

  settings::instance_raw_ptr create_instance(std::string alias, std::string key) override {
    const net::url url = net::parse(key);
    if (url.protocol == "http" || url.protocol == "https") return settings::instance_raw_ptr(new settings::settings_http(this, alias, key));
    return settings::instance_raw_ptr(new settings::INISettings(this, alias, key));
  }

 private:
  boost::filesystem::path shared_;
};

const char *kRootPath = "/nsclient.ini";
const char *kIncludePath = "/fleet.ini";

std::string root_including(const std::string &include_url) { return std::string("[/includes]\nfleet = ") + include_url + "\n"; }

}  // namespace

TEST(settings_http, a_value_from_an_included_url_is_readable) {
  // The baseline the refresh tests build on: an [/includes] entry naming a url
  // resolves through the nested store, so /modules is served from fleet.ini
  // even though the top-level file has no such section.
  serving_http server;
  server.serve(kRootPath, root_including(server.url(kIncludePath)));
  server.serve(kIncludePath, "[/modules]\nCheckSystem = enabled\n");

  temp_dir cache, shared;
  chaining_http_core core(cache.path(), shared.path());
  settings::settings_http s(&core, "test", server.url(kRootPath));

  EXPECT_EQ(s.get_string("/modules", "CheckSystem", ""), "enabled");
}

TEST(settings_http, an_included_url_is_refetched_when_the_top_level_file_is_unchanged) {
  // The reported case. Overriding house_keeping without chaining to the base
  // stopped the walk at this store, so the nested store was only ever
  // re-downloaded when the *top-level* file changed and the whole subtree was
  // rebuilt from scratch. Point an agent at a stable nsclient.ini which
  // includes the file that actually moves, and the include is pinned for the
  // lifetime of the process.
  serving_http server;
  server.serve(kRootPath, root_including(server.url(kIncludePath)));
  server.serve(kIncludePath, "[/modules]\nCheckSystem = enabled\n");

  temp_dir cache, shared;
  chaining_http_core core(cache.path(), shared.path());
  settings::settings_http s(&core, "test", server.url(kRootPath));
  ASSERT_EQ(s.get_string("/modules", "CheckSystem", ""), "enabled");
  ASSERT_EQ(s.get_string("/modules", "NRDPClient", ""), "");

  // Only the include moves; the top-level file is served byte-identical.
  server.serve(kIncludePath, "[/modules]\nCheckSystem = enabled\nNRDPClient = enabled\n");
  s.house_keeping();

  EXPECT_EQ(s.get_string("/modules", "NRDPClient", ""), "enabled");
  EXPECT_EQ(s.get_string("/modules", "CheckSystem", ""), "enabled") << "refreshing the include must not lose what it already had";
  EXPECT_TRUE(core.needs_reload()) << "a changed include has to reach the scheduler, or nothing reloads to apply it";
}

TEST(settings_http, an_unchanged_included_url_is_still_asked_for) {
  // The check above would pass for the wrong reason if the include were never
  // fetched again at all: hash comparison, not the absence of a request, is
  // what makes an unchanged include cheap.
  serving_http server;
  server.serve(kRootPath, root_including(server.url(kIncludePath)));
  server.serve(kIncludePath, "[/modules]\nCheckSystem = enabled\n");

  temp_dir cache, shared;
  chaining_http_core core(cache.path(), shared.path());
  settings::settings_http s(&core, "test", server.url(kRootPath));
  ASSERT_EQ(server.hits(kIncludePath), 1);

  s.house_keeping();

  EXPECT_EQ(server.hits(kIncludePath), 2);
  EXPECT_FALSE(core.needs_reload()) << "nothing changed, so nothing should ask the agent to reload";
}

TEST(settings_http, a_changed_top_level_file_does_not_fetch_its_includes_twice) {
  // Why house_keeping returns early when our own copy changed: reload_data has
  // already discarded the children and rebuilt them, and building the nested
  // store downloads the include as part of its own construction. Recursing as
  // well would ask for every include a second time in the same pass - on a
  // fleet server, once per agent per interval.
  serving_http server;
  server.serve(kRootPath, root_including(server.url(kIncludePath)));
  server.serve(kIncludePath, "[/modules]\nCheckSystem = enabled\n");

  temp_dir cache, shared;
  chaining_http_core core(cache.path(), shared.path());
  settings::settings_http s(&core, "test", server.url(kRootPath));
  ASSERT_EQ(server.hits(kIncludePath), 1);

  server.serve(kRootPath, root_including(server.url(kIncludePath)) + "[/settings/default]\nallowed hosts = 10.0.0.1\n");
  s.house_keeping();

  EXPECT_EQ(server.hits(kIncludePath), 2) << "the rebuilt subtree already fetched it";
  EXPECT_EQ(s.get_string("/settings/default", "allowed hosts", ""), "10.0.0.1");
  EXPECT_EQ(s.get_string("/modules", "CheckSystem", ""), "enabled") << "the include has to survive the rebuild";
}

TEST(settings_http, an_attachment_is_refetched_when_the_top_level_file_is_unchanged) {
  // Attachments were in the same position as includes: fetch_attachments ran
  // at construction and inside the "our own file changed" branch, so an
  // external script served alongside a stable nsclient.ini was written once at
  // boot and never updated again.
  serving_http server;
  server.serve(kRootPath, "[/attachments]\nscripts/check.bat = " + server.url("/check.bat") + "\n");
  server.serve("/check.bat", "@echo first\n");

  temp_dir cache, shared;
  chaining_http_core core(cache.path(), shared.path());
  settings::settings_http s(&core, "test", server.url(kRootPath));

  const boost::filesystem::path written = shared.path() / "scripts" / "check.bat";
  ASSERT_TRUE(boost::filesystem::is_regular_file(written));
  ASSERT_EQ(file_helpers::read_file_as_string(written), "@echo first\n");

  server.serve("/check.bat", "@echo second\n");
  s.house_keeping();

  EXPECT_EQ(file_helpers::read_file_as_string(written), "@echo second\n");
}
