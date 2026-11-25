// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/json.hpp>

#include <climits>

#include "check_jvm_internal.hpp"

using namespace check_net::check_jvm_internal;
namespace json = boost::json;

namespace {

// A Jolokia 2.x bulk answer to build_bulk_request({}) from a Java 21 JVM
// running G1, trimmed to the fields Jolokia actually returns.
const char kBulk[] = R"([
  {"request":{"mbean":"java.lang:type=Memory","attribute":["HeapMemoryUsage","NonHeapMemoryUsage"],"type":"read"},
   "value":{"HeapMemoryUsage":{"init":264241152,"committed":268435456,"max":4194304000,"used":104857600},
            "NonHeapMemoryUsage":{"init":7667712,"committed":62914560,"max":-1,"used":58720256}},
   "status":200,"timestamp":1732450000},
  {"request":{"mbean":"java.lang:type=Threading","attribute":["ThreadCount","DaemonThreadCount","PeakThreadCount"],"type":"read"},
   "value":{"ThreadCount":42,"DaemonThreadCount":30,"PeakThreadCount":45},"status":200,"timestamp":1732450000},
  {"request":{"mbean":"java.lang:type=Runtime","attribute":["Uptime","VmName","VmVendor","VmVersion","SpecVersion"],"type":"read"},
   "value":{"Uptime":3723000,"VmName":"OpenJDK 64-Bit Server VM","VmVendor":"Eclipse Adoptium","VmVersion":"21.0.5+11-LTS","SpecVersion":"21"},
   "status":200,"timestamp":1732450000},
  {"request":{"mbean":"java.lang:type=ClassLoading","attribute":["LoadedClassCount"],"type":"read"},
   "value":{"LoadedClassCount":8123},"status":200,"timestamp":1732450000},
  {"request":{"mbean":"java.lang:name=*,type=GarbageCollector","attribute":["CollectionCount","CollectionTime"],"type":"read"},
   "value":{"java.lang:name=G1 Young Generation,type=GarbageCollector":{"CollectionCount":17,"CollectionTime":120},
            "java.lang:name=G1 Concurrent GC,type=GarbageCollector":{"CollectionCount":4,"CollectionTime":9},
            "java.lang:name=G1 Old Generation,type=GarbageCollector":{"CollectionCount":0,"CollectionTime":0}},
   "status":200,"timestamp":1732450000}
])";

std::string with_custom(const std::string &extra) {
  std::string body = kBulk;
  const auto end = body.rfind(']');
  return body.substr(0, end) + "," + extra + "]";
}

}  // namespace

TEST(CheckJvmParse, ReadsThePlatformMBeans) {
  jvm_status s;
  ASSERT_TRUE(parse_bulk_response(kBulk, {}, s));
  EXPECT_EQ(s.error, "");
  EXPECT_EQ(s.heap_used, 104857600);
  EXPECT_EQ(s.heap_committed, 268435456);
  EXPECT_EQ(s.heap_max, 4194304000LL);
  EXPECT_EQ(s.nonheap_used, 58720256);
  EXPECT_EQ(s.nonheap_committed, 62914560);
  EXPECT_EQ(s.threads, 42);
  EXPECT_EQ(s.daemon_threads, 30);
  EXPECT_EQ(s.peak_threads, 45);
  EXPECT_EQ(s.loaded_classes, 8123);
  EXPECT_EQ(s.uptime_ms, 3723000);
  EXPECT_EQ(s.vm_name, "OpenJDK 64-Bit Server VM");
  EXPECT_EQ(s.vm_vendor, "Eclipse Adoptium");
  EXPECT_EQ(s.vm_version, "21.0.5+11-LTS");
  EXPECT_EQ(s.spec_version, "21");
  ASSERT_EQ(s.collectors.size(), 3u);
  EXPECT_EQ(s.gc_count(), 21);
  EXPECT_EQ(s.gc_time(), 129);
  EXPECT_NE(s.collector_names().find("G1 Young Generation"), std::string::npos);
  EXPECT_NEAR(s.heap_usage(), 2.5, 0.001);
  EXPECT_TRUE(s.memory_read);
  EXPECT_TRUE(s.threading_read);
  EXPECT_TRUE(s.runtime_read);
  EXPECT_TRUE(s.classes_read);
  EXPECT_TRUE(s.gc_read);
}

