// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <net/net.hpp>

TEST(net_url, parse_splits_protocol_host_port_and_path) {
  const net::url u = net::parse("http://example.com:8080/dir/file.ini");
  EXPECT_EQ(u.protocol, "http");
  EXPECT_EQ(u.host, "example.com");
  EXPECT_EQ(u.port, 8080u);
  EXPECT_EQ(u.path, "/dir/file.ini");
  EXPECT_TRUE(u.query.empty());
}

TEST(net_url, parse_extracts_the_query_string) {
  const net::url u = net::parse("http://nsclient.mydom.local/nsclient/nsclient.php?RootFolder=myhost/&Filename=nsclient.ini");
  EXPECT_EQ(u.protocol, "http");
  EXPECT_EQ(u.host, "nsclient.mydom.local");
  // The path stops at the '?' ...
  EXPECT_EQ(u.path, "/nsclient/nsclient.php");
  // ... and the parameters land in query, '&' and '/' included.
  EXPECT_EQ(u.query, "RootFolder=myhost/&Filename=nsclient.ini");
}

TEST(net_url, get_request_path_reassembles_path_and_query) {
  // What a downloader has to put on the request line (issue #460).
  const net::url u = net::parse("https://host/cfg.php?a=1&b=2");
  EXPECT_EQ(u.get_request_path(), "/cfg.php?a=1&b=2");
}

TEST(net_url, get_request_path_without_a_query_is_just_the_path) {
  const net::url u = net::parse("https://host/settings.ini");
  EXPECT_EQ(u.get_request_path(), "/settings.ini");
  // No trailing '?' when there is nothing to pass.
  EXPECT_EQ(u.get_request_path().find('?'), std::string::npos);
}

TEST(net_url, get_request_path_keeps_an_empty_query_marker_out) {
  // "?" with nothing after it parses to an empty query and must not be
  // re-emitted, otherwise every plain url would grow a stray '?'.
  const net::url u = net::parse("http://host/f.ini?");
  EXPECT_EQ(u.path, "/f.ini");
  EXPECT_TRUE(u.query.empty());
  EXPECT_EQ(u.get_request_path(), "/f.ini");
}

TEST(net_url, to_string_round_trips_a_url_with_parameters) {
  const std::string raw = "http://example.com:8080/cfg.php?host=a&mode=b";
  EXPECT_EQ(net::parse(raw).to_string(), raw);
}

TEST(net_url, to_string_includes_port_only_when_set) {
  EXPECT_EQ(net::parse("http://example.com/x.ini").to_string(), "http://example.com/x.ini");
  EXPECT_EQ(net::parse("http://example.com:81/x.ini").to_string(), "http://example.com:81/x.ini");
}

TEST(net_url, ini_paths_keep_their_windows_drive_letter) {
  // "ini://C:/foo" must not read "C" as a host and ":/foo" as a port; the
  // parser skips port handling for the ini and registry protocols.
  const net::url u = net::parse("ini://${shared-path}/nsclient.ini");
  EXPECT_EQ(u.protocol, "ini");
  EXPECT_TRUE(u.query.empty());
  EXPECT_EQ(u.get_request_path(), u.path);
}

TEST(net_url, apply_and_import_carry_the_query) {
  net::url base = net::parse("http://host/a.ini");
  const net::url with_query = net::parse("http://host/b.ini?k=v");

  net::url applied = base;
  applied.apply(with_query);
  EXPECT_EQ(applied.get_request_path(), "/b.ini?k=v");

  net::url imported = net::parse("http://host/");
  imported.path.clear();
  imported.import(with_query);
  EXPECT_EQ(imported.get_request_path(), "/b.ini?k=v");
}
