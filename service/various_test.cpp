#include <vector>
#include <string>
#include <format.hpp>

#include <gtest/gtest.h>


TEST(format, format_byte_units_units) {
	EXPECT_EQ(format::format_byte_units(0), "0B");
	EXPECT_EQ(format::format_byte_units(1), "1B");
	EXPECT_EQ(format::format_byte_units(1024), "1K");
	EXPECT_EQ(format::format_byte_units(1024*1024), "1M");
	EXPECT_EQ(format::format_byte_units(1024*1024*1024), "1G");
}

TEST(format, format_byte_units_common) {
	EXPECT_EQ(format::format_byte_units(512), "512B");
	EXPECT_EQ(format::format_byte_units(999), "999B");
}

TEST(format, format_byte_units_rounding) {
	EXPECT_EQ(format::format_byte_units(0), "0B");
	EXPECT_EQ(format::format_byte_units(1000), "0.977K");
	EXPECT_EQ(format::format_byte_units(1023), "0.999K");
	EXPECT_EQ(format::format_byte_units(1024), "1K");
	EXPECT_EQ(format::format_byte_units(1126), "1.1K");
	EXPECT_EQ(format::format_byte_units(1136), "1.11K");
}
