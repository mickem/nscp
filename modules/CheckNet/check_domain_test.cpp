// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_domain.hpp"

#include <gtest/gtest.h>

#include "check_domain_internal.hpp"

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
