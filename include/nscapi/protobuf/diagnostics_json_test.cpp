// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/json.hpp>
#include <limits>
#include <nscapi/nscapi_diagnostics_helper.hpp>
#include <nscapi/protobuf/diagnostics_json.hpp>

using nscapi::diagnostics::table;
using nscapi::diagnostics::type_float;
using nscapi::diagnostics::type_int;
using nscapi::diagnostics::type_timestamp;

namespace {
PB::Diagnostics::DiagnosticsMessage sample() {
  PB::Diagnostics::DiagnosticsMessage message;
  {
    auto t = table(&message, "processes.top_cpu").describe("Top CPU").captured_at(1763203327);
    t.column("name").column("pid", type_int).column("cpu", type_float, "percent", "Share of all cores").column("started", type_timestamp);
    t.row().add("acme-indexer").add(4242).add(87.5).add_time(1763203312);
    t.row().add("idle").add(1).add(std::numeric_limits<double>::quiet_NaN()).add_time(0);
  }
  message.mutable_sections(0)->set_source("CheckSystem");
  {
    auto t = table(&message, "eventlog.errors").captured_at(1763203300).error("subscription failed");
    t.column("message");
  }
  message.mutable_sections(1)->set_source("CheckEventLog");
  return message;
}
}  // namespace

TEST(diagnostics_json, renders_a_section_as_columns_and_rows_of_objects) {
  const boost::json::object all = diagnostics_json::sections(sample());
  ASSERT_TRUE(all.contains("processes.top_cpu"));
  const boost::json::object &s = all.at("processes.top_cpu").as_object();
  EXPECT_EQ(s.at("source").as_string(), "CheckSystem");
  EXPECT_EQ(s.at("description").as_string(), "Top CPU");
  EXPECT_EQ(s.at("captured_at").as_string(), "2025-11-15T10:42:07Z");
  EXPECT_FALSE(s.at("truncated").as_bool());
  EXPECT_FALSE(s.contains("error"));

  const boost::json::array &columns = s.at("columns").as_array();
  ASSERT_EQ(columns.size(), 4u);
  EXPECT_EQ(columns[0].as_object().at("name").as_string(), "name");
  EXPECT_EQ(columns[0].as_object().at("type").as_string(), "string");
  EXPECT_FALSE(columns[0].as_object().contains("unit"));
  EXPECT_EQ(columns[2].as_object().at("type").as_string(), "float");
  EXPECT_EQ(columns[2].as_object().at("unit").as_string(), "percent");
  EXPECT_EQ(columns[2].as_object().at("description").as_string(), "Share of all cores");
  EXPECT_EQ(columns[3].as_object().at("type").as_string(), "timestamp");

  const boost::json::array &rows = s.at("rows").as_array();
  ASSERT_EQ(rows.size(), 2u);
  const boost::json::object &first = rows[0].as_object();
  EXPECT_EQ(first.at("name").as_string(), "acme-indexer");
  EXPECT_EQ(first.at("pid").as_int64(), 4242);
  EXPECT_DOUBLE_EQ(first.at("cpu").as_double(), 87.5);
  EXPECT_EQ(first.at("started").as_string(), "2025-11-15T10:41:52Z");
  // NaN and an unset time have no JSON spelling: null.
  const boost::json::object &second = rows[1].as_object();
  EXPECT_TRUE(second.at("cpu").is_null());
  EXPECT_TRUE(second.at("started").is_null());
}

TEST(diagnostics_json, keeps_a_failed_section_with_its_error) {
  const boost::json::object all = diagnostics_json::sections(sample());
  const boost::json::object &s = all.at("eventlog.errors").as_object();
  EXPECT_EQ(s.at("error").as_string(), "subscription failed");
  EXPECT_TRUE(s.at("rows").as_array().empty());
  EXPECT_FALSE(s.contains("description"));
}

TEST(diagnostics_json, renders_a_missing_or_short_row_cell_as_null) {
  PB::Diagnostics::DiagnosticsMessage message;
  PB::Diagnostics::Section *s = message.add_sections();
  s->set_name("x");
  s->add_columns()->set_name("a");
  s->add_columns()->set_name("b");
  // A ragged row from a producer that did not use the builder.
  s->add_rows()->add_cells()->set_s("only");
  const boost::json::object row = diagnostics_json::section(*s).at("rows").as_array()[0].as_object();
  EXPECT_EQ(row.at("a").as_string(), "only");
  EXPECT_TRUE(row.at("b").is_null());
}

TEST(diagnostics_json, a_summary_has_no_columns_or_rows) {
  const boost::json::object summary = diagnostics_json::summary(sample().sections(0));
  EXPECT_FALSE(summary.contains("rows"));
  EXPECT_FALSE(summary.contains("columns"));
  EXPECT_EQ(summary.at("source").as_string(), "CheckSystem");
}
