/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
	EXPECT_EQ(str::format::format_byte_units(-1024 * 1024 * 1024 * 1024ULL), "16383.999PB");
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
#ifdef WIN32
	EXPECT_EQ(str::xtos(10230000.0), "1.023e+07");
	EXPECT_EQ(str::xtos(1024000000000.0), "1.024e+12");
	EXPECT_EQ(str::xtos(1024000000000000000.0), "1.024e+18");
	EXPECT_EQ(str::xtos(9223.0), "9223");
	EXPECT_EQ(str::xtos(92233720.0), "9.22337e+07");
	EXPECT_EQ(str::xtos(922337203685.0), "9.22337e+11");
	EXPECT_EQ(str::xtos(9223372036854775807.0), "9.22337e+18");
#else
	EXPECT_EQ(str::xtos(10230000.0), "1.023e+07");
	EXPECT_EQ(str::xtos(1024000000000.0), "1.024e+12");
	EXPECT_EQ(str::xtos(1024000000000000000.0), "1.024e+18");
	EXPECT_EQ(str::xtos(9223.0), "9223");
	EXPECT_EQ(str::xtos(92233720.0), "9.22337e+07");
	EXPECT_EQ(str::xtos(922337203685.0), "9.22337e+11");
	EXPECT_EQ(str::xtos(9223372036854775807.0), "9.22337e+18");
#endif
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
#ifdef WIN32
	EXPECT_EQ(str::xtos_non_sci(9223372036854775807.0), "9223372036854775808");
#else
	EXPECT_EQ(str::xtos_non_sci(9223372036854775807.0), "9223372036854775808");
#endif
}
TEST(format, strex_s__xtos_no_sci_float_1) {
	EXPECT_EQ(str::xtos_non_sci(0.339), "0.339");
	EXPECT_EQ(str::xtos_non_sci(1000.344585858585858585858585585), "1000.34458");
	EXPECT_EQ(str::xtos_non_sci(10230000.3333333333333333333333), "10230000.33333");
#ifdef WIN32
	EXPECT_EQ(str::xtos_non_sci(1024000000000.13123123123123), "1024000000000.13122");
#else
	EXPECT_EQ(str::xtos_non_sci(1024000000000.13123123123123), "1024000000000.13122");
#endif
	EXPECT_EQ(str::xtos_non_sci(1024000000000000000.13123123123123), "1024000000000000000");
	EXPECT_EQ(str::xtos_non_sci(9223.13123432423423), "9223.13123");
	EXPECT_EQ(str::xtos_non_sci(92233720.234324234234234), "92233720.23432");
	EXPECT_EQ(str::xtos_non_sci(922337203685.2423423423423), "922337203685.24231");
#ifdef WIN32
	EXPECT_EQ(str::xtos_non_sci(9223372036854775807.98798789879887), "9223372036854775808");
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
