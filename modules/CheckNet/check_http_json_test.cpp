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

#include <gtest/gtest.h>

#include "check_http_json.hpp"

using check_net::check_http_json::extract;
using check_net::check_http_json::extraction;
using check_net::check_http_json::split_path;

namespace {

extraction run(const std::string &body, std::vector<std::pair<std::string, std::string>> paths, bool expect_ok = true) {
  extraction out;
  EXPECT_EQ(extract(body, paths, out), expect_ok);
  return out;
}

TEST(CheckHttpJson, SplitsDottedPaths) {
  EXPECT_EQ(split_path("a.b.c"), (std::vector<std::string>{"a", "b", "c"}));
  EXPECT_EQ(split_path("data.items.0.value"), (std::vector<std::string>{"data", "items", "0", "value"}));
}

TEST(CheckHttpJson, SplitHonoursQuotedSegments) {
  EXPECT_EQ(split_path("'a.b'.c"), (std::vector<std::string>{"a.b", "c"}));
  EXPECT_EQ(split_path("obj.'key.with.dots'"), (std::vector<std::string>{"obj", "key.with.dots"}));
}

TEST(CheckHttpJson, ExtractsNumbersAndStrings) {
  const auto out = run(R"({"status":"ok","queue":{"length":42},"ratio":0.25})",
                       {{"st", "status"}, {"qlen", "queue.length"}, {"ratio", "ratio"}});
  EXPECT_EQ(out.strings.at("st"), "ok");
  EXPECT_DOUBLE_EQ(out.numbers.at("qlen"), 42.0);
  EXPECT_EQ(out.strings.at("qlen"), "42");
  EXPECT_DOUBLE_EQ(out.numbers.at("ratio"), 0.25);  // float precision preserved
}

TEST(CheckHttpJson, IndexesIntoArrays) {
  const auto out = run(R"({"items":[{"name":"a"},{"name":"b"}]})", {{"second", "items.1.name"}});
  EXPECT_EQ(out.strings.at("second"), "b");
}

TEST(CheckHttpJson, ExtractsBooleans) {
  const auto out = run(R"({"healthy":true,"degraded":false})", {{"h", "healthy"}, {"d", "degraded"}});
  EXPECT_DOUBLE_EQ(out.numbers.at("h"), 1.0);
  EXPECT_DOUBLE_EQ(out.numbers.at("d"), 0.0);
  EXPECT_EQ(out.strings.at("h"), "true");
}

TEST(CheckHttpJson, KeysWithDotsViaQuoting) {
  const auto out = run(R"({"a.b":{"c":7}})", {{"v", "'a.b'.c"}});
  EXPECT_DOUBLE_EQ(out.numbers.at("v"), 7.0);
}

TEST(CheckHttpJson, MissingPathIsAbsentNotAnError) {
  const auto out = run(R"({"a":1})", {{"missing", "b.c.d"}, {"present", "a"}});
  EXPECT_EQ(out.numbers.count("missing"), 0u);
  EXPECT_EQ(out.strings.count("missing"), 0u);
  EXPECT_DOUBLE_EQ(out.numbers.at("present"), 1.0);
}

TEST(CheckHttpJson, InvalidJsonReturnsFalse) {
  extraction out;
  EXPECT_FALSE(extract("this is not json", {{"a", "a"}}, out));
  EXPECT_TRUE(out.numbers.empty());
}

TEST(CheckHttpJson, ArrayOrObjectExposedAsSerializedString) {
  const auto out = run(R"({"list":[1,2,3]})", {{"l", "list"}});
  EXPECT_EQ(out.strings.at("l"), "[1,2,3]");
}

}  // namespace
