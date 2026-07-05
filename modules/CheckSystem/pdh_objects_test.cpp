// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>
#include <tchar.h>

#include <string>
#include <type_traits>
#include <vector>
#include <win/pdh/pdh_interface.hpp>
#include <win/windows.hpp>

// ============================================================================
// pdh_error predicates
// ============================================================================

TEST(PdhError, DefaultIsOk) {
  PDH::pdh_error e;
  EXPECT_TRUE(e.is_ok());
  EXPECT_FALSE(e.is_error());
  EXPECT_EQ(e.get_message(), "");
}

TEST(PdhError, SuccessStatusIsOk) {
  PDH::pdh_error e(ERROR_SUCCESS);
  EXPECT_TRUE(e.is_ok());
  EXPECT_FALSE(e.is_error());
}

TEST(PdhError, MoreDataIsRecognized) {
  PDH::pdh_error e(PDH_MORE_DATA);
  EXPECT_TRUE(e.is_more_data());
  EXPECT_TRUE(e.is_error());
}

TEST(PdhError, InvalidDataPredicateCoversBothCodes) {
  EXPECT_TRUE(PDH::pdh_error(PDH_INVALID_DATA).is_invalid_data());
  EXPECT_TRUE(PDH::pdh_error(PDH_CSTATUS_INVALID_DATA).is_invalid_data());
  EXPECT_FALSE(PDH::pdh_error(PDH_MORE_DATA).is_invalid_data());
}

TEST(PdhError, NotFoundCoversAllThreeCodes) {
  EXPECT_TRUE(PDH::pdh_error(PDH_CSTATUS_NO_OBJECT).is_not_found());
  EXPECT_TRUE(PDH::pdh_error(PDH_CSTATUS_NO_COUNTER).is_not_found());
  EXPECT_TRUE(PDH::pdh_error(PDH_CSTATUS_BAD_COUNTERNAME).is_not_found());
  EXPECT_FALSE(PDH::pdh_error(PDH_INVALID_DATA).is_not_found());
}

TEST(PdhError, NegativeDenominatorCoversBothCodes) {
  EXPECT_TRUE(PDH::pdh_error(PDH_CALC_NEGATIVE_DENOMINATOR).is_negative_denominator());
  EXPECT_TRUE(PDH::pdh_error(PDH_CALC_NEGATIVE_VALUE).is_negative_denominator());
  EXPECT_FALSE(PDH::pdh_error(PDH_INVALID_DATA).is_negative_denominator());
}

// ============================================================================
// pdh_exception
// ============================================================================

// CERT-ERR60-CPP: any type used as an exception must have a non-throwing copy
// constructor.
static_assert(std::is_nothrow_copy_constructible<PDH::pdh_exception>::value, "pdh_exception copy constructor must be noexcept (CERT-ERR60)");

TEST(PdhException, SingleStringConstructor) {
  PDH::pdh_exception e(std::string("boom"));
  EXPECT_STREQ(e.what(), "boom");
  EXPECT_EQ(e.reason(), "boom");
}

TEST(PdhException, TwoStringConstructorJoinsWithColon) {
  PDH::pdh_exception e("counter", "missing");
  EXPECT_EQ(e.reason(), "counter: missing");
}

TEST(PdhException, StringPlusOkErrorOmitsErrorMessage) {
  PDH::pdh_exception e("context", PDH::pdh_error(ERROR_SUCCESS));
  EXPECT_EQ(e.reason(), "context");
}

TEST(PdhException, StringPlusFailedErrorAppendsErrorMessage) {
  PDH::pdh_exception e("context", PDH::pdh_error(PDH_CSTATUS_NO_OBJECT));
  // Whatever localized text PDH.DLL returns, the message starts with our prefix.
  const std::string r = e.reason();
  EXPECT_EQ(r.rfind("context:", 0), 0u) << "actual: " << r;
}

TEST(PdhException, CopyPreservesMessage) {
  PDH::pdh_exception original("orig");
  PDH::pdh_exception copy = original;
  EXPECT_EQ(copy.reason(), "orig");
}

