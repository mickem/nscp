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

#include <parsers/cron/cron_parser.hpp>

#include <str/format.hpp>
#include <str/utils.hpp>

#include <vector>
#include <string>

#include <gtest/gtest.h>

TEST(cron, test_parse_simple) {
	cron_parser::schedule s = cron_parser::parse("0 0 1 1 0");
	EXPECT_EQ("0 0 1 1 0", s.to_string());

	s = cron_parser::parse("1 1 1 1 1");
	EXPECT_EQ("1 1 1 1 1", s.to_string());

	s = cron_parser::parse("2 3 4 5 6");
	EXPECT_EQ("2 3 4 5 6", s.to_string());

	s = cron_parser::parse("59 23 31 12 6");
	EXPECT_EQ("59 23 31 12 6", s.to_string());
}

TEST(cron, test_parse_wildcard) {

	cron_parser::schedule s = cron_parser::parse("* 23 31 12 6");
	EXPECT_EQ("* 23 31 12 6", s.to_string());

	s = cron_parser::parse("59 * 31 12 6");
	EXPECT_EQ("59 * 31 12 6", s.to_string());

	s = cron_parser::parse("59 23 * 12 6");
	EXPECT_EQ("59 23 * 12 6", s.to_string());

	s = cron_parser::parse("59 23 31 * 6");
	EXPECT_EQ("59 23 31 * 6", s.to_string());

	s = cron_parser::parse("59 23 31 12 *");
	EXPECT_EQ("59 23 31 12 *", s.to_string());

}

TEST(cron, test_parse_list) {

	cron_parser::schedule s = cron_parser::parse("10,11 23 31 12 6");
	EXPECT_EQ("10,11 23 31 12 6", s.to_string());

	s = cron_parser::parse("59 10,12,23 31 12 6");
	EXPECT_EQ("59 10,12,23 31 12 6", s.to_string());

}

std::string get_next(std::string schedule, std::string date) {
	cron_parser::schedule s = cron_parser::parse(schedule);
	boost::posix_time::ptime now(boost::posix_time::time_from_string(date));
	boost::posix_time::ptime next = s.find_next(now);
	return boost::posix_time::to_iso_extended_string(next);
}

TEST(cron, test_parse_single_date_1) {

	// minute
	EXPECT_EQ("2016-01-01T01:01:00", get_next("1 * * * *", "2016-01-01 01:00:00"));
	EXPECT_EQ("2016-01-01T02:01:00", get_next("1 * * * *", "2016-01-01 01:01:00"));
	EXPECT_EQ("2016-01-01T02:01:00", get_next("1 * * * *", "2016-01-01 01:02:00"));

	// hour
	EXPECT_EQ("2016-01-01T01:00:00", get_next("* 1 * * *", "2016-01-01 00:00:00"));
	EXPECT_EQ("2016-01-01T01:01:00", get_next("* 1 * * *", "2016-01-01 01:00:00"));
	EXPECT_EQ("2016-01-02T01:00:00", get_next("* 1 * * *", "2016-01-01 01:59:00"));
	EXPECT_EQ("2016-01-02T01:00:00", get_next("* 1 * * *", "2016-01-01 02:00:00"));

	// day of month
	EXPECT_EQ("2016-01-01T00:01:00", get_next("* * 1 * *", "2016-01-01 00:00:00"));
	EXPECT_EQ("2016-01-01T23:59:00", get_next("* * 1 * *", "2016-01-01 23:58:00"));
	EXPECT_EQ("2016-02-01T00:00:00", get_next("* * 1 * *", "2016-01-01 23:59:00"));
	EXPECT_EQ("2016-02-01T00:00:00", get_next("* * 1 * *", "2016-01-02 02:00:00"));
	EXPECT_EQ("2016-02-01T00:00:00", get_next("* * 1 * *", "2016-01-02 07:00:00"));
	EXPECT_EQ("2016-02-01T00:00:00", get_next("* * 1 * *", "2016-01-02 23:00:00"));

	// month
	EXPECT_EQ("2016-01-01T00:01:00", get_next("* * * 1 *", "2016-01-01 00:00:00"));
	EXPECT_EQ("2016-01-31T23:59:00", get_next("* * * 1 *", "2016-01-31 23:58:00"));
	EXPECT_EQ("2017-01-01T00:00:00", get_next("* * * 1 *", "2016-01-31 23:59:00"));
	EXPECT_EQ("2017-01-01T00:00:00", get_next("* * * 1 *", "2016-02-01 02:00:00"));
	EXPECT_EQ("2017-01-01T00:00:00", get_next("* * * 1 *", "2016-02-07 02:11:59"));
	EXPECT_EQ("2017-01-01T00:00:00", get_next("* * * 1 *", "2016-09-07 23:18:14"));

	// day of week
	EXPECT_EQ("2016-01-03T00:00:00", get_next("* * * * 1", "2016-01-03 00:00:00"));
	EXPECT_EQ("2016-01-04T23:59:00", get_next("* * * * 1", "2016-01-04 23:58:00"));
	EXPECT_EQ("2016-01-12T00:00:00", get_next("* * * * 1", "2016-01-04 23:59:00"));
	EXPECT_EQ("2016-01-12T00:00:00", get_next("* * * * 1", "2016-01-05 00:00:00"));

}

