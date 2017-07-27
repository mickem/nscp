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

#include <str/format.hpp>
#include <str/utils.hpp>

#include <vector>
#include <string>

#include <gtest/gtest.h>

TEST(format, format_byte_units_units) {
	EXPECT_EQ(str::format::format_byte_units(0LL), "0B");
	EXPECT_EQ(str::format::format_byte_units(1LL), "1B");
	EXPECT_EQ(str::format::format_byte_units(1024LL), "1KB");
	EXPECT_EQ(str::format::format_byte_units(1024 * 1024LL), "1MB");
	EXPECT_EQ(str::format::format_byte_units(1024 * 1024 * 1024LL), "1GB");
	EXPECT_EQ(str::format::format_byte_units(1024 * 1024 * 1024 * 1024LL), "1TB");
	EXPECT_EQ(str::format::format_byte_units(-76100000000LL), "-70.874GB");
	EXPECT_EQ(str::format::format_byte_units(9223372036854775807LL), "8192PB");
	EXPECT_EQ(str::format::format_byte_units(-1LL), "-1B");
	EXPECT_EQ(str::format::format_byte_units(-1024LL), "-1KB");
	EXPECT_EQ(str::format::format_byte_units(-1024 * 1024LL), "-1MB");
	EXPECT_EQ(str::format::format_byte_units(-1024 * 1024 * 1024LL), "-1GB");
	EXPECT_EQ(str::format::format_byte_units(-1024 * 1024 * 1024 * 1024LL), "-1TB");

	EXPECT_EQ(str::format::format_byte_units(0ULL), "0B");
	EXPECT_EQ(str::format::format_byte_units(1ULL), "1B");
	EXPECT_EQ(str::format::format_byte_units(1024ULL), "1KB");
	EXPECT_EQ(str::format::format_byte_units(1024 * 1024ULL), "1MB");
	EXPECT_EQ(str::format::format_byte_units(1024 * 1024 * 1024ULL), "1GB");
	EXPECT_EQ(str::format::format_byte_units(1024 * 1024 * 1024 * 1024ULL), "1TB");
	EXPECT_EQ(str::format::format_byte_units(-76100000000ULL), "16384PB");
	EXPECT_EQ(str::format::format_byte_units(9223372036854775807ULL), "8192PB");

	EXPECT_EQ(str::format::format_byte_units(-1ULL), "16384PB");
	EXPECT_EQ(str::format::format_byte_units(-1024ULL), "16384PB");
	EXPECT_EQ(str::format::format_byte_units(-1024ULL * 1024ULL * 1024ULL * 1024ULL), "16383.999PB");
}

TEST(format, format_byte_units_common) {
	EXPECT_EQ(str::format::format_byte_units(512LL), "512B");
	EXPECT_EQ(str::format::format_byte_units(999LL), "999B");
}

TEST(format, format_byte_units_rounding) {
	EXPECT_EQ(str::format::format_byte_units(0LL), "0B");
	EXPECT_EQ(str::format::format_byte_units(1000LL), "0.977KB");
	EXPECT_EQ(str::format::format_byte_units(1023LL), "0.999KB");
	EXPECT_EQ(str::format::format_byte_units(1024LL), "1KB");
	EXPECT_EQ(str::format::format_byte_units(1126LL), "1.1KB");
	EXPECT_EQ(str::format::format_byte_units(1136LL), "1.109KB");
}
TEST(format, strex_s__xtos_non_sci_int) {
	EXPECT_EQ(str::xtos_non_sci(0LL), "0");
	EXPECT_EQ(str::xtos_non_sci(1000LL), "1000");
	EXPECT_EQ(str::xtos_non_sci(10230000LL), "10230000");
	EXPECT_EQ(str::xtos_non_sci(1024000000000LL), "1024000000000");
	EXPECT_EQ(str::xtos_non_sci(1024000000000000000ULL), "1024000000000000000");
	EXPECT_EQ(str::xtos_non_sci(9223ULL), "9223");
	EXPECT_EQ(str::xtos_non_sci(92233720ULL), "92233720");
	EXPECT_EQ(str::xtos_non_sci(922337203685ULL), "922337203685");
	EXPECT_EQ(str::xtos_non_sci(9223372036854775807ULL), "9223372036854775807");
}
TEST(format, strex_s__xtos_float) {
	EXPECT_EQ(str::xtos(0.0), "0");
	EXPECT_EQ(str::xtos(1000.0), "1000");
#if (_MSC_VER == 1700)
#define ESUF "e+0"
#else
#define ESUF "e+"
#endif
	EXPECT_EQ(str::xtos(10230000.0), "1.023" ESUF "07");
	EXPECT_EQ(str::xtos(1024000000000.0), "1.024" ESUF "12");
	EXPECT_EQ(str::xtos(1024000000000000000.0), "1.024" ESUF "18");
	EXPECT_EQ(str::xtos(9223.0), "9223");
	EXPECT_EQ(str::xtos(92233720.0), "9.22337" ESUF "07");
	EXPECT_EQ(str::xtos(922337203685.0), "9.22337" ESUF "11");
	EXPECT_EQ(str::xtos(9223372036854775807.0), "9.22337" ESUF "18");
}

