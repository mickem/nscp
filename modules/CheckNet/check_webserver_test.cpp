// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include "check_webserver_internal.hpp"

using namespace check_net::check_webserver_internal;

// ============================================================================
// Apache mod_status ?auto
// ============================================================================

namespace {
const char kApacheAuto[] =
    "localhost\n"
    "ServerVersion: Apache/2.4.58 (Unix)\n"
    "ServerMPM: event\n"
    "Server Built: 2024-01-01T00:00:00\n"
    "CurrentTime: Monday, 01-Jul-2024 12:00:00 UTC\n"
    "RestartTime: Monday, 01-Jul-2024 10:00:00 UTC\n"
    "ParentServerConfigGeneration: 1\n"
    "ParentServerMPMGeneration: 0\n"
    "ServerUptimeSeconds: 7254\n"
    "ServerUptime: 2 hours 54 seconds\n"
    "Load1: 0.15\n"
    "Total Accesses: 8341\n"
    "Total kBytes: 91674\n"
    "Uptime: 7254\n"
    "ReqPerSec: 1.14985\n"
    "BytesPerSec: 12940.7\n"
    "BytesPerReq: 11253.9\n"
    "BusyWorkers: 3\n"
    "IdleWorkers: 47\n"
    "Scoreboard: __W_K_____W.....................\n";
}

TEST(CheckWebserverApache, ParsesTheAutoFormat) {
  apache_status s;
  ASSERT_TRUE(parse_apache_auto(kApacheAuto, s));
  EXPECT_EQ(s.total_accesses, 8341);
  EXPECT_EQ(s.total_kbytes, 91674);
  EXPECT_EQ(s.uptime, 7254);
  EXPECT_NEAR(s.requests_per_sec, 1.14985, 0.0001);
  EXPECT_NEAR(s.bytes_per_sec, 12940.7, 0.1);
  EXPECT_EQ(s.busy_workers, 3);
  EXPECT_EQ(s.idle_workers, 47);
  EXPECT_EQ(s.scoreboard, "__W_K_____W.....................");
}

TEST(CheckWebserverApache, RejectsTheHtmlPage) {
  apache_status s;
  EXPECT_FALSE(parse_apache_auto("<html><head><title>Apache Status</title></head><body>...</body></html>", s));
}

TEST(CheckWebserverApache, RejectsAnEmptyBody) {
  apache_status s;
  EXPECT_FALSE(parse_apache_auto("", s));
}

// ============================================================================
// nginx stub_status
// ============================================================================

TEST(CheckWebserverNginx, ParsesStubStatus) {
  nginx_status s;
  ASSERT_TRUE(parse_nginx_stub(
      "Active connections: 291 \n"
      "server accepts handled requests\n"
      " 16630948 16630946 31070465 \n"
      "Reading: 6 Writing: 179 Waiting: 106 \n",
      s));
  EXPECT_EQ(s.active, 291);
  EXPECT_EQ(s.accepts, 16630948);
  EXPECT_EQ(s.handled, 16630946);
  EXPECT_EQ(s.requests, 31070465);
  EXPECT_EQ(s.reading, 6);
  EXPECT_EQ(s.writing, 179);
  EXPECT_EQ(s.waiting, 106);
}

TEST(CheckWebserverNginx, RejectsANonStatusBody) {
  nginx_status s;
  EXPECT_FALSE(parse_nginx_stub("<html>404 Not Found</html>", s));
}

TEST(CheckWebserverNginx, RejectsATruncatedBody) {
  nginx_status s;
  EXPECT_FALSE(parse_nginx_stub("Active connections: 291\nserver accepts handled requests\n", s));
}

// ============================================================================
// PHP-FPM status page
// ============================================================================

TEST(CheckWebserverPhpFpm, ParsesTheTextFormat) {
  phpfpm_status s;
  ASSERT_TRUE(parse_phpfpm_status(
      "pool:                 www\n"
      "process manager:      dynamic\n"
      "start time:           01/Jul/2024:10:00:04 +0200\n"
      "start since:          7254\n"
      "accepted conn:        4211\n"
      "listen queue:         2\n"
      "max listen queue:     11\n"
      "listen queue len:     511\n"
      "idle processes:       7\n"
      "active processes:     3\n"
      "total processes:      10\n"
      "max active processes: 9\n"
      "max children reached: 1\n"
      "slow requests:        5\n",
      s));
  EXPECT_EQ(s.pool, "www");
  EXPECT_EQ(s.process_manager, "dynamic");
  EXPECT_EQ(s.accepted_conn, 4211);
  EXPECT_EQ(s.listen_queue, 2);
  EXPECT_EQ(s.max_listen_queue, 11);
  EXPECT_EQ(s.listen_queue_len, 511);
  EXPECT_EQ(s.idle_processes, 7);
  EXPECT_EQ(s.active_processes, 3);
  EXPECT_EQ(s.total_processes, 10);
  EXPECT_EQ(s.max_active_processes, 9);
  EXPECT_EQ(s.max_children_reached, 1);
  EXPECT_EQ(s.slow_requests, 5);
}

