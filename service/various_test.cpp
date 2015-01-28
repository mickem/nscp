#include <vector>
#include <string>
#include <format.hpp>
#include <strEx.h>

#include <gtest/gtest.h>


TEST(format, format_byte_units_units) {
	EXPECT_EQ(format::format_byte_units(0LL), "0B");
	EXPECT_EQ(format::format_byte_units(1LL), "1B");
	EXPECT_EQ(format::format_byte_units(1024LL), "1KB");
	EXPECT_EQ(format::format_byte_units(1024*1024LL), "1MB");
	EXPECT_EQ(format::format_byte_units(1024*1024*1024LL), "1GB");
	EXPECT_EQ(format::format_byte_units(1024*1024*1024*1024LL), "1TB");
	EXPECT_EQ(format::format_byte_units(-76100000000LL), "-70.874GB");
	EXPECT_EQ(format::format_byte_units(9223372036854775807LL), "8192PB");
	EXPECT_EQ(format::format_byte_units(-1LL), "-1B");
	EXPECT_EQ(format::format_byte_units(-1024LL), "-1KB");
	EXPECT_EQ(format::format_byte_units(-1024*1024LL), "-1MB");
	EXPECT_EQ(format::format_byte_units(-1024*1024*1024LL), "-1GB");
	EXPECT_EQ(format::format_byte_units(-1024*1024*1024*1024LL), "-1TB");

	EXPECT_EQ(format::format_byte_units(0ULL), "0B");
	EXPECT_EQ(format::format_byte_units(1ULL), "1B");
	EXPECT_EQ(format::format_byte_units(1024ULL), "1KB");
	EXPECT_EQ(format::format_byte_units(1024*1024ULL), "1MB");
	EXPECT_EQ(format::format_byte_units(1024*1024*1024ULL), "1GB");
	EXPECT_EQ(format::format_byte_units(1024*1024*1024*1024ULL), "1TB");
	EXPECT_EQ(format::format_byte_units(-76100000000ULL), "16384PB");
	EXPECT_EQ(format::format_byte_units(9223372036854775807ULL), "8192PB");

	EXPECT_EQ(format::format_byte_units(-1ULL), "16384PB");
	EXPECT_EQ(format::format_byte_units(-1024ULL), "16384PB");
	EXPECT_EQ(format::format_byte_units(-1024*1024*1024*1024ULL), "16383.999PB");

}

TEST(format, format_byte_units_common) {
	EXPECT_EQ(format::format_byte_units(512LL), "512B");
	EXPECT_EQ(format::format_byte_units(999LL), "999B");
}

TEST(format, format_byte_units_rounding) {
	EXPECT_EQ(format::format_byte_units(0LL), "0B");
	EXPECT_EQ(format::format_byte_units(1000LL), "0.977KB");
	EXPECT_EQ(format::format_byte_units(1023LL), "0.999KB");
	EXPECT_EQ(format::format_byte_units(1024LL), "1KB");
	EXPECT_EQ(format::format_byte_units(1126LL), "1.1KB");
	EXPECT_EQ(format::format_byte_units(1136LL), "1.109KB");
}
TEST(format, strex_s__xtos_int) {
	EXPECT_EQ(strEx::s::xtos(0LL), "0");
	EXPECT_EQ(strEx::s::xtos(1000LL), "1000");
	EXPECT_EQ(strEx::s::xtos(10230000LL), "10230000");
	EXPECT_EQ(strEx::s::xtos(1024000000000LL), "1024000000000");
	EXPECT_EQ(strEx::s::xtos(1024000000000000000ULL), "1024000000000000000");
	EXPECT_EQ(strEx::s::xtos(9223ULL), "9223");
	EXPECT_EQ(strEx::s::xtos(92233720ULL), "92233720");
	EXPECT_EQ(strEx::s::xtos(922337203685ULL), "922337203685");
	EXPECT_EQ(strEx::s::xtos(9223372036854775807ULL), "9223372036854775807");
}
TEST(format, strex_s__xtos_float) {
	EXPECT_EQ(strEx::s::xtos(0.0), "0");
	EXPECT_EQ(strEx::s::xtos(1000.0), "1000");
	EXPECT_EQ(strEx::s::xtos(10230000.0), "1.023e+007");
	EXPECT_EQ(strEx::s::xtos(1024000000000.0), "1.024e+012");
	EXPECT_EQ(strEx::s::xtos(1024000000000000000.0), "1.024e+018");
	EXPECT_EQ(strEx::s::xtos(9223.0), "9223");
	EXPECT_EQ(strEx::s::xtos(92233720.0), "9.22337e+007");
	EXPECT_EQ(strEx::s::xtos(922337203685.0), "9.22337e+011");
	EXPECT_EQ(strEx::s::xtos(9223372036854775807.0), "9.22337e+018");
}


