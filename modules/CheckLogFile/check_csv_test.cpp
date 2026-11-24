// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_csv.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <clocale>
#include <fstream>
#include <map>
#include <nscapi/nscapi_helper_singleton.hpp>

#ifdef WIN32
#include <windows.h>

#include <str/utf8.hpp>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

// Normally provided by NSC_WRAP_DLL(); a unit test has no generated module glue.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

using namespace check_csv_command;

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

std::string perf_of(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    for (int j = 0; j < r.lines(i).perf_size(); ++j) {
      if (!out.empty()) out += " ";
      out += r.lines(i).perf(j).alias();
    }
  }
  return out;
}

// A reader serving canned files, recording what it was asked for.
struct fake_source {
  std::map<std::string, std::string> files;
  std::string last_source;
  std::uint64_t last_max_bytes = 0;
  read_function fn() {
    return [this](const std::string &source, const std::uint64_t max_bytes, std::string &bytes, std::string &error) {
      last_source = source;
      last_max_bytes = max_bytes;
      const auto it = files.find(source);
      if (it == files.end()) {
        error = "Failed to open " + source + ": No such file or directory";
        return false;
      }
      bytes = it->second;
      return true;
    };
  }
};

struct runner {
  fake_source source;
  check::access::path_policy access{"file", "files", "/settings/logfile"};
  PB::Commands::QueryResponseMessage::Response response;

  PB::Common::ResultCode run(const std::vector<std::string> &args) {
    response.Clear();
    PB::Commands::QueryRequestMessage::Request request;
    request.set_command("check_csv");
    for (const std::string &a : args) request.add_arguments(a);
    check_with(request, &response, access, source.fn());
    return response.result();
  }
  std::string message() const { return join_lines(response); }
};

const char *kUpdates =
    "Hostname,WaitingUpdates,Load\r\n"
    "srv1,0,0.25\r\n"
    "srv2,7,1.5\r\n"
    "srv3,12,3.75\r\n";

}  // namespace

// --- columns ---------------------------------------------------------------

TEST(CheckCsvColumns, ParsesSqlStyleDeclaration) {
  std::vector<column_spec> cols;
  std::string error;
  ASSERT_TRUE(parse_columns("Hostname TEXT, WaitingUpdates INTEGER, Load real, Note", cols, error)) << error;
  ASSERT_EQ(4u, cols.size());
  EXPECT_EQ("Hostname", cols[0].name);
  EXPECT_EQ(column_type::text, cols[0].type);
  EXPECT_EQ(column_type::integer, cols[1].type);
  EXPECT_EQ(column_type::real, cols[2].type);
  EXPECT_EQ(column_type::text, cols[3].type);  // no type -> TEXT
}

TEST(CheckCsvColumns, AcceptsSizedTypes) {
  std::vector<column_spec> cols;
  std::string error;
  ASSERT_TRUE(parse_columns("Name VARCHAR(64), Price DECIMAL(10,2), Qty BIGINT", cols, error)) << error;
  ASSERT_EQ(3u, cols.size());
  EXPECT_EQ(column_type::text, cols[0].type);
  EXPECT_EQ(column_type::real, cols[1].type);
  EXPECT_EQ(column_type::integer, cols[2].type);
}

TEST(CheckCsvColumns, RefusesReservedAndInvalidNames) {
  std::vector<column_spec> cols;
  std::string error;
  EXPECT_FALSE(parse_columns("count INTEGER", cols, error));
  EXPECT_NE(std::string::npos, error.find("reserved")) << error;
  EXPECT_FALSE(parse_columns("row INTEGER", cols, error));
  EXPECT_FALSE(parse_columns("AND TEXT", cols, error));  // grammar words in any case
  // Variables are case-sensitive, so a capitalised summary name is a column.
  EXPECT_TRUE(parse_columns("Count INTEGER, Status TEXT", cols, error)) << error;
  EXPECT_FALSE(parse_columns("1st TEXT", cols, error));
  EXPECT_FALSE(parse_columns("_x TEXT", cols, error));
  EXPECT_FALSE(parse_columns("a-b TEXT", cols, error));
  EXPECT_FALSE(parse_columns("a TEXT, a INTEGER", cols, error));
  EXPECT_NE(std::string::npos, error.find("twice")) << error;
  EXPECT_FALSE(parse_columns("a BLOB", cols, error));
  EXPECT_NE(std::string::npos, error.find("Invalid type")) << error;
  EXPECT_FALSE(parse_columns("a TEXT,,b TEXT", cols, error));
}