TEST(PdhException, WhatIsNothrow) {
  PDH::pdh_exception e("boom");
  EXPECT_NO_THROW({ (void)e.what(); });
}

// ============================================================================
// pdh_object — type / flags / strategy round-trips
// ============================================================================

TEST(PdhObject, DefaultTypeIsDouble) {
  PDH::pdh_object o;
  EXPECT_EQ(o.get_type(), PDH::pdh_object::type_double);
}

TEST(PdhObject, SetTypeEnumRoundTrip) {
  PDH::pdh_object o;
  o.set_type(PDH::pdh_object::type_long);
  EXPECT_EQ(o.get_type(), PDH::pdh_object::type_long);
  o.set_type(PDH::pdh_object::type_large);
  EXPECT_EQ(o.get_type(), PDH::pdh_object::type_large);
  o.set_type(PDH::pdh_object::type_double);
  EXPECT_EQ(o.get_type(), PDH::pdh_object::type_double);
}

TEST(PdhObject, SetTypeStringRoundTrip) {
  PDH::pdh_object o;
  o.set_type(std::string("long"));
  EXPECT_EQ(o.get_type(), PDH::pdh_object::type_long);
  o.set_type(std::string("large"));
  EXPECT_EQ(o.get_type(), PDH::pdh_object::type_large);
  o.set_type(std::string("long long"));
  EXPECT_EQ(o.get_type(), PDH::pdh_object::type_large);
  o.set_type(std::string("double"));
  EXPECT_EQ(o.get_type(), PDH::pdh_object::type_double);
}

TEST(PdhObject, SetTypeRejectsUnknown) {
  PDH::pdh_object o;
  EXPECT_THROW(o.set_type(std::string("bogus")), PDH::pdh_exception);
}

TEST(PdhObject, AddFlagsKnownTokens) {
  PDH::pdh_object o;
  o.set_type(PDH::pdh_object::type_long);
  const unsigned long base = o.get_flags();
  o.add_flags("nocap100");
  EXPECT_NE(o.get_flags() & PDH_FMT_NOCAP100, 0u);

  o.add_flags("1000,noscale");
  EXPECT_NE(o.get_flags() & PDH_FMT_1000, 0u);
  EXPECT_NE(o.get_flags() & PDH_FMT_NOSCALE, 0u);

  // Type bits must not be wiped out by add_flags.
  EXPECT_EQ(o.get_type(), PDH::pdh_object::type_long);
  EXPECT_NE(o.get_flags() & base, 0u);
}

TEST(PdhObject, AddFlagsRejectsUnknown) {
  PDH::pdh_object o;
  EXPECT_THROW(o.add_flags("bogus"), PDH::pdh_exception);
}

TEST(PdhObject, AddFlagsIsAdditive) {
  // Calling add_flags twice must not clear the previously-set bit (this is
  // the contract that distinguishes add_flags from a hypothetical set_flags).
  PDH::pdh_object o;
  o.add_flags("nocap100");
  o.add_flags("1000");
  EXPECT_NE(o.get_flags() & PDH_FMT_NOCAP100, 0u);
  EXPECT_NE(o.get_flags() & PDH_FMT_1000, 0u);
}

TEST(PdhObject, SetStrategyAcceptsKnownTokens) {
  PDH::pdh_object o;
  o.set_strategy("static");
  EXPECT_TRUE(o.is_static());
  EXPECT_FALSE(o.is_rrd());

  o.set_strategy("rrd");
  EXPECT_TRUE(o.is_rrd());
  EXPECT_FALSE(o.is_static());

  PDH::pdh_object o2;
  o2.set_strategy("round robin");
  EXPECT_TRUE(o2.is_rrd());
}

TEST(PdhObject, SetStrategyEmptyMeansStatic) {
  PDH::pdh_object o;
  o.set_strategy("");
  EXPECT_TRUE(o.is_static());
}

