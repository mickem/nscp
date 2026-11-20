// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The check_certificate keyword registry, over fabricated certificates. The
// OpenSSL source is covered by check_certificate_test.cpp; this is the half
// that turns a parsed certificate into thresholds, a message and perf data,
// and is identical on Windows (cert_source_win.cpp) and everywhere else.

#include "cert_filter.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/helpers.hpp>
#include <string>
#include <vector>

namespace {

const long long day = 86400;

cert_filter::filter_obj_ptr make_cert(const long long expires_in_secs) {
  parsers::where::constants::reset();
  const long long now = parsers::where::constants::get_now();
  auto c = std::make_shared<cert_filter::filter_obj>();
  c->subject = "/CN=host.example.com";
  c->issuer = "/CN=Example CA";
  c->thumbprint = "0123456789abcdef0123456789abcdef01234567";
  c->serial = "1a2b";
  c->source = "/etc/ssl/host.pem";
  c->store = "file";
  c->not_before = now - 30 * day;
  c->not_after = now + expires_in_secs;
  c->signature_algorithm = "sha256WithRSAEncryption";
  c->key_type = "RSA";
  c->key_size = 2048;
  c->trusted = 1;
  return c;
}

struct result {
  PB::Common::ResultCode code;
  std::string message;
  PB::Commands::QueryResponseMessage::Response response;
};

// Drive the filter with check_certificate's defaults over the given certs.
result run(const std::vector<std::string> &args, const std::vector<cert_filter::filter_obj_ptr> &certs) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_certificate");
  for (const std::string &a : args) request.add_arguments(a);
  result r;
  modern_filter::data_container data;
  modern_filter::cli_helper<cert_filter::filter> helper(request, &r.response, data);
  cert_filter::filter filter;
  helper.add_options("expires_in < 30 or weak_signature = 1", "expires_in < 7 or expired = 1", "", filter.get_filter_syntax(), "unknown");
  helper.add_syntax("${status}: ${problem_list}", "${subject} expires in ${expires_in}d", "${subject}", "No certificates found", "%(status): all ok");
  if (helper.parse_options() && helper.build_filter(filter)) {
    for (const auto &c : certs) filter.match(c);
    helper.post_process(filter);
  }
  r.code = r.response.result();
  for (int i = 0; i < r.response.lines_size(); ++i) r.message += r.response.lines(i).message();
  return r;
}

}  // namespace

TEST(CertFilter, DerivedTimeFields) {
  const auto valid = make_cert(10 * day + 60);
  EXPECT_EQ(10, valid->get_expires_in_days());
  EXPECT_EQ(10 * day + 60, valid->get_expires_in_sec());
  EXPECT_EQ(0, valid->get_expired());
  EXPECT_EQ(0, valid->get_not_yet_valid());
  EXPECT_EQ("/CN=host.example.com", valid->show());

  const auto expired = make_cert(-2 * day - 60);
  EXPECT_EQ(-2, expired->get_expires_in_days());
  EXPECT_EQ(1, expired->get_expired());

  auto future = make_cert(400 * day);
  future->not_before = parsers::where::constants::get_now() + day;
  EXPECT_EQ(1, future->get_not_yet_valid());
}

TEST(CertFilter, DefaultThresholdsOnExpiry) {
  EXPECT_EQ(PB::Common::ResultCode::OK, run({}, {make_cert(90 * day)}).code);
  const result warn = run({}, {make_cert(20 * day + 60)});
  EXPECT_EQ(PB::Common::ResultCode::WARNING, warn.code);
  EXPECT_EQ("WARNING: /CN=host.example.com expires in 20d", warn.message);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({}, {make_cert(3 * day)}).code);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({}, {make_cert(-day)}).code);
}

TEST(CertFilter, SecurityFlags) {
  auto weak = make_cert(90 * day);
  weak->weak_signature = 1;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run({}, {weak}).code);

  auto self_signed = make_cert(90 * day);
  self_signed->self_signed = 1;
  self_signed->trusted = 0;
  self_signed->key_size = 1024;
  self_signed->weak_key = 1;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"critical=self_signed = 1"}, {self_signed}).code);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"critical=trusted = 0"}, {self_signed}).code);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"critical=weak_key = 1 and key_size < 2048"}, {self_signed}).code);
  EXPECT_EQ(PB::Common::ResultCode::OK, run({"critical=weak_key = 1"}, {make_cert(90 * day)}).code);

  auto future = make_cert(400 * day);
  future->not_before = parsers::where::constants::get_now() + day;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"critical=not_yet_valid = 1"}, {future}).code);
}

TEST(CertFilter, StringKeywordsFilterAndRender) {
  const auto c = make_cert(90 * day);
  const result r = run(
      {"warning=none", "critical=none", "detail-syntax=${issuer}|${thumbprint}|${serial}|${source}|${store}|${signature_algorithm}|${key_type}|${key_size}"},
      {c});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code);
  EXPECT_EQ("OK: all ok", r.message);

  const result listed = run({"warning=none", "critical=key_size > 0", "top-syntax=${list}",
                             "detail-syntax=${issuer}|${thumbprint}|${serial}|${source}|${store}|${signature_algorithm}|${key_type}|${key_size}"},
                            {c});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, listed.code);
  EXPECT_EQ("/CN=Example CA|0123456789abcdef0123456789abcdef01234567|1a2b|/etc/ssl/host.pem|file|sha256WithRSAEncryption|RSA|2048", listed.message);

  // A filter that drops every certificate falls through to the empty state.
  const result none = run({"filter=store = 'LocalMachine\\My'"}, {make_cert(day)});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, none.code);
  EXPECT_EQ("No certificates found", none.message);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"filter=subject like 'host.example'"}, {make_cert(day)}).code);
}

TEST(CertFilter, DatesRenderAsText) {
  auto c = make_cert(90 * day);
  c->not_before = 1700000000;  // 2023-11-14 22:13:20 UTC
  c->not_after = 1900000000;   // 2030-03-17 17:46:40 UTC
  const result r = run({"warning=none", "critical=expires_in_days > 0", "top-syntax=${list}", "detail-syntax=${valid_from} - ${valid_to}"}, {c});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code);
  EXPECT_EQ(c->get_not_before_su() + " - " + c->get_not_after_su(), r.message);
  EXPECT_NE(std::string::npos, r.message.find("2023-11-14")) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("2030-03-17")) << r.message;

  // not_after / not_before are dates: compared against a relative offset.
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"warning=none", "critical=not_after < 30d"}, {make_cert(10 * day)}).code);
  EXPECT_EQ(PB::Common::ResultCode::OK, run({"warning=none", "critical=not_after < 30d"}, {make_cert(60 * day)}).code);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"warning=none", "critical=expires_in_sec < 3600"}, {make_cert(60)}).code);
}

TEST(CertFilter, ExpiryPerfData) {
  const result r = run({}, {make_cert(90 * day + 60)});
  ASSERT_EQ(PB::Common::ResultCode::OK, r.code);
  ASSERT_EQ(1, r.response.lines_size());
  bool found = false;
  for (const auto &p : r.response.lines(0).perf()) {
    if (p.alias() == "/CN=host.example.com") {
      found = true;
      EXPECT_DOUBLE_EQ(90, p.float_value().value());
      EXPECT_EQ("d", p.float_value().unit());
    }
  }
  EXPECT_TRUE(found) << r.response.DebugString();
}

TEST(CertFilter, NoCertificatesUsesTheEmptyState) {
  const result r = run({}, {});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("No certificates found", r.message);
}