TEST(CheckJvmParse, NothingCountsAsReadBeforeAResponse) {
  // The state the check is left in when the agent is down or the body is not
  // Jolokia: every platform keyword must report no value, not 0.
  const jvm_status s;
  EXPECT_FALSE(s.memory_read || s.threading_read || s.runtime_read || s.classes_read || s.gc_read);
}

TEST(CheckJvmParse, HeapUsageFallsBackToCommittedWithoutAMaximum) {
  jvm_status s;
  s.heap_used = 50;
  s.heap_committed = 200;
  s.heap_max = -1;
  EXPECT_DOUBLE_EQ(s.heap_usage(), 25.0);
  s.heap_committed = 0;
  EXPECT_DOUBLE_EQ(s.heap_usage(), 0.0);
}

TEST(CheckJvmParse, CollectorsThatCannotCountAreNotSubtracted) {
  // CollectionCount/CollectionTime are -1 for a collector that does not
  // track them; summing them raw would make gc_count go down.
  jvm_status s;
  s.collectors.push_back({"a", 10, 100});
  s.collectors.push_back({"b", -1, -1});
  EXPECT_EQ(s.gc_count(), 10);
  EXPECT_EQ(s.gc_time(), 100);
}

TEST(CheckJvmParse, RejectsWhatIsNotABulkResponse) {
  jvm_status s;
  EXPECT_FALSE(parse_bulk_response("<html>Not found</html>", {}, s));
  EXPECT_FALSE(parse_bulk_response("{\"value\":1,\"status\":200}", {}, s));
  // An array of the wrong length is an answer to some other request.
  EXPECT_FALSE(parse_bulk_response("[{\"status\":200,\"value\":{}}]", {}, s));
  EXPECT_FALSE(parse_bulk_response("[1,2,3,4,5]", {}, s));
}

TEST(CheckJvmParse, AFailedReadIsReportedWithTheOthersIntact) {
  std::string body = kBulk;
  const std::string threading_ok = R"("value":{"ThreadCount":42,"DaemonThreadCount":30,"PeakThreadCount":45},"status":200)";
  const auto pos = body.find(threading_ok);
  ASSERT_NE(pos, std::string::npos);
  body.replace(pos, threading_ok.size(), R"("error_type":"java.lang.SecurityException","error":"java.lang.SecurityException : Reading attribute ThreadCount is forbidden","status":403)");
  jvm_status s;
  ASSERT_TRUE(parse_bulk_response(body, {}, s));
  EXPECT_EQ(s.error, "java.lang:type=Threading: java.lang.SecurityException : Reading attribute ThreadCount is forbidden");
  EXPECT_FALSE(s.threading_read);
  EXPECT_EQ(s.heap_used, 104857600);
  EXPECT_TRUE(s.memory_read);
  EXPECT_TRUE(s.gc_read);
}

TEST(CheckJvmParse, ReadsCustomAttributes) {
  std::vector<jvm_read> reads(4);
  reads[0].alias = "sessions";
  reads[1].alias = "state";
  reads[2].alias = "usage";
  reads[3].alias = "missing";
  const std::string body = with_custom(
      R"({"request":{},"value":118,"status":200},)"
      R"({"request":{},"value":"STARTED","status":200},)"
      R"({"request":{},"value":{"used":5,"max":10},"status":200},)"
      R"({"request":{"mbean":"Catalina:type=Manager,context=/nope,host=localhost"},"error_type":"javax.management.InstanceNotFoundException","error":"javax.management.InstanceNotFoundException : Catalina:type=Manager,context=/nope,host=localhost","status":404})");
  jvm_status s;
  ASSERT_TRUE(parse_bulk_response(body, reads, s));
  ASSERT_EQ(s.custom.count("sessions"), 1u);
  EXPECT_TRUE(s.custom["sessions"].has_number);
  EXPECT_DOUBLE_EQ(s.custom["sessions"].number, 118.0);
  EXPECT_EQ(s.custom["sessions"].text, "118");
  EXPECT_FALSE(s.custom["state"].has_number);
  EXPECT_EQ(s.custom["state"].text, "STARTED");
  EXPECT_FALSE(s.custom["usage"].has_number);
  EXPECT_EQ(s.custom["usage"].text, R"({"used":5,"max":10})");
  EXPECT_EQ(s.custom.count("missing"), 0u);
  EXPECT_EQ(s.error.rfind("missing: Catalina:type=Manager,context=/nope,host=localhost: javax.management.InstanceNotFoundException", 0), 0u) << s.error;
}