TEST(format, strex_s__xtos_no_sci_int) {
	EXPECT_EQ(str::xtos_non_sci(0LL), "0");
	EXPECT_EQ(str::xtos_non_sci(1000LL), "1000");
	EXPECT_EQ(str::xtos_non_sci(10230000LL), "10230000");
	EXPECT_EQ(str::xtos_non_sci(1024000000000LL), "1024000000000");
	EXPECT_EQ(str::xtos_non_sci(1024000000000000000ULL), "1024000000000000000");
	EXPECT_EQ(str::xtos_non_sci(9223ULL), "9223");
	EXPECT_EQ(str::xtos_non_sci(92233720ULL), "92233720");
	EXPECT_EQ(str::xtos_non_sci(922337203685ULL), "922337203685");
	EXPECT_EQ(str::xtos_non_sci(9223372036854775807ULL), "9223372036854775807");
}
TEST(format, strex_s__xtos_no_sci_float_0) {
	EXPECT_EQ(str::xtos_non_sci(0.0), "0");
	EXPECT_EQ(str::xtos_non_sci(1000.0), "1000");
	EXPECT_EQ(str::xtos_non_sci(10230000.0), "10230000");
	EXPECT_EQ(str::xtos_non_sci(1024000000000.0), "1024000000000");
	EXPECT_EQ(str::xtos_non_sci(1024000000000000000.0), "1024000000000000000");
	EXPECT_EQ(str::xtos_non_sci(9223.0), "9223");
	EXPECT_EQ(str::xtos_non_sci(92233720.0), "92233720");
	EXPECT_EQ(str::xtos_non_sci(922337203685.0), "922337203685");
#if (_MSC_VER == 1700)
	EXPECT_EQ(str::xtos_non_sci(9223372036854775807.0), "9223372036854775800");
#else
	EXPECT_EQ(str::xtos_non_sci(9223372036854775807.0), "9223372036854775808");
#endif
}
TEST(format, strex_s__xtos_no_sci_float_1) {
	EXPECT_EQ(str::xtos_non_sci(0.339), "0.339");
	EXPECT_EQ(str::xtos_non_sci(1000.344585858585858585858585585), "1000.34458");
	EXPECT_EQ(str::xtos_non_sci(10230000.3333333333333333333333), "10230000.33333");
#if (_MSC_VER == 1700)
	EXPECT_EQ(str::xtos_non_sci(1024000000000.13123123123123), "1024000000000.1312");
#else
	EXPECT_EQ(str::xtos_non_sci(1024000000000.13123123123123), "1024000000000.13122");
#endif
	EXPECT_EQ(str::xtos_non_sci(1024000000000000000.13123123123123), "1024000000000000000");
	EXPECT_EQ(str::xtos_non_sci(9223.13123432423423), "9223.13123");
	EXPECT_EQ(str::xtos_non_sci(92233720.234324234234234), "92233720.23432");
	EXPECT_EQ(str::xtos_non_sci(922337203685.2423423423423), "922337203685.24231");
#if (_MSC_VER == 1700)
	EXPECT_EQ(str::xtos_non_sci(9223372036854775807.98798789879887), "9223372036854775800");
#else
	EXPECT_EQ(str::xtos_non_sci(9223372036854775807.98798789879887), "9223372036854775808");
#endif
}


TEST(format, itos_as_time) {
	EXPECT_EQ(str::format::itos_as_time(12345), "12s");
	EXPECT_EQ(str::format::itos_as_time(1234512), "0:20");
	EXPECT_EQ(str::format::itos_as_time(123451234), "1d 10:17");
	EXPECT_EQ(str::format::itos_as_time(1234512345), "2w 0d 06:55");
	EXPECT_EQ(str::format::itos_as_time(12345123456), "20w 2d 21:12");
}


TEST(format, format_date) {
	boost::posix_time::ptime time(boost::gregorian::date(2002, 3, 4), boost::posix_time::time_duration(5, 6, 7));
	EXPECT_EQ(str::format::format_date(time), "2002-03-04 05:06:07");
}
