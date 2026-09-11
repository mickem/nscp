// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "utf8_conversion.hpp"

namespace replxx {

namespace {

const UTF32 replacement_character = 0x0000FFFD;
const UTF32 max_code_point = 0x0010FFFF;
const UTF32 surrogate_first = 0x0000D800;
const UTF32 surrogate_last = 0x0000DFFF;

// Length in bytes of the sequence this byte introduces, or 0 when it cannot
// start one: 0x80-0xBF are continuation bytes, 0xC0/0xC1 would only ever encode
// an overlong form, and 0xF5 and up are past U+10FFFF.
int sequence_length(UTF8 lead) {
  if (lead < 0x80) {
    return 1;
  }
  if (lead < 0xC2) {
    return 0;
  }
  if (lead < 0xE0) {
    return 2;
  }
  if (lead < 0xF0) {
    return 3;
  }
  if (lead < 0xF5) {
    return 4;
  }
  return 0;
}

// The second byte of a multi-byte sequence carries the range restrictions that
// keep the encoding unambiguous; every byte after it is a plain continuation.
// The four special cases below are what rule out overlong forms (0xE0, 0xF0),
// encoded surrogates (0xED) and code points above U+10FFFF (0xF4).
bool is_valid_second(UTF8 lead, UTF8 second) {
  switch (lead) {
    case 0xE0:
      return second >= 0xA0 && second <= 0xBF;
    case 0xED:
      return second >= 0x80 && second <= 0x9F;
    case 0xF0:
      return second >= 0x90 && second <= 0xBF;
    case 0xF4:
      return second >= 0x80 && second <= 0x8F;
    default:
      return second >= 0x80 && second <= 0xBF;
  }
}

bool is_continuation(UTF8 byte) { return (byte & 0xC0) == 0x80; }

bool is_well_formed(const UTF8* source, int length) {
  if (length > 1 && !is_valid_second(source[0], source[1])) {
    return false;
  }
  for (int i = 2; i < length; ++i) {
    if (!is_continuation(source[i])) {
      return false;
    }
  }
  return true;
}

UTF32 decode(const UTF8* source, int length) {
  switch (length) {
    case 1:
      return source[0];
    case 2:
      return static_cast<UTF32>(source[0] & 0x1F) << 6 | static_cast<UTF32>(source[1] & 0x3F);
    case 3:
      return static_cast<UTF32>(source[0] & 0x0F) << 12 | static_cast<UTF32>(source[1] & 0x3F) << 6 | static_cast<UTF32>(source[2] & 0x3F);
    default:
      return static_cast<UTF32>(source[0] & 0x07) << 18 | static_cast<UTF32>(source[1] & 0x3F) << 12 | static_cast<UTF32>(source[2] & 0x3F) << 6 |
             static_cast<UTF32>(source[3] & 0x3F);
  }
}

int encoded_length(UTF32 code_point) {
  if (code_point < 0x80) {
    return 1;
  }
  if (code_point < 0x800) {
    return 2;
  }
  if (code_point < 0x10000) {
    return 3;
  }
  return 4;
}

void encode(UTF32 code_point, int length, UTF8* target) {
  switch (length) {
    case 1:
      target[0] = static_cast<UTF8>(code_point);
      break;
    case 2:
      target[0] = static_cast<UTF8>(0xC0 | (code_point >> 6));
      target[1] = static_cast<UTF8>(0x80 | (code_point & 0x3F));
      break;
    case 3:
      target[0] = static_cast<UTF8>(0xE0 | (code_point >> 12));
      target[1] = static_cast<UTF8>(0x80 | ((code_point >> 6) & 0x3F));
      target[2] = static_cast<UTF8>(0x80 | (code_point & 0x3F));
      break;
    default:
      target[0] = static_cast<UTF8>(0xF0 | (code_point >> 18));
      target[1] = static_cast<UTF8>(0x80 | ((code_point >> 12) & 0x3F));
      target[2] = static_cast<UTF8>(0x80 | ((code_point >> 6) & 0x3F));
      target[3] = static_cast<UTF8>(0x80 | (code_point & 0x3F));
      break;
  }
}

}  // namespace

ConversionResult ConvertUTF8toUTF32(const UTF8** sourceStart, const UTF8* sourceEnd, UTF32** targetStart, UTF32* targetEnd, ConversionFlags) {
  ConversionResult result = conversionOK;
  const UTF8* source = *sourceStart;
  UTF32* target = *targetStart;
  while (source < sourceEnd) {
    const int length = sequence_length(*source);
    if (length == 0) {
      result = sourceIllegal;
      break;
    }
    if (sourceEnd - source < length) {
      // Either a truncated sequence or one that simply has not arrived yet;
      // the terminal reads stdin a byte at a time and leans on this to tell it
      // to keep reading.
      result = sourceExhausted;
      break;
    }
    if (!is_well_formed(source, length)) {
      result = sourceIllegal;
      break;
    }
    if (target >= targetEnd) {
      result = targetExhausted;
      break;
    }
    *target++ = decode(source, length);
    source += length;
  }
  *sourceStart = source;
  *targetStart = target;
  return result;
}

ConversionResult ConvertUTF32toUTF8(const UTF32** sourceStart, const UTF32* sourceEnd, UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags) {
  ConversionResult result = conversionOK;
  const UTF32* source = *sourceStart;
  UTF8* target = *targetStart;
  while (source < sourceEnd) {
    UTF32 code_point = *source;
    if (flags == strictConversion && code_point >= surrogate_first && code_point <= surrogate_last) {
      result = sourceIllegal;
      break;
    }
    if (code_point > max_code_point) {
      code_point = replacement_character;
      result = sourceIllegal;
    }
    const int length = encoded_length(code_point);
    if (targetEnd - target < length) {
      result = targetExhausted;
      break;
    }
    encode(code_point, length, target);
    target += length;
    ++source;
  }
  *sourceStart = source;
  *targetStart = target;
  return result;
}

}  // namespace replxx