TEST(CheckJvmParse, OutOfRangeValuesSaturateInsteadOfOverflowing) {
  // The agent is remote: a huge number, as JSON or as a numeric string, must
  // clamp rather than be cast to long long (undefined behaviour).
  std::string body = kBulk;
  const std::string threading_ok = R"("ThreadCount":42,"DaemonThreadCount":30,"PeakThreadCount":45)";
  const auto pos = body.find(threading_ok);
  ASSERT_NE(pos, std::string::npos);
  body.replace(pos, threading_ok.size(), R"("ThreadCount":"1e300","DaemonThreadCount":1e300,"PeakThreadCount":"-1e300")");
  jvm_status s;
  ASSERT_TRUE(parse_bulk_response(body, {}, s));
  EXPECT_EQ(s.threads, LLONG_MAX);
  EXPECT_EQ(s.daemon_threads, LLONG_MAX);
  // A negative value is the JMX "unknown" for a count: it reads as 0.
  EXPECT_EQ(s.peak_threads, 0);
}

TEST(CheckJvmParse, NumericStringsAndBooleansAreNumbers) {
  std::vector<jvm_read> reads(2);
  reads[0].alias = "rate";
  reads[1].alias = "flag";
  jvm_status s;
  ASSERT_TRUE(parse_bulk_response(with_custom(R"({"value":"12.5","status":200},{"value":true,"status":200})"), reads, s));
  EXPECT_TRUE(s.custom["rate"].has_number);
  EXPECT_DOUBLE_EQ(s.custom["rate"].number, 12.5);
  EXPECT_TRUE(s.custom["flag"].has_number);
  EXPECT_DOUBLE_EQ(s.custom["flag"].number, 1.0);
  EXPECT_EQ(s.custom["flag"].text, "true");
}

TEST(CheckJvmParse, LargeFractionalValuesRenderWithoutScientificNotation) {
  std::vector<jvm_read> reads(2);
  reads[0].alias = "bytes";
  reads[1].alias = "ratio";
  jvm_status s;
  ASSERT_TRUE(parse_bulk_response(with_custom(R"({"value":1234567.89,"status":200},{"value":0.73,"status":200})"), reads, s));
  EXPECT_EQ(s.custom["bytes"].text, "1234567.89");
  EXPECT_DOUBLE_EQ(s.custom["bytes"].number, 1234567.89);
  EXPECT_EQ(s.custom["ratio"].text, "0.73");
}

TEST(CheckJvmParse, NonFiniteValuesAreNotNumbers) {
  std::vector<jvm_read> reads(2);
  reads[0].alias = "big";
  reads[1].alias = "huge";
  jvm_status s;
  ASSERT_TRUE(parse_bulk_response(with_custom(R"({"value":"1e400","status":200},{"value":1e400,"status":200})"), reads, s));
  EXPECT_FALSE(s.custom["big"].has_number);
  EXPECT_EQ(s.custom["big"].text, "1e400");
  EXPECT_FALSE(s.custom["huge"].has_number);
}

TEST(CheckJvmParse, TopLevelErrorIsRecognised) {
  EXPECT_EQ(top_level_error(R"({"error_type":"java.lang.Exception","error":"java.lang.Exception : Origin not allowed","status":403})"),
            "java.lang.Exception : Origin not allowed");
  EXPECT_EQ(top_level_error(kBulk), "");
  EXPECT_EQ(top_level_error("not json"), "");
}

TEST(CheckJvmReadSpec, SplitsAliasMBeanAttributeAndPath) {
  jvm_read r;
  std::string error;
  ASSERT_TRUE(parse_read_spec("old_gen=java.lang:type=MemoryPool,name=G1 Old Gen/Usage/used", r, error)) << error;
  EXPECT_EQ(r.alias, "old_gen");
  EXPECT_EQ(r.mbean, "java.lang:type=MemoryPool,name=G1 Old Gen");
  EXPECT_EQ(r.attribute, "Usage");
  EXPECT_EQ(r.path, "used");
}

