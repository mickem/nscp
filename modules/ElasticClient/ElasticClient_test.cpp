// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the ElasticClient bulk-payload helpers: the _bulk request body
// (per-document ids, optional legacy _type, %(date) index expansion) and the
// error extraction from Elasticsearch responses. The response bodies come
// from the network, so extract_errors is exercised against hostile and
// unexpected shapes as well as the documented ones.

#include "elastic_bulk.hpp"

#include <gtest/gtest.h>

#include <boost/date_time/gregorian/formatters.hpp>
#include <boost/json.hpp>

#include <set>
#include <string>
#include <vector>

namespace {

std::vector<std::string> split_lines(const std::string &payload) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start < payload.size()) {
    std::size_t end = payload.find('\n', start);
    if (end == std::string::npos) end = payload.size();
    lines.push_back(payload.substr(start, end - start));
    start = end + 1;
  }
  return lines;
}

}  // namespace

TEST(ElasticBulk, parse_index_expands_date) {
  const std::string today = boost::gregorian::to_iso_extended_string(boost::gregorian::day_clock::universal_day());
  EXPECT_EQ(elastic_bulk::parse_index("nsclient_log-%(date)"), "nsclient_log-" + today);
  EXPECT_EQ(elastic_bulk::parse_index("static_index"), "static_index");
}

TEST(ElasticBulk, build_payload_is_ndjson_with_one_action_per_document) {
  const std::vector<std::string> docs = {"{\"a\":1}", "{\"b\":2}"};
  const std::string payload = elastic_bulk::build_payload("idx", "", docs);

  // The body must end with a newline (the _bulk API requires it).
  ASSERT_FALSE(payload.empty());
  EXPECT_EQ(payload.back(), '\n');

  const std::vector<std::string> lines = split_lines(payload);
  ASSERT_EQ(lines.size(), 4u);
  const auto header0 = boost::json::parse(lines[0]).as_object();
  EXPECT_EQ(header0.at("index").as_object().at("_index").as_string(), "idx");
  EXPECT_EQ(lines[1], docs[0]);
  EXPECT_EQ(lines[3], docs[1]);
}

TEST(ElasticBulk, build_payload_gives_every_document_its_own_id) {
  // A shared _id makes documents in the same batch overwrite each other, so
  // all but one silently disappear.
  const std::vector<std::string> docs = {"{}", "{}", "{}"};
  const std::string payload = elastic_bulk::build_payload("idx", "", docs);
  const std::vector<std::string> lines = split_lines(payload);
  ASSERT_EQ(lines.size(), 6u);
  std::set<std::string> ids;
  for (std::size_t i = 0; i < lines.size(); i += 2) {
    const auto header = boost::json::parse(lines[i]).as_object();
    ids.insert(std::string(header.at("index").as_object().at("_id").as_string().c_str()));
  }
  EXPECT_EQ(ids.size(), 3u);
}

TEST(ElasticBulk, build_payload_omits_type_unless_configured) {
  // _type was removed in Elasticsearch 8; it must only appear when the
  // operator explicitly configures one (for 6.x and older).
  const std::vector<std::string> docs = {"{}"};

  const std::string without = elastic_bulk::build_payload("idx", "", docs);
  const auto header = boost::json::parse(split_lines(without)[0]).as_object();
  EXPECT_FALSE(header.at("index").as_object().contains("_type"));

  const std::string with = elastic_bulk::build_payload("idx", "legacy", docs);
  const auto legacy_header = boost::json::parse(split_lines(with)[0]).as_object();
  EXPECT_EQ(legacy_header.at("index").as_object().at("_type").as_string(), "legacy");
}

TEST(ElasticBulk, extract_errors_empty_on_success) {
  EXPECT_EQ(elastic_bulk::extract_errors("{\"took\":3,\"errors\":false,\"items\":[]}"), "");
  EXPECT_EQ(elastic_bulk::extract_errors("{\"took\":3}"), "");
}

TEST(ElasticBulk, extract_errors_collects_item_reasons) {
  const std::string body =
      "{\"errors\":true,\"items\":["
      "{\"index\":{\"error\":{\"type\":\"mapper_parsing_exception\",\"reason\":\"failed to parse\"}}},"
      "{\"index\":{\"status\":201}},"
      "{\"create\":{\"error\":{\"reason\":\"index is read-only\"}}}"
      "]}";
  EXPECT_EQ(elastic_bulk::extract_errors(body), "failed to parse, index is read-only");
}

TEST(ElasticBulk, extract_errors_reports_top_level_error) {
  EXPECT_EQ(elastic_bulk::extract_errors("{\"error\":{\"type\":\"security_exception\",\"reason\":\"missing credentials\"},\"status\":401}"),
            "missing credentials");
  // Historic servers report the error as a plain string.
  EXPECT_EQ(elastic_bulk::extract_errors("{\"error\":\"IndexMissingException\"}"), "IndexMissingException");
}

TEST(ElasticBulk, extract_errors_never_throws_on_hostile_shapes) {
  // Non-JSON and wrong-typed fields must produce a bounded message, not an
  // exception: the body is attacker-influenced on an unverified connection.
  EXPECT_NE(elastic_bulk::extract_errors("<html>502 Bad Gateway</html>"), "");
  EXPECT_NE(elastic_bulk::extract_errors(""), "");
  EXPECT_EQ(elastic_bulk::extract_errors("{\"errors\":\"yes\"}"), "");  // wrong type: not reported as a failure
  EXPECT_EQ(elastic_bulk::extract_errors("{\"errors\":true,\"items\":\"nope\"}"),
            "Bulk request reported errors but no reason was found in the response");
  EXPECT_EQ(elastic_bulk::extract_errors("{\"errors\":true,\"items\":[{\"index\":{\"error\":17}}]}"), "17");
  // A huge body is truncated, not echoed wholesale into the log.
  const std::string huge(100000, 'x');
  EXPECT_LE(elastic_bulk::extract_errors(huge).size(), 300u);
}

TEST(ElasticBulk, extract_errors_strips_control_characters) {
  // Everything returned here is written to the agent log, so a hostile server
  // must not be able to forge extra log lines with CR/LF - truncation alone
  // would leave them in place.
  const std::string body = "{\"error\":{\"reason\":\"boom\\r\\n2026-01-01 FATAL forged line\"}}";
  const std::string errors = elastic_bulk::extract_errors(body);
  EXPECT_EQ(errors.find('\n'), std::string::npos);
  EXPECT_EQ(errors.find('\r'), std::string::npos);
  EXPECT_NE(errors.find("boom"), std::string::npos);

  // The same holds for a body that is not JSON at all.
  const std::string raw = elastic_bulk::extract_errors("not json\r\nforged line");
  EXPECT_EQ(raw.find('\n'), std::string::npos);
  EXPECT_EQ(raw.find('\r'), std::string::npos);
}
