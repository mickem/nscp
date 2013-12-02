#include <vector>
#include <string>
#include <format.hpp>

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