TEST(CheckJvmReadSpec, UnescapesSlashesInTheMBeanName) {
  jvm_read r;
  std::string error;
  ASSERT_TRUE(parse_read_spec("sessions=Catalina:type=Manager,host=localhost,context=!/app/activeSessions", r, error)) << error;
  EXPECT_EQ(r.mbean, "Catalina:type=Manager,host=localhost,context=/app");
  EXPECT_EQ(r.attribute, "activeSessions");
  EXPECT_EQ(r.path, "");
}

TEST(CheckJvmReadSpec, KeepsEscapesInTheInnerPath) {
  // The inner path is handed to Jolokia as-is, so its escapes must survive.
  jvm_read r;
  std::string error;
  ASSERT_TRUE(parse_read_spec("x=d:k=v/Attr/a!/b/c", r, error)) << error;
  EXPECT_EQ(r.path, "a!/b/c");
}

TEST(CheckJvmReadSpec, RejectsMalformedSpecs) {
  jvm_read r;
  std::string error;
  EXPECT_FALSE(parse_read_spec("java.lang:type=Memory/HeapMemoryUsage", r, error));  // alias = "java.lang:type"
  EXPECT_FALSE(parse_read_spec("noequals", r, error));
  EXPECT_FALSE(parse_read_spec("1x=d:k=v/Attr", r, error));
  EXPECT_FALSE(parse_read_spec("x=d:k=v", r, error));
  EXPECT_FALSE(parse_read_spec("x=d:k=v/", r, error));
  EXPECT_FALSE(parse_read_spec("x=nodomain/Attr", r, error));
  // The filter grammar's variable names start with a letter.
  EXPECT_FALSE(parse_read_spec("_x=d:k=v/Attr", r, error));
}

TEST(CheckJvmReadSpec, RejectsAliasesSpelledLikeFilterWords) {
  // These parse as operators, so a keyword by that name could never be used;
  // say so up front instead of failing later with an anonymous parse error.
  jvm_read r;
  std::string error;
  for (const char *word : {"in", "and", "OR", "Not", "like", "regexp", "eq", "ne", "lt", "le", "gt", "ge", "str"}) {
    EXPECT_FALSE(parse_read_spec(std::string(word) + "=d:k=v/Attr", r, error)) << word;
    EXPECT_NE(error.find("word of the filter language"), std::string::npos) << error;
  }
  // `not` is matched without a word boundary: `notice > 1` would parse as
  // `not (ice > 1)`, so an alias starting with it is refused too - unless an
  // underscore follows, which cannot start a name (`not_after` parses).
  EXPECT_FALSE(parse_read_spec("notice=d:k=v/Attr", r, error));
  EXPECT_FALSE(parse_read_spec("Nothing=d:k=v/Attr", r, error));
  EXPECT_TRUE(parse_read_spec("not_after=d:k=v/Attr", r, error)) << error;
  // Other words only matter on their own.
  EXPECT_TRUE(parse_read_spec("index=d:k=v/Attr", r, error)) << error;
  EXPECT_TRUE(parse_read_spec("order=d:k=v/Attr", r, error)) << error;
  EXPECT_TRUE(parse_read_spec("android=d:k=v/Attr", r, error)) << error;
}

TEST(CheckJvmRequest, BuildsTheBulkRead) {
  jvm_read custom;
  custom.alias = "old_gen";
  custom.mbean = "java.lang:type=MemoryPool,name=G1 Old Gen";
  custom.attribute = "Usage";
  custom.path = "used";
  const json::value v = json::parse(build_bulk_request({custom}));
  ASSERT_TRUE(v.is_array());
  const json::array &a = v.as_array();
  ASSERT_EQ(a.size(), fixed_read_count + 1);
  EXPECT_EQ(a[0].at("mbean").as_string(), "java.lang:type=Memory");
  EXPECT_EQ(a[4].at("mbean").as_string(), "java.lang:type=GarbageCollector,name=*");
  for (const json::value &e : a) EXPECT_EQ(e.at("type").as_string(), "read");
  EXPECT_EQ(a[5].at("mbean").as_string(), "java.lang:type=MemoryPool,name=G1 Old Gen");
  EXPECT_EQ(a[5].at("attribute").as_string(), "Usage");
  EXPECT_EQ(a[5].at("path").as_string(), "used");
  EXPECT_EQ(a[0].as_object().if_contains("path"), nullptr);
}