TEST(CheckCsvColumns, FromHeader) {
  std::vector<column_spec> cols;
  std::string error;
  ASSERT_TRUE(columns_from_header({" Hostname", "WaitingUpdates "}, cols, error)) << error;
  ASSERT_EQ(2u, cols.size());
  EXPECT_EQ("Hostname", cols[0].name);
  EXPECT_EQ("WaitingUpdates", cols[1].name);
  EXPECT_FALSE(columns_from_header({"Host name"}, cols, error));
  EXPECT_NE(std::string::npos, error.find("columns=")) << error;
}

// --- decoding --------------------------------------------------------------

TEST(CheckCsvDecode, Utf8BomIsDropped) {
  std::string out, error;
  ASSERT_TRUE(
      decode_text("\xEF\xBB\xBF"
                  "a,b",
                  "auto", out, error));
  EXPECT_EQ("a,b", out);
  ASSERT_TRUE(
      decode_text("\xEF\xBB\xBF"
                  "a,b",
                  "utf-8", out, error));
  EXPECT_EQ("a,b", out);
}

TEST(CheckCsvDecode, Utf16LittleEndianWithBomIsDetected) {
  // "Å,1\r\n" as Windows writes it (Export-Csv -Encoding Unicode).
  const std::string bytes(
      "\xFF\xFE\xC5\x00,\x00"
      "1\x00\r\x00\n\x00",
      12);
  std::string out, error;
  ASSERT_TRUE(decode_text(bytes, "auto", out, error)) << error;
  EXPECT_EQ("\xC3\x85,1\r\n", out);
}

TEST(CheckCsvDecode, Utf16BigEndianAndSurrogates) {
  // BOM, U+1F600 as a surrogate pair, then 'x'.
  const std::string bytes("\xFE\xFF\xD8\x3D\xDE\x00\x00x", 8);
  std::string out, error;
  ASSERT_TRUE(decode_text(bytes, "auto", out, error)) << error;
  EXPECT_EQ("\xF0\x9F\x98\x80x", out);
}

TEST(CheckCsvDecode, ExplicitUtf16WithoutBom) {
  const std::string bytes("a\x00,\x00", 4);
  std::string out, error;
  ASSERT_TRUE(decode_text(bytes, "UTF-16LE", out, error)) << error;
  EXPECT_EQ("a,", out);
}

TEST(CheckCsvDecode, OddUtf16IsAnError) {
  std::string out, error;
  EXPECT_FALSE(decode_text(std::string("\xFF\xFE"
                                       "a",
                                       3),
                           "auto", out, error));
  EXPECT_NE(std::string::npos, error.find("UTF-16")) << error;
}

TEST(CheckCsvDecode, Latin1) {
  std::string out, error;
  ASSERT_TRUE(decode_text("caf\xE9", "latin1", out, error));
  EXPECT_EQ("caf\xC3\xA9", out);
}

TEST(CheckCsvDecode, UnknownEncodingIsAnError) {
  std::string out, error;
  EXPECT_FALSE(decode_text("x", "ebcdic", out, error));
}

// The splitter itself is csv_parser (include/parsers/csv/csv_parser_test.cpp);
// these cases only cover how the check passes its options through.

TEST(CheckCsv, QuoteNoneAndOtherDelimiters) {
  runner r;
  r.source.files["/x.csv"] = "a;\"b\"\rc;d\n";
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL,
            r.run({"file=/x.csv", "delimiter=semicolon", "quote=none", "columns=N, V", "critical=V = '\"b\"'", "detail-syntax=%(N)"}))
      << r.message();
  EXPECT_EQ("CRITICAL: a", r.message());
  EXPECT_EQ(PB::Common::ResultCode::OK, r.run({"file=/x.csv", "delimiter=pipe", "quote=none", "columns=N"})) << r.message();
  EXPECT_EQ("OK: 2 rows matched", r.message());
}