TEST(PdhObject, SetStrategyRejectsUnknown) {
  PDH::pdh_object o;
  EXPECT_THROW(o.set_strategy("garbage"), PDH::pdh_exception);
}

TEST(PdhObject, RrdStrategySetsDefaultBufferSize) {
  PDH::pdh_object o;
  o.set_strategy("rrd");
  // Default is "60m" = 3600 seconds.
  EXPECT_EQ(o.buffer_size, 3600);
}

TEST(PdhObject, SetBufferSizeParsesTime) {
  PDH::pdh_object o;
  o.set_buffer_size("120s");
  EXPECT_EQ(o.buffer_size, 120);
  o.buffer_size = 0;
  o.set_buffer_size("2m");
  EXPECT_EQ(o.buffer_size, 120);
}

TEST(PdhObject, SetBufferSizeEmptyLeavesUnchanged) {
  PDH::pdh_object o;
  o.buffer_size = 42;
  o.set_buffer_size("");
  EXPECT_EQ(o.buffer_size, 42);
}

TEST(PdhObject, SetBufferSizeInvalidResetsToZero) {
  PDH::pdh_object o;
  o.buffer_size = 99;
  o.set_buffer_size("not-a-time");
  EXPECT_EQ(o.buffer_size, 0);
}

// ============================================================================
// pdh_object::has_instances() — the truth table
// ============================================================================

TEST(PdhObject, HasInstancesEmptyInstancesAndPlaceholderTrue) {
  PDH::pdh_object o;
  o.set_counter("\\Foo($INSTANCE$)\\Bar");
  EXPECT_TRUE(o.has_instances());
}

TEST(PdhObject, HasInstancesAutoWithPlaceholderTrue) {
  PDH::pdh_object o;
  o.set_counter("\\Foo($INSTANCE$)\\Bar");
  o.set_instances("auto");
  EXPECT_TRUE(o.has_instances());
}

TEST(PdhObject, HasInstancesTrueAlwaysTrue) {
  PDH::pdh_object o;
  o.set_counter("\\Foo(_Total)\\Bar");  // no $INSTANCE$
  o.set_instances("true");
  EXPECT_TRUE(o.has_instances());
}

TEST(PdhObject, HasInstancesFalseWithoutPlaceholder) {
  PDH::pdh_object o;
  o.set_counter("\\Foo(_Total)\\Bar");
  EXPECT_FALSE(o.has_instances());
}

TEST(PdhObject, HasInstancesFalseWhenInstancesIsFalseString) {
  PDH::pdh_object o;
  o.set_counter("\\Foo($INSTANCE$)\\Bar");
  o.set_instances("false");
  EXPECT_FALSE(o.has_instances());
}

// ============================================================================
// helpers::build_list — MULTI_SZ parsing
// ============================================================================

TEST(PdhHelpers, BuildListEmptyBufferReturnsEmpty) {
  TCHAR buffer[1] = {0};
  auto result = PDH::helpers::build_list(buffer, 0);
  EXPECT_TRUE(result.empty());
}

TEST(PdhHelpers, BuildListSingleString) {
  // MULTI_SZ: "Foo\0\0"
  const TCHAR data[] = _T("Foo");
  std::vector<TCHAR> buf(data, data + 4);  // F, o, o, \0
  buf.push_back(0);                        // terminating MULTI_SZ null
  auto result = PDH::helpers::build_list(buf.data(), static_cast<DWORD>(buf.size()));
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front(), "Foo");
}

TEST(PdhHelpers, BuildListMultipleStrings) {
  // MULTI_SZ: "Foo\0Bar\0Baz\0\0"
  const TCHAR data[] = {_T('F'), _T('o'), _T('o'), 0, _T('B'), _T('a'), _T('r'), 0, _T('B'), _T('a'), _T('z'), 0, 0};
  std::vector<TCHAR> buf(data, data + sizeof(data) / sizeof(data[0]));
  auto result = PDH::helpers::build_list(buf.data(), static_cast<DWORD>(buf.size()));
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "Foo");
  EXPECT_EQ(*it++, "Bar");
  EXPECT_EQ(*it++, "Baz");
}

