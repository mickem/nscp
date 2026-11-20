// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_domain.hpp"

#include <gtest/gtest.h>

#include "check_domain_internal.hpp"
#include "loopback_test_support.hpp"

namespace {
using namespace check_net::domain;

TEST(CheckDomain, RedirectsRemoveDotSegmentsWithoutChangingQueryOrEscapedDots) {
  using internal::redirected_url;
  const std::string base = "https://example.com/nested/redirect?old=../keep";
  EXPECT_EQ("https://example.com:443/final", redirected_url(base, "../final"));
  EXPECT_EQ("https://example.com:443/final", redirected_url(base, "../../../../final"));
  EXPECT_EQ("https://example.com:443/nested/final", redirected_url(base, "./final"));
  EXPECT_EQ("https://example.com:443/final?x=../keep", redirected_url(base, "../final?x=../keep"));
  EXPECT_EQ("https://example.com:443/nested/redirect?new=../keep", redirected_url(base, "?new=../keep"));
  EXPECT_EQ("https://example.com:443/nested/", redirected_url(base, "."));
  EXPECT_EQ("https://example.com:443/", redirected_url(base, ".."));
  EXPECT_EQ("https://example.com:443/nested/%2e%2e/final", redirected_url(base, "%2e%2e/final"));
  EXPECT_EQ("https://example.com:443/a//final", redirected_url(base, "/a//b/../final"));
  EXPECT_EQ("https://other.test/final", redirected_url(base, "https://other.test/a/../final"));
  EXPECT_EQ("https://other.test/final", redirected_url(base, "//other.test/a/../final"));
  EXPECT_EQ("https://example.com:443/final?next=https://other.test/a/../b", redirected_url(base, "../final?next=https://other.test/a/../b"));
  EXPECT_EQ("https://other.test/?next=../keep", redirected_url(base, "https://other.test?next=../keep"));
  EXPECT_EQ("https://[::1]:8443/final", redirected_url("https://[::1]:8443/nested/redirect", "../final"));
  EXPECT_THROW(redirected_url(base, "http://other.test/a/../final"), std::exception);
}

TEST(CheckDomain, NormalizesAndValidatesNames) {
  EXPECT_EQ("example.com", normalize_domain("EXAMPLE.COM."));
  EXPECT_EQ("xn--bcher-kva.de", normalize_domain("xn--bcher-kva.de"));
  for (const auto &name : {"", "com", "https://example.com", "example.com/path", "example.com?x", "example..com", "-example.com", "example-.com", "127.0.0.1",
                           "example.com\r\nother.com", "example.com ", "example_.com"})
    EXPECT_THROW(normalize_domain(name), std::exception) << name;
  EXPECT_THROW(normalize_domain(std::string(64, 'a') + ".com"), std::exception);
}

TEST(CheckDomain, TimestampsAndRounding) {
  EXPECT_THROW(parse_timestamp("2028-02-29T12:00:00." + std::string(10000, '1') + "Z"), std::exception);
  const auto utc = parse_timestamp("2028-02-29T12:00:00Z");
  EXPECT_EQ(utc, parse_timestamp("2028-02-29T14:30:00+02:30"));
  EXPECT_EQ(utc, parse_timestamp("2028-02-29t10:00:00-02:00"));
  EXPECT_EQ(utc + boost::posix_time::microseconds(123456), parse_timestamp("2028-02-29T12:00:00.123456789z"));
  EXPECT_EQ(-1, days_remaining(utc - boost::posix_time::microseconds(1), utc));
  EXPECT_EQ(-1, days_remaining(utc - boost::posix_time::hours(24), utc));
  EXPECT_EQ(-2, days_remaining(utc - boost::posix_time::hours(25), utc));
  EXPECT_EQ(0, days_remaining(utc, utc));
  EXPECT_EQ(0, days_remaining(utc + boost::posix_time::hours(23), utc));
  EXPECT_EQ(30, days_remaining(utc + boost::posix_time::hours(720), utc));
  for (const auto &s : {"2027-02-29T12:00:00Z", "2028-02-29", "2028-02-29T12:00:00", "2028-02-29T24:00:00Z", "2028-02-29T12:60:00Z",
                        "2028-02-29T12:00:00+24:00", "2028-02-29T12:00:00-00:00", "2028-02-29T12:00:00Zgarbage"})
    EXPECT_THROW(parse_timestamp(s), std::exception) << s;
}

TEST(CheckDomain, RegistrarDateTakesPrecedenceAndNestedEventsAreIgnored) {
  const auto record = parse_rdap(R"({"objectClassName":"domain","ldhName":"EXAMPLE.COM","events":[
    {"eventAction":"registration","eventDate":"invalid unused date"},
    {"eventAction":"expiration","eventDate":"2029-01-01T00:00:00Z"},
    {"eventAction":"registrar expiration","eventDate":"2028-01-01T00:00:00Z"}],
    "entities":[{"events":[{"eventAction":"expiration","eventDate":"2000-01-01T00:00:00Z"}]}],
    "links":[{"rel":"related","type":"application/rdap+json","href":"https://registrar.test/domain/example.com"}]})",
                                 "example.com");
  EXPECT_EQ("registrar", record.expiration_type);
  EXPECT_EQ(parse_timestamp("2028-01-01T00:00:00Z"), record.expiration);
  EXPECT_EQ("https://registrar.test/domain/example.com", record.registrar_url);
  EXPECT_TRUE(parse_rdap(R"({"objectClassName":"domain","ldhName":"example.com","entities":[{"events":[
    {"eventAction":"expiration","eventDate":"2030-01-01T00:00:00Z"}]}]})",
                         "example.com")
                  .expiration.is_not_a_date_time());
}