TEST(CheckCsv, ParseErrorsAreUnknownAndNameTheFile) {
  runner r;
  r.source.files["/x.csv"] = "a,\"open\n";
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/x.csv", "columns=a"}));
  EXPECT_EQ("Unterminated quoted field starting on line 1: /x.csv", r.message());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/x.csv", "columns=a", "quote=;", "delimiter=semicolon"}));
  EXPECT_EQ("The quote and the delimiter must differ", r.message());
}

// --- the check -------------------------------------------------------------

TEST(CheckCsv, RowCountModeIsCriticalWhenRowsMatch) {
  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.run({"file=/data/updates.csv", "header=true", "columns=Hostname TEXT, WaitingUpdates INTEGER, Load REAL",
                                                     "filter=WaitingUpdates > 5", "critical=count > 0"}))
      << r.message();
  EXPECT_EQ("CRITICAL: Hostname=srv2 WaitingUpdates=7 Load=1.5, Hostname=srv3 WaitingUpdates=12 Load=3.75", r.message());
}

TEST(CheckCsv, RowCountModeIsOkWhenNothingMatches) {
  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  EXPECT_EQ(PB::Common::ResultCode::OK, r.run({"file=/data/updates.csv", "header=true", "columns=Hostname TEXT, WaitingUpdates INTEGER",
                                               "filter=WaitingUpdates > 100", "critical=count > 0"}))
      << r.message();
  EXPECT_EQ("OK: No rows matched the filter", r.message());
}

TEST(CheckCsv, SingleValueModeThresholdsOneCell) {
  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, r.run({"file=/data/updates.csv", "header=true", "columns=Hostname TEXT, WaitingUpdates INTEGER",
                                                    "filter=Hostname = 'srv2'", "warning=WaitingUpdates > 5", "critical=WaitingUpdates > 10",
                                                    "detail-syntax=%(Hostname): %(WaitingUpdates) updates", "perf-syntax=%(Hostname)"}))
      << r.message();
  EXPECT_EQ("WARNING: srv2: 7 updates", r.message());
  EXPECT_EQ("srv2_WaitingUpdates", perf_of(r.response));
}

TEST(CheckCsv, RealColumnsCompareAsNumbers) {
  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.run({"file=/data/updates.csv", "header=true", "columns=Hostname, WaitingUpdates INTEGER, Load REAL",
                                                     "critical=Load > 3.5", "detail-syntax=%(Hostname)"}))
      << r.message();
  EXPECT_EQ("CRITICAL: srv3", r.message());
}

TEST(CheckCsv, RealColumnsDoNotTruncateAgainstAnIntegerThreshold) {
  // Load is 0.25 / 1.5 / 3.75: an integer threshold must compare as a decimal,
  // not against the value with its fraction cut off (3.75 > 3 is true).
  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.run({"file=/data/updates.csv", "header=true", "columns=Hostname, WaitingUpdates INTEGER, Load REAL",
                                                     "critical=Load > 3", "detail-syntax=%(Hostname)"}))
      << r.message();
  EXPECT_EQ("CRITICAL: srv3", r.message());
  EXPECT_EQ(PB::Common::ResultCode::WARNING, r.run({"file=/data/updates.csv", "header=true", "columns=Hostname, WaitingUpdates INTEGER, Load REAL",
                                                    "warning=Load >= 1", "detail-syntax=%(Hostname)"}))
      << r.message();
  EXPECT_EQ("WARNING: srv2, srv3", r.message());
}

TEST(CheckCsv, EmptyOrTextRealCellNeverMatchesANumber) {
  runner r;
  r.source.files["/x.csv"] = "a,\nb,0.5\nc,n/a\n";
  // Only b has a value; a blank or non-numeric cell is not 0.
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.run({"file=/x.csv", "columns=Name, Load REAL", "critical=Load < 1", "detail-syntax=%(Name)"})) << r.message();
  EXPECT_EQ("CRITICAL: b", r.message());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.run({"file=/x.csv", "columns=Name, Load REAL", "critical=Load = ''", "detail-syntax=%(Name)"})) << r.message();
  EXPECT_EQ("CRITICAL: a, c", r.message());
}

