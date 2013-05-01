#include <vector>
#include <string>
#include <format.hpp>

#include <gtest/gtest.h>


TEST(format, format_byte_units_units) {
	EXPECT_EQ(format::format_byte_units(0), _T("0B"));
	EXPECT_EQ(format::format_byte_units(1), _T("1B"));
	EXPECT_EQ(format::format_byte_units(1024), _T("1K"));
	EXPECT_EQ(format::format_byte_units(1024*1024), _T("1M"));
	EXPECT_EQ(format::format_byte_units(1024*1024*1024), _T("1G"));
}

TEST(format, format_byte_units_common) {
	EXPECT_EQ(format::format_byte_units(512), _T("512B"));
	EXPECT_EQ(format::format_byte_units(999), _T("999B"));
}

TEST(format, format_byte_units_rounding) {
	EXPECT_EQ(format::format_byte_units(0), _T("0B"));
	EXPECT_EQ(format::format_byte_units(1000), _T("0.977K"));
	EXPECT_EQ(format::format_byte_units(1023), _T("0.999K"));
	EXPECT_EQ(format::format_byte_units(1024), _T("1K"));
	EXPECT_EQ(format::format_byte_units(1126), _T("1.1K"));
	EXPECT_EQ(format::format_byte_units(1136), _T("1.11K"));
}