TEST(CheckDomain, RejectsWrongDomainMalformedOrConflictingRdap) {
  for (const auto &body : {"not json", "[]", "{}", R"({"objectClassName":"domain","ldhName":"other.com"})",
                           R"({"objectClassName":"entity","ldhName":"example.com"})", R"({"objectClassName":"domain","ldhName":"example.com","events":{}})",
                           R"({"objectClassName":"domain","ldhName":"example.com","events":[{"eventAction":"expiration","eventDate":"tomorrow"}]})",
                           R"({"objectClassName":"domain","ldhName":"example.com","events":[
                             {"eventAction":"expiration","eventDate":"2028-01-01T00:00:00Z"},
                             {"eventAction":"expiration","eventDate":"2029-01-01T00:00:00Z"}]})"})
    EXPECT_THROW(parse_rdap(body, "example.com"), std::exception) << body;
}

TEST(CheckDomain, RegistryDateAndEquivalentDuplicates) {
  const auto record = parse_rdap(R"({"objectClassName":"domain","ldhName":"example.com","events":[
    {"eventAction":"expiration","eventDate":"2028-01-01T00:00:00Z"},
    {"eventAction":"expiration","eventDate":"2028-01-01T01:00:00+01:00"}]})",
                                 "example.com");
  EXPECT_EQ("registry", record.expiration_type);
  EXPECT_EQ(parse_timestamp("2028-01-01T00:00:00Z"), record.expiration);
}

TEST(CheckDomain, WhoisParsesOnlyKnownUnambiguousFields) {
  const auto record = parse_whois(
      "Domain Name: EXAMPLE.COM\r\nRegistry Expiry Date: 2029-01-01T00:00:00Z\r\n"
      "Registrar Registration Expiration Date: 2028-01-01T00:00:00Z\r\n",
      "example.com");
  EXPECT_EQ("registrar", record.expiration_type);
  EXPECT_EQ(parse_timestamp("2028-01-01T00:00:00Z"), record.expiration);
  const auto generic = parse_whois("domain: example.com\npaid-till: 2028-01-01\n", "example.com");
  EXPECT_EQ(record.expiration, generic.expiration);
  EXPECT_EQ("whois", generic.expiration_type);
  for (const auto &text : {"Creation Date: 2028-01-01T00:00:00Z", "Expires: 01/02/2028", "No match", "Expires: tomorrow",
                           "Domain Name: other.com\nExpires: 2028-01-01", "Expires: 2028-01-01\nExpires: 2029-01-01"})
    EXPECT_THROW(parse_whois(text, "example.com"), std::exception) << text;
}
}  // namespace

// ============================================================================
// The check_domain command: option validation, and full lookups against a
// loopback RDAP (TLS) or WHOIS server, so nothing leaves the host.
// ============================================================================