TEST(CheckCsv, RealPerfdataKeepsTheFractionAndSkipsEmptyCells) {
  runner r;
  r.source.files["/x.csv"] = "a,3.75\nb,\n";
  EXPECT_EQ(PB::Common::ResultCode::WARNING,
            r.run({"file=/x.csv", "columns=Name, Load REAL", "warning=Load > 3", "detail-syntax=%(Name)=%(Load)", "perf-syntax=%(Name)"}))
      << r.message();
  EXPECT_EQ("WARNING: a=3.75", r.message());
  ASSERT_EQ(1, r.response.lines_size());
  ASSERT_EQ(1, r.response.lines(0).perf_size()) << perf_of(r.response);
  const PB::Common::PerformanceData &perf = r.response.lines(0).perf(0);
  EXPECT_EQ("a_Load", perf.alias());
  ASSERT_TRUE(perf.has_float_value());
  EXPECT_DOUBLE_EQ(3.75, perf.float_value().value());
  EXPECT_DOUBLE_EQ(3.0, perf.float_value().warning().value());
}

TEST(CheckCsv, DecimalsDoNotFollowTheProcessLocale) {
  // The agent calls setlocale(LC_ALL, "") at startup; on a host whose locale
  // writes decimals with a comma, a C-library parser reads "3.75" as no value.
  const char *candidates[] = {"de_DE.UTF-8", "de_DE.utf8", "sv_SE.UTF-8", "German_Germany.1252", "Swedish_Sweden.1252"};
  const char *selected = nullptr;
  for (const char *name : candidates) {
    if (std::setlocale(LC_NUMERIC, name) != nullptr) {
      selected = name;
      break;
    }
  }
  if (selected == nullptr) GTEST_SKIP() << "no decimal-comma locale installed on this host";
  ASSERT_EQ(',', *std::localeconv()->decimal_point) << selected;

  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  const PB::Common::ResultCode code =
      r.run({"file=/data/updates.csv", "header=true", "columns=Hostname, WaitingUpdates INTEGER, Load REAL", "critical=Load > 3", "detail-syntax=%(Hostname)"});
  const std::string message = r.message();
  r.source.files["/x.csv"] = "a,3.0\n";
  const PB::Common::ResultCode int_code = r.run({"file=/x.csv", "columns=Name, Count INTEGER", "critical=Count = 3", "detail-syntax=%(Name)"});
  std::setlocale(LC_NUMERIC, "C");

  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, code) << message;
  EXPECT_EQ("CRITICAL: srv3", message);
  // INTEGER also reads "3.0" (SQLite affinity) through the same parser.
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, int_code);
}

TEST(CheckCsv, ColumnsStartingWithNotAreOrdinaryKeywords) {
  // "not" is a word of the filter language; a column that merely starts with
  // it (NotAfter, notes) must still be a column, at the start of a filter and
  // after and/or, whether it was declared or taken from the header.
  runner r;
  r.source.files["/certs.csv"] = "Name,NotAfter,notes\nweb,2026-01-01,ok\ndb,2027-06-30,renew\n";
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.run({"file=/certs.csv", "header=true", "critical=NotAfter = '2026-01-01'", "detail-syntax=%(Name)"}))
      << r.message();
  EXPECT_EQ("CRITICAL: web", r.message());
  EXPECT_EQ(PB::Common::ResultCode::WARNING, r.run({"file=/certs.csv", "header=true", "warning=Name = 'db' and notes = 'renew'", "detail-syntax=%(Name)"}))
      << r.message();
  EXPECT_EQ("WARNING: db", r.message());
  // ...while the word itself still negates.
  EXPECT_EQ(PB::Common::ResultCode::WARNING, r.run({"file=/certs.csv", "header=true", "warning=not notes = 'ok'", "detail-syntax=%(Name)"})) << r.message();
  EXPECT_EQ("WARNING: db", r.message());
}

TEST(CheckCsv, ColumnsFromTheHeaderRow) {
  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL,
            r.run({"file=/data/updates.csv", "header=true", "filter=Hostname = 'srv3'", "critical=count = 1", "detail-syntax=%(WaitingUpdates)"}))
      << r.message();
  EXPECT_EQ("CRITICAL: 12", r.message());
}

