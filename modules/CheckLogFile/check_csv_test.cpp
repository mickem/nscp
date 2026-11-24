// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_csv.hpp"

#include <gtest/gtest.h>

#include <map>
#include <nscapi/nscapi_helper_singleton.hpp>

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
  ASSERT_TRUE(decode_text("\xEF\xBB\xBF" "a,b", "auto", out, error));
  EXPECT_EQ("a,b", out);
  ASSERT_TRUE(decode_text("\xEF\xBB\xBF" "a,b", "utf-8", out, error));
  EXPECT_EQ("a,b", out);
}

TEST(CheckCsvDecode, Utf16LittleEndianWithBomIsDetected) {
  // "Å,1\r\n" as Windows writes it (Export-Csv -Encoding Unicode).
  const std::string bytes("\xFF\xFE\xC5\x00,\x00" "1\x00\r\x00\n\x00", 12);
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
  EXPECT_FALSE(decode_text(std::string("\xFF\xFE" "a", 3), "auto", out, error));
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

// --- splitting -------------------------------------------------------------

TEST(CheckCsvSplit, Rfc4180Quoting) {
  std::vector<csv_record> rows;
  std::string error;
  ASSERT_TRUE(split_csv("a,\"b,c\",\"say \"\"hi\"\"\",\"\"\r\nx,y\r\n", ',', '"', rows, error)) << error;
  ASSERT_EQ(2u, rows.size());
  ASSERT_EQ(4u, rows[0].fields.size());
  EXPECT_EQ("a", rows[0].fields[0]);
  EXPECT_EQ("b,c", rows[0].fields[1]);
  EXPECT_EQ("say \"hi\"", rows[0].fields[2]);
  EXPECT_EQ("", rows[0].fields[3]);
  EXPECT_EQ(2u, rows[1].line);
}

TEST(CheckCsvSplit, BackslashIsAnOrdinaryCharacter) {
  std::vector<csv_record> rows;
  std::string error;
  ASSERT_TRUE(split_csv("srv1,C:\\data\\x.csv,\"\\\\server\\share\"", ',', '"', rows, error)) << error;
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ("C:\\data\\x.csv", rows[0].fields[1]);
  EXPECT_EQ("\\\\server\\share", rows[0].fields[2]);
}

TEST(CheckCsvSplit, QuotedLineBreakStaysInTheField) {
  std::vector<csv_record> rows;
  std::string error;
  ASSERT_TRUE(split_csv("a,\"line1\nline2\"\nb,c\n", ',', '"', rows, error)) << error;
  ASSERT_EQ(2u, rows.size());
  EXPECT_EQ("line1\nline2", rows[0].fields[1]);
  EXPECT_EQ(3u, rows[1].line);  // the record after a two-line record starts on line 3
}

TEST(CheckCsvSplit, EmptyLinesAndTrailingFields) {
  std::vector<csv_record> rows;
  std::string error;
  ASSERT_TRUE(split_csv("\n\na,\n\r\n,\nlast", ',', '"', rows, error)) << error;
  ASSERT_EQ(3u, rows.size());
  EXPECT_EQ(2u, rows[0].fields.size());  // "a,"  -> a, ""
  EXPECT_EQ(2u, rows[1].fields.size());  // ","   -> "", ""
  EXPECT_EQ(1u, rows[2].fields.size());  // no newline at the end
  EXPECT_EQ("last", rows[2].fields[0]);
}

TEST(CheckCsvSplit, OtherDelimiterAndNoQuoting) {
  std::vector<csv_record> rows;
  std::string error;
  ASSERT_TRUE(split_csv("a;\"b\";c\rd;e;f", ';', '\0', rows, error)) << error;
  ASSERT_EQ(2u, rows.size());
  EXPECT_EQ("\"b\"", rows[0].fields[1]);
  EXPECT_EQ("d", rows[1].fields[0]);
}

TEST(CheckCsvSplit, UnterminatedQuoteIsAnError) {
  std::vector<csv_record> rows;
  std::string error;
  EXPECT_FALSE(split_csv("a,b\nc,\"open\nd,e\n", ',', '"', rows, error));
  EXPECT_NE(std::string::npos, error.find("line 2")) << error;
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
  EXPECT_EQ(PB::Common::ResultCode::WARNING,
            r.run({"file=/data/updates.csv", "header=true", "columns=Hostname TEXT, WaitingUpdates INTEGER", "filter=Hostname = 'srv2'",
                   "warning=WaitingUpdates > 5", "critical=WaitingUpdates > 10", "detail-syntax=%(Hostname): %(WaitingUpdates) updates",
                   "perf-syntax=%(Hostname)"}))
      << r.message();
  EXPECT_EQ("WARNING: srv2: 7 updates", r.message());
  EXPECT_EQ("srv2_WaitingUpdates", perf_of(r.response));
}

TEST(CheckCsv, RealColumnsCompareAsNumbers) {
  runner r;
  r.source.files["/data/updates.csv"] = kUpdates;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL,
            r.run({"file=/data/updates.csv", "header=true", "columns=Hostname, WaitingUpdates INTEGER, Load REAL", "critical=Load > 3.5",
                   "detail-syntax=%(Hostname)"}))
      << r.message();
  EXPECT_EQ("CRITICAL: srv3", r.message());
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
