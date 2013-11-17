#include <vector>
#include <string>
#include <format.hpp>

#include <gtest/gtest.h>


TEST(format, format_byte_units_units) {
	EXPECT_EQ(format::format_byte_units(0), "0B");
	EXPECT_EQ(format::format_byte_units(1), "1B");
	EXPECT_EQ(format::format_byte_units(1024), "1KB");
	EXPECT_EQ(format::format_byte_units(1024*1024), "1MB");
	EXPECT_EQ(format::format_byte_units(1024*1024*1024), "1GB");
}

TEST(format, format_byte_units_common) {
	EXPECT_EQ(format::format_byte_units(512), "512B");
	EXPECT_EQ(format::format_byte_units(999), "999B");
}

TEST(format, format_byte_units_rounding) {
	EXPECT_EQ(format::format_byte_units(0), "0B");
	EXPECT_EQ(format::format_byte_units(1000), "0.977KB");
	EXPECT_EQ(format::format_byte_units(1023), "0.999KB");
	EXPECT_EQ(format::format_byte_units(1024), "1KB");
	EXPECT_EQ(format::format_byte_units(1126), "1.1KB");
	EXPECT_EQ(format::format_byte_units(1136), "1.11KB");
}