TEST(CheckCsv, WithoutHeaderTheFirstRowIsData) {
  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  EXPECT_EQ(PB::Common::ResultCode::OK, r.run({"file=/data/updates.csv", "columns=Hostname, WaitingUpdates INTEGER"})) << r.message();
  EXPECT_EQ("OK: 4 rows matched", r.message());
}

TEST(CheckCsv, SkipLinesDropsThePowerShellTypeLine) {
  runner r;
  r.source.files["C:/export.csv"] = std::string("\xFF\xFE", 2);
  const std::string text = "#TYPE System.Management.Automation.PSCustomObject\r\n\"Name\",\"Count\"\r\n\"a\",\"1\"\r\n\"b\",\"2\"\r\n";
  for (const char c : text) {
    r.source.files["C:/export.csv"] += c;
    r.source.files["C:/export.csv"] += '\0';
  }
  EXPECT_EQ(PB::Common::ResultCode::WARNING,
            r.run({"file=C:/export.csv", "skip-lines=1", "header=true", "columns=Name, Count INTEGER", "warning=Count > 1", "detail-syntax=%(Name)"}))
      << r.message();
  EXPECT_EQ("WARNING: b", r.message());
  // ... and a column named Count does not disturb the summary %(count).
  EXPECT_EQ(PB::Common::ResultCode::WARNING, r.run({"file=C:/export.csv", "skip-lines=1", "header=true", "warning=Count = '2'",
                                                    "top-syntax=%(status): %(count) of %(total): %(list)", "detail-syntax=%(Name)=%(Count)"}))
      << r.message();
  EXPECT_EQ("WARNING: 2 of 2: a=1, b=2", r.message());
}

TEST(CheckCsv, SkipLinesCountsBlankLines) {
  // Counted as records, the blank line would not count: skip-lines=2 would
  // then eat the header too and make the first data row the header.
  runner r;
  r.source.files["/x.csv"] = "Exported 2026-11-24\n\nName,Count\na,1\nb,3\n";
  EXPECT_EQ(PB::Common::ResultCode::OK, r.run({"file=/x.csv", "skip-lines=2", "header=true", "top-syntax=%(count) rows: %(list)", "detail-syntax=%(Name)"}))
      << r.message();
  EXPECT_EQ("2 rows: a, b", r.message());
}

TEST(CheckCsv, SkipLinesDoesNotParseTheSkippedLines) {
  // A preamble that opens a quote and never closes it: skipped, it must not
  // swallow the rest of the file.
  runner r;
  r.source.files["/x.csv"] = "\"Quarterly report\nName,Count\na,1\nb,3\n";
  EXPECT_EQ(PB::Common::ResultCode::WARNING,
            r.run({"file=/x.csv", "skip-lines=1", "header=true", "columns=Name, Count INTEGER", "warning=Count > 2", "detail-syntax=%(Name)"}))
      << r.message();
  EXPECT_EQ("WARNING: b", r.message());
}

TEST(CheckCsv, EmptyIntegerCellHasNoValue) {
  runner r;
  r.source.files["/x.csv"] = "a,\nb,5\nc\n";
  EXPECT_EQ(PB::Common::ResultCode::OK, r.run({"file=/x.csv", "columns=Name, Value INTEGER", "critical=Value < 1"})) << r.message();
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL,
            r.run({"file=/x.csv", "columns=Name, Value INTEGER", "critical=Value = ''", "detail-syntax=%(Name)/%(field_count)"}))
      << r.message();
  EXPECT_EQ("CRITICAL: a/2, c/1", r.message());
}

TEST(CheckCsv, RowKeywordNumbersDataRows) {
  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  EXPECT_EQ(PB::Common::ResultCode::WARNING,
            r.run({"file=/data/updates.csv", "header=true", "columns=Hostname", "warning=row = 2", "detail-syntax=%(row):%(Hostname)"}))
      << r.message();
  EXPECT_EQ("WARNING: 2:srv2", r.message());
}

TEST(CheckCsv, DelimiterNames) {
  runner r;
  r.source.files["/x.tsv"] = "a\t1\nb\t2\n";
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.run({"file=/x.tsv", "delimiter=tab", "columns=N, V INTEGER", "critical=V = 2", "detail-syntax=%(N)"}))
      << r.message();
  EXPECT_EQ("CRITICAL: b", r.message());
}

