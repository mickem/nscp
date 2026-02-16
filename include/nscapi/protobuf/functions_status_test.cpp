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

#include <NSCAPI.h>
#include <gtest/gtest.h>

#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_status.hpp>

// Status conversion tests
TEST(StatusConversionTest, gbp_to_nagios_status) {
  EXPECT_EQ(NSCAPI::query_return_codes::returnOK, nscapi::protobuf::functions::gbp_to_nagios_status(PB::Common::ResultCode::OK));
  EXPECT_EQ(NSCAPI::query_return_codes::returnWARN, nscapi::protobuf::functions::gbp_to_nagios_status(PB::Common::ResultCode::WARNING));
  EXPECT_EQ(NSCAPI::query_return_codes::returnCRIT, nscapi::protobuf::functions::gbp_to_nagios_status(PB::Common::ResultCode::CRITICAL));
  EXPECT_EQ(NSCAPI::query_return_codes::returnUNKNOWN, nscapi::protobuf::functions::gbp_to_nagios_status(PB::Common::ResultCode::UNKNOWN));
}

// parse_nagios tests
TEST(StatusConversionTest, parse_nagios_ok) {
  EXPECT_EQ(PB::Common::ResultCode::OK, nscapi::protobuf::functions::parse_nagios("ok"));
  EXPECT_EQ(PB::Common::ResultCode::OK, nscapi::protobuf::functions::parse_nagios("OK"));
  EXPECT_EQ(PB::Common::ResultCode::OK, nscapi::protobuf::functions::parse_nagios("o"));
  EXPECT_EQ(PB::Common::ResultCode::OK, nscapi::protobuf::functions::parse_nagios("0"));
}

TEST(StatusConversionTest, parse_nagios_warning) {
  EXPECT_EQ(PB::Common::ResultCode::WARNING, nscapi::protobuf::functions::parse_nagios("warn"));
  EXPECT_EQ(PB::Common::ResultCode::WARNING, nscapi::protobuf::functions::parse_nagios("WARNING"));
  EXPECT_EQ(PB::Common::ResultCode::WARNING, nscapi::protobuf::functions::parse_nagios("w"));
  EXPECT_EQ(PB::Common::ResultCode::WARNING, nscapi::protobuf::functions::parse_nagios("1"));
}

TEST(StatusConversionTest, parse_nagios_critical) {
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, nscapi::protobuf::functions::parse_nagios("crit"));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, nscapi::protobuf::functions::parse_nagios("CRITICAL"));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, nscapi::protobuf::functions::parse_nagios("c"));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, nscapi::protobuf::functions::parse_nagios("2"));
}

TEST(StatusConversionTest, parse_nagios_unknown) {
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, nscapi::protobuf::functions::parse_nagios("unknown"));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, nscapi::protobuf::functions::parse_nagios("invalid"));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, nscapi::protobuf::functions::parse_nagios("3"));
}

// nagios_status_to_gpb tests
TEST(StatusConversionTest, nagios_status_to_gpb) {
  EXPECT_EQ(PB::Common::ResultCode::OK, nscapi::protobuf::functions::nagios_status_to_gpb(NSCAPI::query_return_codes::returnOK));
  EXPECT_EQ(PB::Common::ResultCode::WARNING, nscapi::protobuf::functions::nagios_status_to_gpb(NSCAPI::query_return_codes::returnWARN));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, nscapi::protobuf::functions::nagios_status_to_gpb(NSCAPI::query_return_codes::returnCRIT));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, nscapi::protobuf::functions::nagios_status_to_gpb(NSCAPI::query_return_codes::returnUNKNOWN));
}
