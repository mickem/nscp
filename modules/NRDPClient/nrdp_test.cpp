/*
 * Copyright (C) 2004-2026 Michael Medin
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

#include "nrdp.hpp"

#include <gtest/gtest.h>

#include <string>

// ============================================================================
// nrdp::data::add_host / add_service / add_command tests
// ============================================================================

TEST(NrdpData, AddHostPopulatesItem) {
  nrdp::data d;
  d.add_host("host-a", 0, "ok message");
  ASSERT_EQ(d.items.size(), 1u);
  const auto& it = d.items.front();
  EXPECT_EQ(it.type, nrdp::data::type_host);
  EXPECT_EQ(it.host, "host-a");
  EXPECT_EQ(it.service, "");
  EXPECT_EQ(it.result, 0);
  EXPECT_EQ(it.message, "ok message");
}

TEST(NrdpData, AddServicePopulatesItem) {
  nrdp::data d;
  d.add_service("host-b", "svc-1", 2, "crit message");
  ASSERT_EQ(d.items.size(), 1u);
  const auto& it = d.items.front();
  EXPECT_EQ(it.type, nrdp::data::type_service);
  EXPECT_EQ(it.host, "host-b");
  EXPECT_EQ(it.service, "svc-1");
  EXPECT_EQ(it.result, 2);
  EXPECT_EQ(it.message, "crit message");
}

TEST(NrdpData, AddMultipleItemsPreservesOrder) {
  nrdp::data d;
  d.add_host("h1", 0, "m1");
  d.add_service("h2", "s2", 1, "m2");
  d.add_service("h3", "s3", 2, "m3");
  ASSERT_EQ(d.items.size(), 3u);
  auto it = d.items.begin();
  EXPECT_EQ(it->type, nrdp::data::type_host);
  EXPECT_EQ(it->host, "h1");
  ++it;
  EXPECT_EQ(it->type, nrdp::data::type_service);
  EXPECT_EQ(it->host, "h2");
  EXPECT_EQ(it->service, "s2");
  EXPECT_EQ(it->result, 1);
  ++it;
  EXPECT_EQ(it->type, nrdp::data::type_service);
  EXPECT_EQ(it->host, "h3");
  EXPECT_EQ(it->service, "s3");
  EXPECT_EQ(it->result, 2);
  EXPECT_EQ(it->message, "m3");
}

// ============================================================================
// nrdp::data::render_request tests
// ============================================================================

TEST(NrdpRender, EmptyProducesCheckresultsRoot) {
  nrdp::data d;
  const std::string xml = d.render_request();
  EXPECT_NE(xml.find("<checkresults"), std::string::npos);
  // No individual <checkresult> entries should be emitted.
  EXPECT_EQ(xml.find("<checkresult "), std::string::npos);
  EXPECT_EQ(xml.find("<checkresult>"), std::string::npos);
}

TEST(NrdpRender, ServiceRendersExpectedFields) {
  nrdp::data d;
  d.add_service("host1", "CPU", 1, "warn message");
  const std::string xml = d.render_request();

  EXPECT_NE(xml.find("type=\"service\""), std::string::npos);
  EXPECT_NE(xml.find("<hostname>host1</hostname>"), std::string::npos);
  EXPECT_NE(xml.find("<servicename>CPU</servicename>"), std::string::npos);
  EXPECT_NE(xml.find("<state>1</state>"), std::string::npos);
  EXPECT_NE(xml.find("<output>warn message</output>"), std::string::npos);
}

TEST(NrdpRender, HostDoesNotEmitServicename) {
  nrdp::data d;
  d.add_host("host2", 0, "host ok");
  const std::string xml = d.render_request();

  EXPECT_NE(xml.find("type=\"host\""), std::string::npos);
  EXPECT_NE(xml.find("<hostname>host2</hostname>"), std::string::npos);
  EXPECT_EQ(xml.find("<servicename"), std::string::npos);
  EXPECT_NE(xml.find("<state>0</state>"), std::string::npos);
  EXPECT_NE(xml.find("<output>host ok</output>"), std::string::npos);
}

TEST(NrdpRender, MultipleItemsAllRendered) {
  nrdp::data d;
  d.add_host("h1", 0, "host msg");
  d.add_service("h2", "svc", 2, "svc msg");
  const std::string xml = d.render_request();

  EXPECT_NE(xml.find("<hostname>h1</hostname>"), std::string::npos);
  EXPECT_NE(xml.find("<hostname>h2</hostname>"), std::string::npos);
  EXPECT_NE(xml.find("<servicename>svc</servicename>"), std::string::npos);
  EXPECT_NE(xml.find("<output>host msg</output>"), std::string::npos);
  EXPECT_NE(xml.find("<output>svc msg</output>"), std::string::npos);
}

TEST(NrdpRender, SpecialCharactersAreXmlEscaped) {
  nrdp::data d;
  d.add_service("host3", "svc&<>", 0, "<bad> & \"messy\" 'value'");
  const std::string xml = d.render_request();

  // The raw characters must not appear unescaped in the rendered output
  // (i.e. the < / > / & in our payload must have been encoded).
  EXPECT_EQ(xml.find("<bad>"), std::string::npos);
  EXPECT_EQ(xml.find("svc&<"), std::string::npos);
  // The encoded forms should be present.
  EXPECT_NE(xml.find("&lt;"), std::string::npos);
  EXPECT_NE(xml.find("&gt;"), std::string::npos);
  EXPECT_NE(xml.find("&amp;"), std::string::npos);
}

// ============================================================================
// nrdp::data::parse_response tests
// ============================================================================

TEST(NrdpParse, ValidSuccessResponse) {
  const std::string body =
      "<?xml version=\"1.0\"?>"
      "<result>"
      "<status>0</status>"
      "<message>OK</message>"
      "</result>";

  const auto ret = nrdp::data::parse_response(body);
  EXPECT_EQ(ret.get<0>(), 0);
  EXPECT_EQ(ret.get<1>(), "OK");
}

TEST(NrdpParse, ValidErrorResponse) {
  const std::string body =
      "<?xml version=\"1.0\"?>"
      "<result>"
      "<status>-1</status>"
      "<message>Bad token</message>"
      "</result>";

  const auto ret = nrdp::data::parse_response(body);
  EXPECT_EQ(ret.get<0>(), -1);
  EXPECT_EQ(ret.get<1>(), "Bad token");
}

TEST(NrdpParse, MissingResultElementFails) {
  const std::string body = "<?xml version=\"1.0\"?><other/>";
  const auto ret = nrdp::data::parse_response(body);
  EXPECT_EQ(ret.get<0>(), -1);
  EXPECT_EQ(ret.get<1>(), "Invalid response from server");
}

TEST(NrdpParse, MissingStatusElementFails) {
  const std::string body =
      "<?xml version=\"1.0\"?>"
      "<result><message>foo</message></result>";
  const auto ret = nrdp::data::parse_response(body);
  EXPECT_EQ(ret.get<0>(), -1);
  EXPECT_EQ(ret.get<1>(), "Invalid response from server");
}

TEST(NrdpParse, MissingMessageElementFails) {
  const std::string body =
      "<?xml version=\"1.0\"?>"
      "<result><status>0</status></result>";
  const auto ret = nrdp::data::parse_response(body);
  EXPECT_EQ(ret.get<0>(), -1);
  EXPECT_EQ(ret.get<1>(), "Invalid response from server");
}

TEST(NrdpParse, NonNumericStatusFallsBackToMinusOne) {
  const std::string body =
      "<?xml version=\"1.0\"?>"
      "<result>"
      "<status>not-a-number</status>"
      "<message>weird</message>"
      "</result>";
  const auto ret = nrdp::data::parse_response(body);
  EXPECT_EQ(ret.get<0>(), -1);
  EXPECT_EQ(ret.get<1>(), "weird");
}

// ============================================================================
// Round-trip render -> parse smoke test
// ============================================================================

TEST(NrdpRoundTrip, RenderedRequestParsesAsXml) {
  nrdp::data d;
  d.add_service("host", "svc", 0, "msg");
  const std::string xml = d.render_request();

  // The rendered request should at minimum start with an XML declaration
  // and contain a single <checkresults> root element.
  EXPECT_EQ(xml.compare(0, 5, "<?xml"), 0);
  EXPECT_NE(xml.find("<checkresults"), std::string::npos);
  EXPECT_NE(xml.find("</checkresults>"), std::string::npos);
}

