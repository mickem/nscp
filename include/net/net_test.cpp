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

// --- request-line safety ----------------------------------------------------

TEST(net_url, encode_query_leaves_legal_characters_alone) {
  // Everything RFC 3986 permits in a query has to survive verbatim, or the
  // parameters stop meaning what the operator wrote.
  const std::string legal = "RootFolder=myhost/&Filename=nsclient.ini";
  EXPECT_EQ(net::encode_query(legal), legal);
  EXPECT_EQ(net::encode_query("a=1&b=2;c=3,d=4+5:6@7?8*9!$'()~-._"), "a=1&b=2;c=3,d=4+5:6@7?8*9!$'()~-._");
}

TEST(net_url, encode_query_escapes_a_space) {
  EXPECT_EQ(net::encode_query("Folder=my host"), "Folder=my%20host");
}

TEST(net_url, encode_query_escapes_crlf) {
  // The interesting one: unescaped, this splits the request line in two.
  EXPECT_EQ(net::encode_query("a=1\r\nX-Evil: yes"), "a=1%0D%0AX-Evil:%20yes");
}

TEST(net_url, encode_query_does_not_double_encode) {
  // A query written already-encoded must not turn "%20" into "%2520".
  EXPECT_EQ(net::encode_query("Folder=my%20host"), "Folder=my%20host");
  EXPECT_EQ(net::encode_query("a=%2F%2f"), "a=%2F%2f");
}

TEST(net_url, encode_query_escapes_a_stray_percent) {
  // A '%' that introduces no valid pair is not an escape.
  EXPECT_EQ(net::encode_query("a=100%"), "a=100%25");
  EXPECT_EQ(net::encode_query("a=%zz"), "a=%25zz");
  EXPECT_EQ(net::encode_query("a=%2"), "a=%252");
}

TEST(net_url, encode_query_escapes_high_bytes_and_controls) {
  EXPECT_EQ(net::encode_query(std::string("a=\x01")), "a=%01");
  EXPECT_EQ(net::encode_query(std::string("a=\xc3\xa5")), "a=%C3%A5");
}

TEST(net_url, get_request_path_encodes_the_query) {
  const net::url u = net::parse("http://host/cfg.php?Folder=my host");
  EXPECT_EQ(u.get_request_path(), "/cfg.php?Folder=my%20host");
}

// --- log-safe rendering (keeps parameters out of the log) -------------------

TEST(net_url, to_log_safe_string_drops_the_query) {
  const net::url u = net::parse("https://cfgsrv:8443/nsclient.php?token=s3cret&host=a");
  EXPECT_EQ(u.to_log_safe_string(), "https://cfgsrv:8443/nsclient.php");
  EXPECT_EQ(u.to_log_safe_string().find("s3cret"), std::string::npos);
  // to_string() is still the faithful rendering.
  EXPECT_NE(u.to_string().find("token=s3cret"), std::string::npos);
}

TEST(net_url, to_log_safe_string_equals_to_string_without_a_query) {
  const net::url u = net::parse("http://cfgsrv/settings.ini");
  EXPECT_EQ(u.to_log_safe_string(), u.to_string());
}

TEST(net_url, get_baseurl_and_get_path_split_the_url) {
  const net::url u = net::parse("https://cfgsrv:8443/dir/nsclient.php?token=s3cret");
  EXPECT_EQ(u.get_baseurl(), "https://cfgsrv:8443");
  EXPECT_EQ(u.get_path(), "/dir/nsclient.php");
  EXPECT_EQ(u.get_baseurl() + u.get_path(), u.to_log_safe_string());
}

TEST(net_url, get_baseurl_omits_an_unset_port) {
  EXPECT_EQ(net::parse("http://cfgsrv/x.ini").get_baseurl(), "http://cfgsrv");
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
