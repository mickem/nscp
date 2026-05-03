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

#include <list>
#include <string>

#include "check_process.hpp"

using process_checks::realtime::process_name_matches_any;

// ============================================================================
// process_name_matches_any — case-insensitive any-of match
//
// Backs the realtime check_process matcher (issues #587, #552). Windows
// process / executable names are case-insensitive, so the realtime path must
// match `notepad.exe` against `NOTEPAD.EXE` and any case combination. The
// active check path already does this via CaseBlindCompare; these tests pin
// the realtime path's behaviour to the same contract.
// ============================================================================

TEST(ProcessNameMatchesAny, EmptyListReturnsFalse) {
  EXPECT_FALSE(process_name_matches_any({}, "notepad.exe"));
}

TEST(ProcessNameMatchesAny, EmptyCandidateAgainstNonEmptyList) {
  EXPECT_FALSE(process_name_matches_any({"notepad.exe"}, ""));
}

TEST(ProcessNameMatchesAny, ExactMatch) {
  EXPECT_TRUE(process_name_matches_any({"notepad.exe"}, "notepad.exe"));
}

TEST(ProcessNameMatchesAny, UpperCaseCandidateMatchesLowerName) {
  EXPECT_TRUE(process_name_matches_any({"notepad.exe"}, "NOTEPAD.EXE"));
}

TEST(ProcessNameMatchesAny, LowerCaseCandidateMatchesUpperName) {
  EXPECT_TRUE(process_name_matches_any({"NOTEPAD.EXE"}, "notepad.exe"));
}

TEST(ProcessNameMatchesAny, MixedCaseMatches) {
  // Examples drawn straight from the issues: WinLogon.exe vs winlogon.exe and
  // NoTePaD.eXe — all the same process to Windows.
  EXPECT_TRUE(process_name_matches_any({"winlogon.exe"}, "WinLogon.exe"));
  EXPECT_TRUE(process_name_matches_any({"WinLogon.exe"}, "winlogon.exe"));
  EXPECT_TRUE(process_name_matches_any({"notepad.exe"}, "NoTePaD.eXe"));
}

TEST(ProcessNameMatchesAny, NoMatchOnDifferentName) {
  EXPECT_FALSE(process_name_matches_any({"notepad.exe"}, "calc.exe"));
}

TEST(ProcessNameMatchesAny, NoMatchOnSubstring) {
  // The matcher must compare full names — 'note.exe' is not the same process
  // as 'notepad.exe' and the user is configuring exact names.
  EXPECT_FALSE(process_name_matches_any({"note.exe"}, "notepad.exe"));
  EXPECT_FALSE(process_name_matches_any({"notepad"}, "notepad.exe"));
}

TEST(ProcessNameMatchesAny, MatchesAnyOfMultiple) {
  const std::list<std::string> names = {"calc.exe", "notepad.exe", "explorer.exe"};

  EXPECT_TRUE(process_name_matches_any(names, "calc.exe"));
  EXPECT_TRUE(process_name_matches_any(names, "NOTEPAD.EXE"));
  EXPECT_TRUE(process_name_matches_any(names, "Explorer.EXE"));
  EXPECT_FALSE(process_name_matches_any(names, "csrss.exe"));
}

TEST(ProcessNameMatchesAny, FirstMatchInListIsEnough) {
  // any_of semantics — even with the match in position 0 the function must
  // return true (and not iterate further).
  const std::list<std::string> names = {"notepad.exe", "another.exe"};
  EXPECT_TRUE(process_name_matches_any(names, "NOTEPAD.EXE"));
}

TEST(ProcessNameMatchesAny, LastMatchInListIsEnough) {
  const std::list<std::string> names = {"a.exe", "b.exe", "notepad.exe"};
  EXPECT_TRUE(process_name_matches_any(names, "NOTEPAD.EXE"));
}

TEST(ProcessNameMatchesAny, DuplicatesInListDoNotBreakSemantics) {
  const std::list<std::string> names = {"notepad.exe", "notepad.exe", "NOTEPAD.EXE"};
  EXPECT_TRUE(process_name_matches_any(names, "notepad.exe"));
  EXPECT_FALSE(process_name_matches_any(names, "calc.exe"));
}
