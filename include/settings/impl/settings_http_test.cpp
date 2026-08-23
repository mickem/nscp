// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio.hpp>
#include <future>
#include <settings/impl/settings_http.hpp>
#include <settings/test_helpers.hpp>
#include <str/utils.hpp>
#include <thread>

using settings_test::mock_settings_core;
using settings_test::temp_dir;

namespace {

// A settings_core that overrides expand_path so the CACHE_FOLDER token
// (defined as "${cache-folder}" in config.h) resolves to a real, writable
// temp directory.  Mirrors what NSCSettingsImpl does in production.
class http_test_core : public mock_settings_core {
 public:
  explicit http_test_core(boost::filesystem::path cache) : cache_(std::move(cache)) {}

  std::string expand_path(std::string key) override {
    if (key == CACHE_FOLDER) return cache_.string();
    return key;
  }

 private:
  boost::filesystem::path cache_;
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

TEST(settings_http, attachment_target_without_a_placeholder_is_unchanged) {
  // Attachments have always been declared as plain paths; those must resolve
  // exactly as before.
  attachment_core core;
  EXPECT_EQ(settings::settings_http::resolve_attachment_target(&core, "scripts/myscript.bat"), "scripts/myscript.bat");
  EXPECT_EQ(settings::settings_http::resolve_attachment_target(&core, "${shared-path}/scripts/myscript.bat"), "/etc/nsclient/scripts/myscript.bat");
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
