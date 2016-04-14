#include <vector>
#include <string>
#include <format.hpp>
#include <strEx.h>
#include <parsers/cron/cron_parser.hpp>

#include <gtest/gtest.h>

TEST(cron, test_parse_simple) {
	cron_parser::schedule s = cron_parser::parse("0 0 1 1 0");
	EXPECT_EQ(0, s.min.value_);
	EXPECT_EQ(false, s.min.star_);
	EXPECT_EQ(0, s.hour.value_);
	EXPECT_EQ(false, s.hour.star_);
	EXPECT_EQ(1, s.dom.value_);
	EXPECT_EQ(false, s.dom.star_);
	EXPECT_EQ(1, s.mon.value_);
	EXPECT_EQ(false, s.mon.star_);
	EXPECT_EQ(0, s.dow.value_);
	EXPECT_EQ(false, s.dow.star_);

	s = cron_parser::parse("1 1 1 1 1");
	EXPECT_EQ(1, s.min.value_);
	EXPECT_EQ(1, s.hour.value_);
	EXPECT_EQ(1, s.dom.value_);
	EXPECT_EQ(1, s.mon.value_);
	EXPECT_EQ(1, s.dow.value_);

	s = cron_parser::parse("2 3 4 5 6");
	EXPECT_EQ(2, s.min.value_);
	EXPECT_EQ(3, s.hour.value_);
	EXPECT_EQ(4, s.dom.value_);
	EXPECT_EQ(5, s.mon.value_);
	EXPECT_EQ(6, s.dow.value_);

	s = cron_parser::parse("59 23 31 12 6");
	EXPECT_EQ(59, s.min.value_);
	EXPECT_EQ(23, s.hour.value_);
	EXPECT_EQ(31, s.dom.value_);
	EXPECT_EQ(12, s.mon.value_);
	EXPECT_EQ(6, s.dow.value_);
}

TEST(cron, test_parse_wildcard) {

	cron_parser::schedule s = cron_parser::parse("* 23 31 12 6");
	EXPECT_EQ(0, s.min.value_);
	EXPECT_EQ(true, s.min.star_);
	EXPECT_EQ(23, s.hour.value_);
	EXPECT_EQ(false, s.hour.star_);
	EXPECT_EQ(31, s.dom.value_);
	EXPECT_EQ(false, s.dom.star_);
	EXPECT_EQ(12, s.mon.value_);
	EXPECT_EQ(false, s.mon.star_);
	EXPECT_EQ(6, s.dow.value_);
	EXPECT_EQ(false, s.dow.star_);

	s = cron_parser::parse("59 * 31 12 6");
	EXPECT_EQ(59, s.min.value_);
	EXPECT_EQ(false, s.min.star_);
	EXPECT_EQ(0, s.hour.value_);
	EXPECT_EQ(true, s.hour.star_);
	EXPECT_EQ(31, s.dom.value_);
	EXPECT_EQ(false, s.dom.star_);
	EXPECT_EQ(12, s.mon.value_);
	EXPECT_EQ(false, s.mon.star_);
	EXPECT_EQ(6, s.dow.value_);
	EXPECT_EQ(false, s.dow.star_);

	s = cron_parser::parse("59 23 * 12 6");
	EXPECT_EQ(59, s.min.value_);
	EXPECT_EQ(false, s.min.star_);
	EXPECT_EQ(23, s.hour.value_);
	EXPECT_EQ(false, s.hour.star_);
	EXPECT_EQ(0, s.dom.value_);
	EXPECT_EQ(true, s.dom.star_);
	EXPECT_EQ(12, s.mon.value_);
	EXPECT_EQ(false, s.mon.star_);
	EXPECT_EQ(6, s.dow.value_);
	EXPECT_EQ(false, s.dow.star_);

	s = cron_parser::parse("59 23 31 * 6");
	EXPECT_EQ(59, s.min.value_);
	EXPECT_EQ(false, s.min.star_);
	EXPECT_EQ(23, s.hour.value_);
	EXPECT_EQ(false, s.hour.star_);
	EXPECT_EQ(31, s.dom.value_);
	EXPECT_EQ(false, s.dom.star_);
	EXPECT_EQ(0, s.mon.value_);
	EXPECT_EQ(true, s.mon.star_);
	EXPECT_EQ(6, s.dow.value_);
	EXPECT_EQ(false, s.dow.star_);

	s = cron_parser::parse("59 23 31 12 *");
	EXPECT_EQ(59, s.min.value_);
	EXPECT_EQ(false, s.min.star_);
	EXPECT_EQ(23, s.hour.value_);
	EXPECT_EQ(false, s.hour.star_);
	EXPECT_EQ(31, s.dom.value_);
	EXPECT_EQ(false, s.dom.star_);
	EXPECT_EQ(12, s.mon.value_);
	EXPECT_EQ(false, s.mon.star_);
	EXPECT_EQ(0, s.dow.value_);
	EXPECT_EQ(true, s.dow.star_);

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