TEST(cron, test_parse_single_date_2) {

	// minute
	EXPECT_EQ("2016-01-01T01:02:00", get_next("2 * * * *", "2016-01-01 01:01:00"));
	EXPECT_EQ("2016-01-01T02:02:00", get_next("2 * * * *", "2016-01-01 01:02:00"));
	EXPECT_EQ("2016-01-01T02:02:00", get_next("2 * * * *", "2016-01-01 01:03:00"));

	// hour
	EXPECT_EQ("2016-01-01T02:00:00", get_next("* 2 * * *", "2016-01-01 01:00:00"));
	EXPECT_EQ("2016-01-01T02:01:00", get_next("* 2 * * *", "2016-01-01 02:00:00"));
	EXPECT_EQ("2016-01-02T02:00:00", get_next("* 2 * * *", "2016-01-01 02:59:00"));
	EXPECT_EQ("2016-01-02T02:00:00", get_next("* 2 * * *", "2016-01-01 03:00:00"));

	// day of month
	EXPECT_EQ("2016-01-02T00:01:00", get_next("* * 2 * *", "2016-01-02 00:00:00"));
	EXPECT_EQ("2016-01-02T23:59:00", get_next("* * 2 * *", "2016-01-02 23:58:00"));
	EXPECT_EQ("2016-02-02T00:00:00", get_next("* * 2 * *", "2016-01-02 23:59:00"));
	EXPECT_EQ("2016-02-02T00:00:00", get_next("* * 2 * *", "2016-01-03 02:00:00"));

	// month
	EXPECT_EQ("2016-02-01T00:01:00", get_next("* * * 2 *", "2016-02-01 00:00:00"));
	EXPECT_EQ("2016-02-29T23:59:00", get_next("* * * 2 *", "2016-02-29 23:58:00"));
	EXPECT_EQ("2017-02-01T00:00:00", get_next("* * * 2 *", "2016-02-29 23:59:00"));
	EXPECT_EQ("2017-02-01T00:00:00", get_next("* * * 2 *", "2016-03-01 02:00:00"));
	EXPECT_EQ("2017-02-01T00:00:00", get_next("* * * 2 *", "2016-03-07 02:11:59"));
	EXPECT_EQ("2017-02-01T00:00:00", get_next("* * * 2 *", "2016-03-07 23:18:14"));

	// day of week
	EXPECT_EQ("2016-01-04T00:00:00", get_next("* * * * 2", "2016-01-04 00:00:00"));
	EXPECT_EQ("2016-01-05T23:59:00", get_next("* * * * 2", "2016-01-05 23:58:00"));
	EXPECT_EQ("2016-01-13T00:00:00", get_next("* * * * 2", "2016-01-05 23:59:00"));
	EXPECT_EQ("2016-01-13T00:00:00", get_next("* * * * 2", "2016-01-06 00:00:00"));

}


TEST(cron, test_eval_list) {

	EXPECT_EQ("2016-01-01T01:01:00", get_next("1 * * * *", "2016-01-01 01:00:00"));
	EXPECT_EQ("2016-01-01T02:01:00", get_next("1 * * * *", "2016-01-01 01:01:00"));
	EXPECT_EQ("2016-01-01T01:01:00", get_next("1,5 * * * *", "2016-01-01 01:00:00"));
	EXPECT_EQ("2016-01-01T01:05:00", get_next("1,5 * * * *", "2016-01-01 01:01:00"));
	EXPECT_EQ("2016-01-01T02:01:00", get_next("1,5 * * * *", "2016-01-01 01:05:00"));
	EXPECT_EQ("2016-01-01T01:05:00", get_next("5,10 * * * *", "2016-01-01 01:00:00"));
	EXPECT_EQ("2016-01-01T01:10:00", get_next("5,10 * * * *", "2016-01-01 01:05:00"));
	EXPECT_EQ("2016-01-01T02:05:00", get_next("5,10 * * * *", "2016-01-01 01:10:00"));

}