TEST(CheckCsv, ArgumentErrorsAreUnknown) {
  runner r;
  r.source.files["/x.csv"] = "a,1\n";
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"columns=a"}));
  EXPECT_NE(std::string::npos, r.message().find("No CSV specified")) << r.message();
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/x.csv"}));
  EXPECT_NE(std::string::npos, r.message().find("No columns declared")) << r.message();
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/x.csv", "columns=count"}));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/x.csv", "columns=a", "delimiter=ab"}));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/x.csv", "columns=a", "quote=,"}));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/x.csv", "columns=a", "encoding=ebcdic"}));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/missing.csv", "columns=a"}));
  EXPECT_NE(std::string::npos, r.message().find("No such file")) << r.message();
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/x.csv", "columns=a", "max-size=2"}));
  EXPECT_NE(std::string::npos, r.message().find("max-size")) << r.message();
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/x.csv", "columns=a", "filter=nosuchcolumn = 1"}));
}

TEST(CheckCsv, HeaderOnlyFileIsEmpty) {
  runner r;
  r.source.files["/x.csv"] = "Name,Value\n";
  EXPECT_EQ(PB::Common::ResultCode::OK, r.run({"file=/x.csv", "header=true"})) << r.message();
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/x.csv", "header=true", "empty-state=unknown"})) << r.message();
  r.source.files["/empty.csv"] = "";
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/empty.csv", "header=true"})) << r.message();
  EXPECT_NE(std::string::npos, r.message().find("no header row")) << r.message();
}

TEST(CheckCsv, ThePathIsReadWithTheSizeLimit) {
  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  EXPECT_EQ(PB::Common::ResultCode::OK, r.run({"file=/data/updates.csv", "header=true"})) << r.message();
  EXPECT_EQ("/data/updates.csv", r.source.last_source);
  EXPECT_EQ(16u * 1024u * 1024u, r.source.last_max_bytes);
  EXPECT_EQ(PB::Common::ResultCode::OK, r.run({"file=/data/updates.csv", "header=true", "max-size=1k"})) << r.message();
  EXPECT_EQ(1024u, r.source.last_max_bytes);
}

TEST(CheckCsv, PredefinedNameResolvesToItsPath) {
  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  r.access.set_mode("predefined");
  r.access.add_predefined("updates", "/data/updates.csv");
  EXPECT_EQ(PB::Common::ResultCode::OK, r.run({"file=updates", "header=true"})) << r.message();
  EXPECT_EQ("/data/updates.csv", r.source.last_source);
}

TEST(CheckCsv, RestrictedAccessRefusesAPathOutsideTheAllowList) {
  runner r;
  r.source.files["/etc/passwd"] = "root:x:0:0\n";
  r.access.set_mode("predefined");
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.run({"file=/etc/passwd", "columns=a", "delimiter=:"})) << r.message();
  EXPECT_TRUE(r.source.last_source.empty()) << "the source must not be read before the gate";
}

// --- the file readers (csv_source_win.cpp / csv_source_unix.cpp) ----------
// These run against the real file system: the reader is the only thing
// between a caller-supplied path and an unbounded read.

namespace {
struct scratch_dir {
  boost::filesystem::path dir;
  scratch_dir() : dir(boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("check-csv-%%%%-%%%%")) {
    boost::filesystem::create_directories(dir);
  }
  ~scratch_dir() {
    boost::system::error_code ec;
    boost::filesystem::remove_all(dir, ec);
  }
  std::string write(const std::string &name, const std::string &content) const {
    const boost::filesystem::path file = dir / name;
    std::ofstream out(file.string().c_str(), std::ios::binary);
    out << content;
    return file.string();
  }
};
}  // namespace

TEST(CheckCsvReader, ReadsARegularFile) {
  const scratch_dir tmp;
  const std::string content("a,1\r\nb,\0002\r\n", 11);
  std::string bytes, error;
  ASSERT_TRUE(read_local_file(tmp.write("x.csv", content), 1024, bytes, error)) << error;
  EXPECT_EQ(content, bytes);
}