TEST(PdhHelpers, BuildListNullBufferIsSafe) {
  // Bounds-safe walk must reject a null buffer rather than UAF.
  auto result = PDH::helpers::build_list(nullptr, 16);
  EXPECT_TRUE(result.empty());
}

TEST(PdhHelpers, BuildListSingleNullTerminator) {
  // bufferSize == 1 with just the terminating null — the old loop's
  // `i < bufferSize - 1` was zero so this happened to work, but the new
  // implementation handles it explicitly. Result is an empty list.
  TCHAR buffer[1] = {0};
  auto result = PDH::helpers::build_list(buffer, 1);
  EXPECT_TRUE(result.empty());
}

TEST(PdhHelpers, BuildListUnterminatedBufferDoesNotOverrun) {
  // Buffer of `Foo` with NO trailing null. A bounds-unaware walk would read
  // past the end; bounds-safe walk stops at `end`.
  const TCHAR data[] = {_T('F'), _T('o'), _T('o')};
  auto result = PDH::helpers::build_list(data, 3);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front(), "Foo");
}

// ============================================================================
// pdh_error::get_message — numeric status is always present
// ============================================================================

TEST(PdhError, GetMessageIncludesNumericStatus) {
  // get_message must include the hex status even when PDH.DLL's localized
  // text is empty (issue #255). We can't control whether FormatMessage
  // returns text in this test environment, so just assert the hex prefix.
  const PDH::pdh_error e(PDH_CSTATUS_NO_OBJECT);
  const std::string msg = e.get_message();
  EXPECT_NE(msg.find("0xC0000BB8"), std::string::npos) << "actual: " << msg;
}

TEST(PdhError, GetMessageOnSuccessIsEmpty) {
  // Success short-circuits — message is empty, not "PDH 0x00000000".
  const PDH::pdh_error e;
  EXPECT_EQ(e.get_message(), "");
}

// ============================================================================
// factory::create — strategy/type dispatch (non-instance, no PDH calls)
// ============================================================================

TEST(PdhFactoryCreate, StaticDoubleProducesInstance) {
  PDH::pdh_object o;
  o.set_counter("\\Foo\\Bar");
  o.set_alias("alias");
  o.set_strategy_static();
  o.set_type(PDH::pdh_object::type_double);
  PDH::pdh_instance inst = PDH::factory::create(o);
  ASSERT_NE(inst, nullptr);
  EXPECT_FALSE(inst->has_instances());
  EXPECT_EQ(inst->get_name(), "alias");
  EXPECT_EQ(inst->get_counter(), "\\Foo\\Bar");
}

TEST(PdhFactoryCreate, StaticLongProducesInstance) {
  PDH::pdh_object o;
  o.set_counter("\\Foo\\Bar");
  o.set_strategy_static();
  o.set_type(PDH::pdh_object::type_long);
  PDH::pdh_instance inst = PDH::factory::create(o);
  ASSERT_NE(inst, nullptr);
  EXPECT_FALSE(inst->has_instances());
}

TEST(PdhFactoryCreate, StaticLargeProducesInstance) {
  PDH::pdh_object o;
  o.set_counter("\\Foo\\Bar");
  o.set_strategy_static();
  o.set_type(PDH::pdh_object::type_large);
  PDH::pdh_instance inst = PDH::factory::create(o);
  ASSERT_NE(inst, nullptr);
  EXPECT_FALSE(inst->has_instances());
}

TEST(PdhFactoryCreate, RrdStrategyProducesInstance) {
  PDH::pdh_object o;
  o.set_counter("\\Foo\\Bar");
  o.set_strategy("rrd");
  o.set_type(PDH::pdh_object::type_double);
  PDH::pdh_instance inst = PDH::factory::create(o);
  ASSERT_NE(inst, nullptr);
  EXPECT_FALSE(inst->has_instances());
}