TEST(format, strex_s__xtos_no_sci_int) {
	EXPECT_EQ(strEx::s::xtos_non_sci(0LL), "0");
	EXPECT_EQ(strEx::s::xtos_non_sci(1000LL), "1000");
	EXPECT_EQ(strEx::s::xtos_non_sci(10230000LL), "10230000");
	EXPECT_EQ(strEx::s::xtos_non_sci(1024000000000LL), "1024000000000");
	EXPECT_EQ(strEx::s::xtos_non_sci(1024000000000000000ULL), "1024000000000000000");
	EXPECT_EQ(strEx::s::xtos_non_sci(9223ULL), "9223");
	EXPECT_EQ(strEx::s::xtos_non_sci(92233720ULL), "92233720");
	EXPECT_EQ(strEx::s::xtos_non_sci(922337203685ULL), "922337203685");
	EXPECT_EQ(strEx::s::xtos_non_sci(9223372036854775807ULL), "9223372036854775807");
}
TEST(format, strex_s__xtos_no_sci_float_0) {
	EXPECT_EQ(strEx::s::xtos_non_sci(0.0), "0");
	EXPECT_EQ(strEx::s::xtos_non_sci(1000.0), "1000");
	EXPECT_EQ(strEx::s::xtos_non_sci(10230000.0), "10230000");
	EXPECT_EQ(strEx::s::xtos_non_sci(1024000000000.0), "1024000000000");
	EXPECT_EQ(strEx::s::xtos_non_sci(1024000000000000000.0), "1024000000000000000");
	EXPECT_EQ(strEx::s::xtos_non_sci(9223.0), "9223");
	EXPECT_EQ(strEx::s::xtos_non_sci(92233720.0), "92233720");
	EXPECT_EQ(strEx::s::xtos_non_sci(922337203685.0), "922337203685");
	EXPECT_EQ(strEx::s::xtos_non_sci(9223372036854775807.0), "9223372036854775800");
}
TEST(format, strex_s__xtos_no_sci_float_1) {
	EXPECT_EQ(strEx::s::xtos_non_sci(0.339), "0.339");
	EXPECT_EQ(strEx::s::xtos_non_sci(1000.344585858585858585858585585), "1000.34458");
	EXPECT_EQ(strEx::s::xtos_non_sci(10230000.3333333333333333333333), "10230000.33333");
	EXPECT_EQ(strEx::s::xtos_non_sci(1024000000000.13123123123123), "1024000000000.1312");
	EXPECT_EQ(strEx::s::xtos_non_sci(1024000000000000000.13123123123123), "1024000000000000000");
	EXPECT_EQ(strEx::s::xtos_non_sci(9223.13123432423423), "9223.13123");
	EXPECT_EQ(strEx::s::xtos_non_sci(92233720.234324234234234), "92233720.23432");
	EXPECT_EQ(strEx::s::xtos_non_sci(922337203685.2423423423423), "922337203685.24231");
	EXPECT_EQ(strEx::s::xtos_non_sci(9223372036854775807.98798789879887), "9223372036854775800");
}
