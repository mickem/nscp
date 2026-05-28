/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include "static_controller.hpp"

#include <gtest/gtest.h>

#include <string>

// The HTTP-routing side of StaticController is hard to unit-test without
// standing up a Mongoose server, but the built-in placeholder served when
// the web bundle isn't installed is a pure value and very much worth
// pinning: regressions here turn a fresh `apt install` into a confusing
// 500 instead of a friendly "run nscp web install-ui" page.

TEST(StaticControllerPlaceholder, IsNonEmpty) {
  EXPECT_FALSE(StaticController::placeholder_html().empty());
}

TEST(StaticControllerPlaceholder, LooksLikeAnHtmlDocument) {
  const std::string &h = StaticController::placeholder_html();
  EXPECT_NE(h.find("<!DOCTYPE html>"), std::string::npos);
  EXPECT_NE(h.find("<title>"), std::string::npos);
  EXPECT_NE(h.find("</html>"), std::string::npos);
}

TEST(StaticControllerPlaceholder, ShowsTheInstallCommand) {
  // The whole reason this page exists: tell the operator what to type.
  // If this regresses (e.g. the command gets renamed without updating the
  // placeholder), users hitting `/` get a page that doesn't explain how
  // to fix the problem.
  EXPECT_NE(StaticController::placeholder_html().find("nscp web install-ui"), std::string::npos);
}

TEST(StaticControllerPlaceholder, MentionsAirGappedFromFlag) {
  // The --from path lets offline / air-gapped operators install. Surface
  // it on the placeholder so they don't have to read docs to find it.
  EXPECT_NE(StaticController::placeholder_html().find("--from"), std::string::npos);
}

TEST(StaticControllerPlaceholder, HasNoExternalResources) {
  // The placeholder must work when *nothing* else in the bundle works:
  // a fresh install with no JS, no CSS, no images on disk. Any external
  // <script>, <link rel=stylesheet>, or <img src=> would defeat that.
  const std::string &h = StaticController::placeholder_html();
  EXPECT_EQ(h.find("<script"), std::string::npos);
  EXPECT_EQ(h.find("<link"), std::string::npos);
  EXPECT_EQ(h.find("<img"), std::string::npos);
  // Same for <iframe>, which would also fail without a network fetch.
  EXPECT_EQ(h.find("<iframe"), std::string::npos);
}

TEST(StaticControllerPlaceholder, IsReasonablySized) {
  // Caps a runaway: if someone inlines the whole bundled UI as an
  // emergency fallback, the placeholder loses its "always works"
  // property and the daemon binary grows uncomfortably. A few kilobytes
  // is plenty for the static HTML+CSS we serve here.
  EXPECT_LT(StaticController::placeholder_html().size(), 8 * 1024u);
}
