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

#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio.hpp>
#include <future>
#include <settings/impl/settings_http.hpp>
#include <settings/test_helpers.hpp>
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

 private:
  std::string body_;
  unsigned short port_;
  std::thread thread_;
};

std::string http_url(unsigned short port, const std::string &path = "/settings.ini") { return "http://127.0.0.1:" + std::to_string(port) + path; }

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
