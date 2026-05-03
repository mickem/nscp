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

#include "check_files.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <string>

#include "test_support.hpp"

namespace {

namespace fs = boost::filesystem;
using check_disk_test_support::join_lines;
using ScratchDir = check_disk_test_support::ScratchDir;

}  // namespace

// ============================================================================
// Argument validation
// ============================================================================

TEST(CheckFilesCommand, NoPathSpecifiedReturnsUnknown) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("No path specified"), std::string::npos) << join_lines(response);
}

TEST(CheckFilesCommand, MissingPathIsReportedAsUnknown) {
  // Issue #613: a top-level path that does not exist must surface as UNKNOWN
  // with the offending path, not as a silent OK / "No files found".
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=Z:\\nscp_test_definitely_not_a_real_path_47b1f0e5");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Path was not found"), std::string::npos) << join_lines(response);
}

// ============================================================================
// Happy-path scans against a hermetic temp directory
// ============================================================================

TEST(CheckFilesCommand, EmptyDirectoryUsesDefaultEmptyStateUnknown) {
  // The default empty-state declared in check_files_command::check() is
  // "unknown". An empty directory therefore reports UNKNOWN (the legacy
  // CheckFiles wrapper overrides this to "ok" via empty-state=ok; that
  // override is verified separately below).
  ScratchDir dir;

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("No files found"), std::string::npos) << join_lines(response);
}

TEST(CheckFilesCommand, EmptyStateOverrideMakesEmptyDirectoryOk) {
  const ScratchDir dir;

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("empty-state=ok");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK);
}

TEST(CheckFilesCommand, ScansFilesInDirectory) {
  const ScratchDir dir;
  dir.touch("a.txt");
  dir.touch("b.txt");
  dir.touch("c.txt");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.txt");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK);
  // Default detail-syntax includes "%(count) files"; with three matching
  // files the rendered top message must report all three.
  EXPECT_NE(join_lines(response).find("3"), std::string::npos) << join_lines(response);
}

TEST(CheckFilesCommand, MaxDepthZeroSkipsSubdirectories) {
  // Issue #730: max-depth=0 must mean "scan the top directory only".
  const ScratchDir dir;
  dir.touch("top.txt");
  fs::create_directory(dir.path() / "nested");
  std::ofstream((dir.path() / "nested" / "deep.txt").string()) << "x";

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.txt");
  request.add_arguments("max-depth=0");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK);
  // Only top.txt is reachable at depth 0; deep.txt must not be scanned.
  const std::string out = join_lines(response);
  EXPECT_NE(out.find("1"), std::string::npos) << out;
  EXPECT_EQ(out.find("deep.txt"), std::string::npos) << out;
}

// ============================================================================
// Deferred per-row warn/crit evaluation (issues #97, #159, #291, #486)
//
// Before the deferred-eval refactor, modern_filter::match() evaluated each
// row's warn/crit expression while iterating, so summary aggregates such as
// `count` saw running totals rather than their final values. A mixed
// expression like `crit=count<N OR <row-predicate>` would therefore CRIT on
// the first row (count=1 < N) even when the final count would not satisfy
// the threshold.
//
// These tests use check_files_command — which goes through the same
// modern_filter::match_post path as every other active check — to pin the
// post-fix semantics.
// ============================================================================