namespace {
using namespace check_net_test;

struct domain_result {
  PB::Common::ResultCode code;
  std::string message;
  std::string perf;
};

domain_result run_domain(const std::vector<std::string> &args, const std::string &default_ca = "") {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_domain");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  check_net::check_domain(default_ca, request, &response);
  domain_result r{response.result(), "", ""};
  for (int i = 0; i < response.lines_size(); ++i) {
    r.message += response.lines(i).message();
    for (const auto &p : response.lines(i).perf()) r.perf += p.alias() + "=" + std::to_string(static_cast<long long>(p.float_value().value())) + ";";
  }
  return r;
}

bool contains(const std::string &haystack, const std::string &needle) { return haystack.find(needle) != std::string::npos; }

// An RDAP URL that refuses the connection straight away.
std::string dead_rdap() { return "rdap-url=https://127.0.0.1:" + closed_port() + "/domain/{domain}"; }

// The expiration `days` (and a bit) from now, as WHOIS/RDAP spell it.
std::string expiry_in(const int days) {
  const auto t = boost::posix_time::second_clock::universal_time() + boost::posix_time::hours(24 * days + 12);
  return boost::posix_time::to_iso_extended_string(t) + "Z";
}

// A WHOIS server that checks the query line and answers with `body`.
script_type whois_reply(const std::string &body, std::string *query = nullptr) {
  return [body, query](tcp::socket &s) {
    const std::string line = read_line(s);
    if (query) *query = line;
    send(s, body);
    boost::system::error_code ec;
    s.shutdown(tcp::socket::shutdown_both, ec);
  };
}
}  // namespace

TEST(CheckDomainCommand, RejectsOutOfRangeTimeout) {
  for (const auto &timeout : {"timeout=0", "timeout=301"}) {
    const domain_result r = run_domain({"domain=example.com", timeout});
    EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << timeout;
    EXPECT_EQ("timeout must be between 1 and 300 seconds", r.message) << timeout;
  }
}

TEST(CheckDomainCommand, WhoisFallbackNeedsAServerAndPort) {
  const char *expected = "whois-fallback requires whois-server and a nonzero whois-port";
  EXPECT_EQ(expected, run_domain({"domain=example.com", "whois-fallback=true"}).message);
  EXPECT_EQ(expected, run_domain({"domain=example.com", "whois-fallback=true", "whois-server=127.0.0.1", "whois-port=0"}).message);
}

TEST(CheckDomainCommand, InvalidDomainIsReportedWithoutALookup) {
  const domain_result r = run_domain({"domain=https://example.com", dead_rdap()});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_TRUE(contains(r.message, "Domain lookup failed: ")) << r.message;
}

TEST(CheckDomainCommand, RdapFailureWithoutFallbackIsUnknown) {
  const domain_result r = run_domain({"domain=example.com", dead_rdap(), "timeout=5"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "Domain lookup failed: ")) << r.message;
}