TEST(CheckCsvReader, RefusesAFileOverTheLimit) {
  const scratch_dir tmp;
  std::string bytes, error;
  EXPECT_FALSE(read_local_file(tmp.write("x.csv", std::string(100, 'x')), 10, bytes, error));
  EXPECT_NE(std::string::npos, error.find("larger than max-size (10 bytes)")) << error;
}

TEST(CheckCsvReader, ZeroLimitMeansUnlimited) {
  const scratch_dir tmp;
  std::string bytes, error;
  ASSERT_TRUE(read_local_file(tmp.write("x.csv", std::string(200000, 'x')), 0, bytes, error)) << error;
  EXPECT_EQ(200000u, bytes.size());
}

TEST(CheckCsvReader, MissingFileIsAnError) {
  const scratch_dir tmp;
  std::string bytes, error;
  EXPECT_FALSE(read_local_file((tmp.dir / "missing.csv").string(), 1024, bytes, error));
  EXPECT_NE(std::string::npos, error.find("Failed to open")) << error;
}

TEST(CheckCsvReader, DirectoryIsRefused) {
  const scratch_dir tmp;
  std::string bytes, error;
  EXPECT_FALSE(read_local_file(tmp.dir.string(), 1024, bytes, error));
  EXPECT_FALSE(error.empty());
}

#ifndef WIN32
TEST(CheckCsvReader, DeviceIsRefusedBeforeAnythingIsRead) {
  // /dev/zero reports a size of 0 and never ends: read in full it exhausts
  // memory, so it has to be refused for what it is, not for its size.
  std::string bytes, error;
  EXPECT_FALSE(read_local_file("/dev/zero", 1024, bytes, error));
  EXPECT_EQ("Not a regular file: /dev/zero", error);
  EXPECT_TRUE(bytes.empty());
  EXPECT_FALSE(read_local_file("/dev/zero", 0, bytes, error));  // unlimited too
}

TEST(CheckCsvReader, SymlinkToADeviceIsRefused) {
  const scratch_dir tmp;
  const std::string link = (tmp.dir / "looks-like.csv").string();
  ASSERT_EQ(0, ::symlink("/dev/zero", link.c_str()));
  std::string bytes, error;
  EXPECT_FALSE(read_local_file(link, 1024, bytes, error));
  EXPECT_EQ("Not a regular file: " + link, error);
}

TEST(CheckCsvReader, FifoIsRefusedWithoutBlocking) {
  // Opening a FIFO that has no writer blocks a plain open() forever.
  const scratch_dir tmp;
  const std::string fifo = (tmp.dir / "pipe.csv").string();
  ASSERT_EQ(0, ::mkfifo(fifo.c_str(), 0600));
  std::string bytes, error;
  EXPECT_FALSE(read_local_file(fifo, 1024, bytes, error));
  EXPECT_EQ("Not a regular file: " + fifo, error);
}

TEST(CheckCsvReader, LimitIsEnforcedWhileReadingAFileThatReportsNoSize) {
  // A /proc file is a regular file whose size reads as 0 while it has
  // content: only the per-chunk check can stop it.
  struct stat st{};
  ASSERT_EQ(0, ::stat("/proc/self/status", &st));
  ASSERT_EQ(0, st.st_size);
  std::string bytes, error;
  EXPECT_FALSE(read_local_file("/proc/self/status", 16, bytes, error));
  EXPECT_NE(std::string::npos, error.find("larger than max-size (16 bytes)")) << error;
  ASSERT_TRUE(read_local_file("/proc/self/status", 0, bytes, error)) << error;
  EXPECT_FALSE(bytes.empty());
}
#else
TEST(CheckCsvReader, NamedPipeIsRefusedWithoutBlocking) {
  // A client end of a named pipe opens fine, and ReadFile on it then waits for
  // the server to write: the check would hang.
  const std::wstring name = L"\\\\.\\pipe\\nscp-check-csv-test-" + std::to_wstring(GetCurrentProcessId());
  const HANDLE server = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 0, 0, 0, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, server);
  std::string bytes, error;
  const bool read = read_local_file(utf8::cvt<std::string>(name), 1024, bytes, error);
  CloseHandle(server);
  EXPECT_FALSE(read);
  EXPECT_NE(std::string::npos, error.find("Not a regular file")) << error;
}
#endif
