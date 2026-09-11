// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "utf8_conversion.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using replxx::ConversionResult;
using replxx::conversionOK;
using replxx::ConvertUTF32toUTF8;
using replxx::ConvertUTF8toUTF32;
using replxx::lenientConversion;
using replxx::sourceExhausted;
using replxx::sourceIllegal;
using replxx::strictConversion;
using replxx::targetExhausted;
using replxx::UTF32;
using replxx::UTF8;

namespace {

struct decoded {
  ConversionResult result;
  std::vector<UTF32> code_points;
  // Bytes left unconsumed when the conversion stopped.
  std::size_t remaining;
};

decoded decode(const std::string& bytes, std::size_t target_size = 64, replxx::ConversionFlags flags = lenientConversion) {
  std::vector<UTF32> target(target_size, 0);
  const UTF8* source = reinterpret_cast<const UTF8*>(bytes.data());
  const UTF8* source_end = source + bytes.size();
  UTF32* out = target.data();
  decoded d;
  d.result = ConvertUTF8toUTF32(&source, source_end, &out, target.data() + target_size, flags);
  d.code_points.assign(target.data(), out);
  d.remaining = static_cast<std::size_t>(source_end - source);
  return d;
}

struct encoded {
  ConversionResult result;
  std::string bytes;
  // Code points left unconsumed when the conversion stopped.
  std::size_t remaining;
};

encoded encode(const std::vector<UTF32>& code_points, std::size_t target_size = 64, replxx::ConversionFlags flags = lenientConversion) {
  std::vector<UTF8> target(target_size, 0);
  const UTF32* source = code_points.data();
  const UTF32* source_end = source + code_points.size();
  UTF8* out = target.data();
  encoded e;
  e.result = ConvertUTF32toUTF8(&source, source_end, &out, target.data() + target_size, flags);
  e.bytes.assign(reinterpret_cast<const char*>(target.data()), static_cast<std::size_t>(out - target.data()));
  e.remaining = static_cast<std::size_t>(source_end - source);
  return e;
}

// One sequence per encoded length, plus the first and last code point that
// each length is allowed to carry.
const struct {
  const char* name;
  UTF32 code_point;
  // Not NUL-terminated-safe on its own: U+0000 encodes to a single 0 byte, so
  // the length is spelled out rather than measured.
  const char* bytes;
  std::size_t length;
} k_round_trips[] = {
    {"nul", 0x000000, "\x00", 1},
    {"ascii", 0x000041, "A", 1},
    {"one byte max", 0x00007F, "\x7F", 1},
    {"two byte min", 0x000080, "\xC2\x80", 2},
    {"latin small e with acute", 0x0000E9, "\xC3\xA9", 2},
    {"two byte max", 0x0007FF, "\xDF\xBF", 2},
    {"three byte min", 0x000800, "\xE0\xA0\x80", 3},
    {"euro sign", 0x0020AC, "\xE2\x82\xAC", 3},
    {"last before surrogates", 0x00D7FF, "\xED\x9F\xBF", 3},
    {"first after surrogates", 0x00E000, "\xEE\x80\x80", 3},
    {"three byte max", 0x00FFFF, "\xEF\xBF\xBF", 3},
    {"four byte min", 0x010000, "\xF0\x90\x80\x80", 4},
    {"grinning face", 0x01F600, "\xF0\x9F\x98\x80", 4},
    {"four byte max", 0x10FFFF, "\xF4\x8F\xBF\xBF", 4},
};

TEST(utf8_conversion, decodes_every_sequence_length) {
  for (const auto& c : k_round_trips) {
    const decoded d = decode(std::string(c.bytes, c.length));
    EXPECT_EQ(conversionOK, d.result) << c.name;
    ASSERT_EQ(1u, d.code_points.size()) << c.name;
    EXPECT_EQ(c.code_point, d.code_points[0]) << c.name;
    EXPECT_EQ(0u, d.remaining) << c.name;
  }
}

TEST(utf8_conversion, encodes_every_sequence_length) {
  for (const auto& c : k_round_trips) {
    const encoded e = encode({c.code_point});
    EXPECT_EQ(conversionOK, e.result) << c.name;
    EXPECT_EQ(std::string(c.bytes, c.length), e.bytes) << c.name;
    EXPECT_EQ(0u, e.remaining) << c.name;
  }
}

TEST(utf8_conversion, decodes_a_mixed_string_in_one_go) {
  const decoded d = decode("a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80z");
  EXPECT_EQ(conversionOK, d.result);
  EXPECT_EQ(std::vector<UTF32>({0x61, 0xE9, 0x20AC, 0x1F600, 0x7A}), d.code_points);
}

TEST(utf8_conversion, round_trips_a_mixed_string) {
  const std::string original("a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80z");
  const decoded d = decode(original);
  ASSERT_EQ(conversionOK, d.result);
  const encoded e = encode(d.code_points);
  EXPECT_EQ(conversionOK, e.result);
  EXPECT_EQ(original, e.bytes);
}

// The terminal feeds stdin to the decoder one byte at a time and keys on "not
// conversionOK" to decide it needs another byte, so a well-formed prefix must
// report sourceExhausted and consume nothing.
TEST(utf8_conversion, reports_a_truncated_sequence_as_exhausted) {
  const char* const prefixes[] = {"\xC3", "\xE2", "\xE2\x82", "\xF0", "\xF0\x9F", "\xF0\x9F\x98"};
  for (const char* prefix : prefixes) {
    const std::string bytes(prefix);
    const decoded d = decode(bytes);
    EXPECT_EQ(sourceExhausted, d.result) << bytes.size() << " byte prefix";
    EXPECT_TRUE(d.code_points.empty()) << bytes.size() << " byte prefix";
    EXPECT_EQ(bytes.size(), d.remaining) << bytes.size() << " byte prefix";
  }
}

TEST(utf8_conversion, decodes_a_sequence_fed_one_byte_at_a_time) {
  const std::string emoji("\xF0\x9F\x98\x80");
  for (std::size_t len = 1; len < emoji.size(); ++len) {
    EXPECT_EQ(sourceExhausted, decode(emoji.substr(0, len)).result) << len;
  }
  const decoded d = decode(emoji);
  EXPECT_EQ(conversionOK, d.result);
  ASSERT_EQ(1u, d.code_points.size());
  EXPECT_EQ(0x1F600u, d.code_points[0]);
}

TEST(utf8_conversion, rejects_malformed_input) {
  const struct {
    const char* name;
    const char* bytes;
  } cases[] = {
      {"lone continuation byte", "\x80"},
      {"lone continuation byte at the top of the range", "\xBF"},
      {"overlong two byte nul", "\xC0\x80"},
      {"overlong two byte slash", "\xC1\xAF"},
      {"overlong three byte", "\xE0\x80\xAF"},
      {"overlong four byte", "\xF0\x80\x80\xAF"},
      {"high surrogate", "\xED\xA0\x80"},
      {"low surrogate", "\xED\xBF\xBF"},
      {"above the last code point", "\xF4\x90\x80\x80"},
      {"five byte lead", "\xF8\x88\x80\x80\x80"},
      {"lead 0xFF", "\xFF"},
      {"missing continuation byte", "\xE2\x28\xA1"},
      {"continuation byte that is a lead", "\xF0\x9F\xC3\xA9"},
      // The two the replaced implementation got wrong: its legality check
      // tested only the upper bound of the second byte after an 0xED or 0xF4
      // lead, so a byte that is not a continuation at all slipped through and
      // decoded to a garbage code point.
      {"non-continuation after an 0xED lead", "\xED\x21\x95"},
      {"non-continuation after an 0xF4 lead", "\xF4\x2B\xBA\x91"},
  };
  for (const auto& c : cases) {
    const decoded d = decode(c.bytes);
    EXPECT_EQ(sourceIllegal, d.result) << c.name;
    EXPECT_TRUE(d.code_points.empty()) << c.name;
  }
}

// Malformed input stops the conversion where it sits: whatever came before it
// is already in the target, and the source pointer is left on the bad byte.
TEST(utf8_conversion, stops_on_the_offending_byte) {
  const decoded d = decode("ok\xC0\x80more");
  EXPECT_EQ(sourceIllegal, d.result);
  EXPECT_EQ(std::vector<UTF32>({0x6F, 0x6B}), d.code_points);
  EXPECT_EQ(6u, d.remaining);
}

TEST(utf8_conversion, stops_when_the_target_is_full) {
  const decoded d = decode("abc", 2);
  EXPECT_EQ(targetExhausted, d.result);
  EXPECT_EQ(std::vector<UTF32>({0x61, 0x62}), d.code_points);
  EXPECT_EQ(1u, d.remaining);
}

// A multi-byte sequence that does not fit must be left whole for the next call
// rather than half-written.
TEST(utf8_conversion, does_not_split_a_sequence_across_a_full_target) {
  const encoded e = encode({0x61, 0x1F600}, 3);
  EXPECT_EQ(targetExhausted, e.result);
  EXPECT_EQ(std::string("a"), e.bytes);
  EXPECT_EQ(1u, e.remaining);
}

TEST(utf8_conversion, substitutes_code_points_above_the_last_one) {
  const encoded e = encode({0x61, 0x110000, 0x62});
  EXPECT_EQ(sourceIllegal, e.result);
  EXPECT_EQ(std::string("a\xEF\xBF\xBD\x62"), e.bytes);
  EXPECT_EQ(0u, e.remaining);
}

// replxx converts leniently, which keeps an unpaired surrogate (a filename or
// log line that came in as ill-formed UTF-16, say) round-tripping instead of
// dropping the line it sits on.
TEST(utf8_conversion, encodes_surrogates_only_when_lenient) {
  const encoded lenient = encode({0xD800});
  EXPECT_EQ(conversionOK, lenient.result);
  EXPECT_EQ(std::string("\xED\xA0\x80"), lenient.bytes);

  const encoded strict = encode({0x61, 0xD800}, 64, strictConversion);
  EXPECT_EQ(sourceIllegal, strict.result);
  EXPECT_EQ(std::string("a"), strict.bytes);
  EXPECT_EQ(1u, strict.remaining);
}

TEST(utf8_conversion, converts_nothing_when_there_is_nothing_to_convert) {
  const decoded d = decode("");
  EXPECT_EQ(conversionOK, d.result);
  EXPECT_TRUE(d.code_points.empty());

  const encoded e = encode({});
  EXPECT_EQ(conversionOK, e.result);
  EXPECT_TRUE(e.bytes.empty());
}

}  // namespace