TEST(CheckFilesCommand, MixedCritDoesNotFireOnRunningCount) {
  // Five files in the directory. With the pre-fix bug, `count<3` on the
  // first row evaluates true (count=1) and the verdict escalates to CRIT
  // immediately. With deferred evaluation, count=5 by the time the warn/crit
  // engine runs, so `count<3` is false and no row matches the name predicate.
  const ScratchDir dir;
  dir.touch("a.txt");
  dir.touch("b.txt");
  dir.touch("c.txt");
  dir.touch("d.txt");
  dir.touch("e.txt");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.txt");
  request.add_arguments("crit=count<3 or name='nonexistent.foo'");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckFilesCommand, MixedCritFiresWhenRowPredicateMatchesEvenIfSummaryFalse) {
  // Sanity check that the deferred-eval refactor did not break the basic
  // "row predicate matches → CRIT" path. The summary side (`count<1`) is
  // false (final count=2) so only the row predicate carries the verdict.
  const ScratchDir dir;
  dir.touch("alert.txt");
  dir.touch("benign.txt");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.txt");
  request.add_arguments("crit=count<1 or name='alert.txt'");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

// ============================================================================
// Force-evaluate warn/crit when no rows matched (issues #74, #494, #200, #283)
//
// modern_filter::match_post now invokes engine_crit->match_force() when the
// iteration produced zero matched rows, so mixed expressions like
// `crit=name='X' OR count=0` can still escalate the verdict for the empty
// case. The default expect_object=false path only handles pure-summary
// expressions (require_object=false); the mixed expression has
// require_object=true and was previously skipped entirely.
//
// `empty-state=ignored` is required so the cli_helper post_process does not
// override summary.returnCode based on the empty_state — without it, the
// final response is forced to whatever empty_state translates to (default
// "unknown" for check_files) regardless of the warn/crit verdict.
// ============================================================================

TEST(CheckFilesCommand, MixedCritFiresOnEmptyResultViaForceEvaluate) {
  // The pattern matches no files, so modern_filter::match() never runs.
  // match_post must still evaluate `crit=count=0 OR name='X'` and escalate
  // to CRIT — this is the issue #74 / #494 / #200 / #283 fix.
  const ScratchDir dir;
  dir.touch("nothing-relevant.log");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.bin");
  request.add_arguments("empty-state=ignored");
  request.add_arguments("crit=count=0 or name='alert.txt'");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST(CheckFilesCommand, PureSummaryCritFiresOnEmptyResult) {
  // Regression sentinel for the pre-existing expect_object=false branch:
  // `crit=count=0` does not require an object so it has always fired on the
  // empty case. The deferred-eval refactor must not have broken this.
  const ScratchDir dir;
  dir.touch("nothing-relevant.log");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.bin");
  request.add_arguments("empty-state=ignored");
  request.add_arguments("crit=count=0");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST(CheckFilesCommand, MixedCritDoesNotFireOnEmptyResultWhenSummarySideFalse) {
  // The mixed force-evaluate path resolves object-bound vars to a default
  // (false) and summary vars to their final values. With zero rows but a
  // summary side that explicitly requires count>0, the AND short-circuits
  // on the sure-false summary side before the bound side propagates unsure,
  // so the verdict stays OK.
  const ScratchDir dir;
  dir.touch("nothing-relevant.log");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.bin");
  request.add_arguments("empty-state=ignored");
  request.add_arguments("crit=count>0 and name='alert.txt'");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

// ============================================================================
// UNKNOWN escalation when force-evaluate produces an unsure verdict
//
// modern_filter::match_post now surfaces UNKNOWN when the no-rows
// force-evaluate path produced an unsure result (a mixed expression depended
// on an object-bound subterm that couldn't fully resolve). These tests pin
// that contract end-to-end through check_files.
// ============================================================================

TEST(CheckFilesCommand, MixedCritUnsureSurfacesAsUnknown) {
  // OR that can't short-circuit: bound side is unsure-false (name='alert.txt'
  // with no object), summary side is sure-false (size>1000 doesn't apply
  // because there are no rows — so no row makes it true; force-eval treats
  // it as bound-unsure too). The whole expression is unsure-false → no sure
  // CRIT/WARN/OK verdict → match_post escalates to UNKNOWN.
  const ScratchDir dir;
  dir.touch("nothing-relevant.log");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.bin");
  request.add_arguments("empty-state=ignored");
  // Both sides are object-bound, so both go unsure under force-eval — no
  // summary side to definitively rescue the OR. Result: UNKNOWN.
  request.add_arguments("crit=name='alert.txt' or size>1000");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

TEST(CheckFilesCommand, MixedCritInOnStringUnsureSurfacesAsUnknown) {
  // `name in (...)` on no rows: lhs is nil → operator_in::eval_string returns
  // unsure-false (was: throw → silent OK). With summary side also false, the
  // OR is unsure-false. modern_filter::match_post escalates to UNKNOWN.
  const ScratchDir dir;
  dir.touch("nothing-relevant.log");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.bin");
  request.add_arguments("empty-state=ignored");
  request.add_arguments("crit=name in ('alert.txt', 'urgent.txt') or count>0");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

TEST(CheckFilesCommand, MixedCritInOnStringSureTrueSummaryStillFiresCrit) {
  // `<unsure-false> OR <sure-true>` short-circuits in operator_or to
  // sure-true, so even with the broken-IN bound side the sure-true summary
  // side still drives the CRIT verdict. Confirms the fix did not regress
  // issue #74.
  const ScratchDir dir;
  dir.touch("nothing-relevant.log");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.bin");
  request.add_arguments("empty-state=ignored");
  request.add_arguments("crit=name in ('alert.txt') or count=0");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}