TEST(CheckWebserverPhpFpm, RejectsANonStatusBody) {
  phpfpm_status s;
  EXPECT_FALSE(parse_phpfpm_status("File not found.\n", s));
}

// ============================================================================
// Tomcat manager XML status
// ============================================================================

namespace {
const char kTomcatXml[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><?xml-stylesheet type=\"text/xsl\" href=\"/manager/xform.xsl\" ?>"
    "<status><jvm><memory free='1734127416' total='2147483648' max='4294967296'/>"
    "<memorypool name='PS Eden Space' type='Heap memory' usageInit='268435456' usageCommitted='268435456' usageMax='268435456' usageUsed='1048576'/>"
    "</jvm>"
    "<connector name='\"http-nio-8080\"'><threadInfo maxThreads=\"200\" currentThreadCount=\"25\" currentThreadsBusy=\"4\"/>"
    "<requestInfo maxTime=\"1230\" processingTime=\"55211\" requestCount=\"104211\" errorCount=\"17\" bytesReceived=\"0\" bytesSent=\"1048576000\"/>"
    "<workers></workers></connector>"
    "<connector name='\"ajp-nio-8009\"'><threadInfo maxThreads=\"100\" currentThreadCount=\"0\" currentThreadsBusy=\"0\"/>"
    "<requestInfo maxTime=\"0\" processingTime=\"0\" requestCount=\"0\" errorCount=\"0\" bytesReceived=\"0\" bytesSent=\"0\"/>"
    "<workers></workers></connector></status>";
}

TEST(CheckWebserverTomcat, ParsesConnectorsAndMemory) {
  tomcat_status s;
  ASSERT_TRUE(parse_tomcat_status_xml(kTomcatXml, s));
  EXPECT_EQ(s.memory_free, 1734127416);
  EXPECT_EQ(s.memory_total, 2147483648LL);
  EXPECT_EQ(s.memory_max, 4294967296LL);
  ASSERT_EQ(s.connectors.size(), 2u);
  // The name attribute value itself is quoted by Tomcat; the quotes are stripped.
  EXPECT_EQ(s.connectors[0].name, "http-nio-8080");
  EXPECT_EQ(s.connectors[0].threads_max, 200);
  EXPECT_EQ(s.connectors[0].threads_current, 25);
  EXPECT_EQ(s.connectors[0].threads_busy, 4);
  EXPECT_EQ(s.connectors[0].max_time, 1230);
  EXPECT_EQ(s.connectors[0].processing_time, 55211);
  EXPECT_EQ(s.connectors[0].request_count, 104211);
  EXPECT_EQ(s.connectors[0].error_count, 17);
  EXPECT_EQ(s.connectors[0].bytes_sent, 1048576000);
  EXPECT_EQ(s.connectors[1].name, "ajp-nio-8009");
}

TEST(CheckWebserverTomcat, RejectsNonXml) {
  tomcat_status s;
  EXPECT_FALSE(parse_tomcat_status_xml("<html>login required</html>", s));
}

TEST(CheckWebserverTomcat, RejectsXmlWithoutAStatusRoot) {
  tomcat_status s;
  EXPECT_FALSE(parse_tomcat_status_xml("<?xml version=\"1.0\"?><other/>", s));
}

// ============================================================================
// ensure_query_param
// ============================================================================

TEST(CheckWebserverUrl, AppendsTheParamWhenMissing) {
  EXPECT_EQ(ensure_query_param("http://h/server-status", "auto"), "http://h/server-status?auto");
  EXPECT_EQ(ensure_query_param("http://h/status?full", "json"), "http://h/status?full&json");
  EXPECT_EQ(ensure_query_param("http://h:8080/manager/status", "XML=true"), "http://h:8080/manager/status?XML=true");
}

TEST(CheckWebserverUrl, LeavesAnExplicitParamAlone) {
  EXPECT_EQ(ensure_query_param("http://h/server-status?auto", "auto"), "http://h/server-status?auto");
  EXPECT_EQ(ensure_query_param("http://h/s?refresh=5&auto", "auto"), "http://h/s?refresh=5&auto");
  // Same parameter name with a different value is respected as-is.
  EXPECT_EQ(ensure_query_param("http://h/manager/status?XML=false", "XML=true"), "http://h/manager/status?XML=false");
}
