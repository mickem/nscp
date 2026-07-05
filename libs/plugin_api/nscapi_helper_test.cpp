// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper.hpp>

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