TEST(CheckDomainCommand, NonHttpsRdapUrlIsRejected) {
  const domain_result r = run_domain({"domain=example.com", "rdap-url=http://127.0.0.1/{domain}"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_TRUE(contains(r.message, "RDAP requires an HTTPS URL")) << r.message;
}

TEST(CheckDomainCommand, WhoisFallbackAnswersWhenRdapIsDown) {
  std::string query;
  scripted_server whois(whois_reply("Domain Name: EXAMPLE.COM\r\nRegistry Expiry Date: " + expiry_in(90) + "\r\n", &query));
  const domain_result r =
      run_domain({"domain=Example.COM.", dead_rdap(), "timeout=5", "whois-fallback=true", "whois-server=127.0.0.1", "whois-port=" + whois.port()});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_EQ("example.com\r\n", query);
  EXPECT_EQ(0u, r.message.find("OK: example.com expires in 90d (")) << r.message;
  EXPECT_TRUE(contains(r.message, "Z, registry, whois:127.0.0.1:" + whois.port() + ")")) << r.message;
  EXPECT_TRUE(contains(r.perf, "example.com=90;")) << r.perf;
}

TEST(CheckDomainCommand, ThresholdsAndDetailOverWhois) {
  // A date-only expiry is midnight of that day, so cutting the time off
  // expiry_in() loses up to a day: 21 days and a bit from now leaves 20 or
  // 21 whole days, whatever the time of day. With 20 it left 19 through
  // every morning (UTC), and the "2" below did not match.
  scripted_server whois(whois_reply("domain: example.com\nexpires: " + expiry_in(21).substr(0, 10) + "\n"));
  const domain_result r = run_domain({"domain=example.com", dead_rdap(), "timeout=5", "whois-fallback=true", "whois-server=127.0.0.1",
                                      "whois-port=" + whois.port(), "detail-syntax=${domain} ${expires_in} ${expiration_type} ${source}"});
  EXPECT_EQ(PB::Common::ResultCode::WARNING, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "example.com 2")) << r.message;
  EXPECT_TRUE(contains(r.message, " whois whois:127.0.0.1:" + whois.port())) << r.message;
}

TEST(CheckDomainCommand, BothSourcesFailingNamesEach) {
  scripted_server whois(whois_reply("No match for domain\r\n"));
  const domain_result r =
      run_domain({"domain=example.com", dead_rdap(), "timeout=5", "whois-fallback=true", "whois-server=127.0.0.1", "whois-port=" + whois.port()});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_TRUE(contains(r.message, "RDAP: ")) << r.message;
  EXPECT_TRUE(contains(r.message, "; WHOIS: No supported WHOIS expiration date")) << r.message;
}

#ifdef USE_SSL
namespace {
// A TLS RDAP server answering one request with `status` and `body`.
script_type rdap_reply(const test_identity &id, const int status, const std::string &body, const std::string &headers = "", std::string *head = nullptr) {
  return tls_server(id, [status, body, headers, head](boost::asio::ssl::stream<tcp::socket &> &stream) {
    std::string data;
    char buf[4096];
    boost::system::error_code ec;
    while (!ec && data.find("\r\n\r\n") == std::string::npos) data.append(buf, stream.read_some(boost::asio::buffer(buf), ec));
    if (head) *head = data;
    const std::string reply = http_response(status, body, headers);
    boost::asio::write(stream, boost::asio::buffer(reply), ec);
    stream.shutdown(ec);
  });
}
}  // namespace

TEST(CheckDomainCommand, RdapLookupOverVerifiedTls) {
  test_identity id;
  const std::string ca = id.write_pem();
  std::string head;
  scripted_server server(rdap_reply(
      id, 200, R"({"objectClassName":"domain","ldhName":"example.com","events":[{"eventAction":"expiration","eventDate":")" + expiry_in(5) + R"("}]})",
      "Content-Type: application/rdap+json\r\n", &head));
  const std::string url = "https://localhost:" + server.port() + "/domain/{domain}";
  const domain_result r = run_domain({"domain=example.com", "rdap-url=" + url, "timeout=5", "detail-syntax=${expires_in} ${expiration_type} ${source}"}, ca);
  boost::filesystem::remove(ca);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_EQ("CRITICAL: 5 registry rdap:https://localhost:" + server.port() + "/domain/example.com", r.message);
  EXPECT_TRUE(contains(head, "GET /domain/example.com HTTP/1.1")) << head;
  EXPECT_TRUE(contains(head, "User-Agent: NSClient++ check_domain")) << head;
}

TEST(CheckDomainCommand, RdapHttpErrorIsReported) {
  test_identity id;
  const std::string ca = id.write_pem();
  scripted_server server(rdap_reply(id, 404, "not found"));
  const domain_result r = run_domain({"domain=example.com", "rdap-url=https://localhost:" + server.port() + "/{domain}", "ca=" + ca, "timeout=5"});
  boost::filesystem::remove(ca);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_TRUE(contains(r.message, "RDAP returned HTTP 404")) << r.message;
}

TEST(CheckDomainCommand, RdapWithoutExpirationIsAnError) {
  test_identity id;
  const std::string ca = id.write_pem();
  scripted_server server(rdap_reply(id, 200, R"({"objectClassName":"domain","ldhName":"example.com"})"));
  const domain_result r = run_domain({"domain=example.com", "rdap-url=https://localhost:" + server.port() + "/{domain}", "ca=" + ca, "timeout=5"});
  boost::filesystem::remove(ca);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_TRUE(contains(r.message, "No RDAP expiration event for domain")) << r.message;
}
#endif  // USE_SSL
