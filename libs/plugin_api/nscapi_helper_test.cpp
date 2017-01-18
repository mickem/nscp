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

#include <nscapi/nscapi_helper.hpp>

#include <gtest/gtest.h>

TEST(nscapiHelperTest, maxState_ok) {
	namespace qrc = NSCAPI::query_return_codes;
	EXPECT_EQ(qrc::returnOK, nscapi::plugin_helper::maxState(qrc::returnOK, qrc::returnOK));
}

TEST(nscapiHelperTest, maxState_warn) {
	namespace qrc = NSCAPI::query_return_codes;
	EXPECT_EQ(qrc::returnWARN, nscapi::plugin_helper::maxState(qrc::returnWARN, qrc::returnOK));
	EXPECT_EQ(qrc::returnWARN, nscapi::plugin_helper::maxState(qrc::returnWARN, qrc::returnWARN));
	EXPECT_EQ(qrc::returnWARN, nscapi::plugin_helper::maxState(qrc::returnOK, qrc::returnWARN));
}

TEST(nscapiHelperTest, maxState_crit) {
	namespace qrc = NSCAPI::query_return_codes;
	EXPECT_EQ(qrc::returnCRIT, nscapi::plugin_helper::maxState(qrc::returnCRIT, qrc::returnOK));
	EXPECT_EQ(qrc::returnCRIT, nscapi::plugin_helper::maxState(qrc::returnCRIT, qrc::returnWARN));
	EXPECT_EQ(qrc::returnCRIT, nscapi::plugin_helper::maxState(qrc::returnCRIT, qrc::returnCRIT));
	EXPECT_EQ(qrc::returnCRIT, nscapi::plugin_helper::maxState(qrc::returnOK, qrc::returnCRIT));
	EXPECT_EQ(qrc::returnCRIT, nscapi::plugin_helper::maxState(qrc::returnWARN, qrc::returnCRIT));
}

TEST(nscapiHelperTest, maxState_unknown) {
	namespace qrc = NSCAPI::query_return_codes;
	EXPECT_EQ(qrc::returnUNKNOWN, nscapi::plugin_helper::maxState(qrc::returnUNKNOWN, qrc::returnOK));
	EXPECT_EQ(qrc::returnUNKNOWN, nscapi::plugin_helper::maxState(qrc::returnUNKNOWN, qrc::returnWARN));
	EXPECT_EQ(qrc::returnUNKNOWN, nscapi::plugin_helper::maxState(qrc::returnUNKNOWN, qrc::returnCRIT));
	EXPECT_EQ(qrc::returnUNKNOWN, nscapi::plugin_helper::maxState(qrc::returnUNKNOWN, qrc::returnUNKNOWN));
	EXPECT_EQ(qrc::returnUNKNOWN, nscapi::plugin_helper::maxState(qrc::returnOK, qrc::returnUNKNOWN));
	EXPECT_EQ(qrc::returnUNKNOWN, nscapi::plugin_helper::maxState(qrc::returnWARN, qrc::returnUNKNOWN));
	EXPECT_EQ(qrc::returnUNKNOWN, nscapi::plugin_helper::maxState(qrc::returnCRIT, qrc::returnUNKNOWN));
}
