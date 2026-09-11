// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// UTF-8 <-> UTF-32 conversion for the vendored replxx.
//
// This replaces upstream's src/ConvertUTF.{cpp,h}, which carries Unicode,
// Inc.'s 2001-2004 notice: a grant limited to "products supporting the Unicode
// Standard" that does not allow modification, so it is not DFSG-free and keeps
// the Debian package out of main (#1278, #1294, #1515). The code below is our
// own, written from the encoding definition in the Unicode standard (Table 3-7,
// "Well-Formed UTF-8 Byte Sequences") rather than derived from that file.
//
// The entry points keep the names and signatures the replaced file used, so
// upstream's src/conversion.cxx compiles against this header unchanged, and
// they answer the same way for the same input - bar two cases where the old
// code was wrong and this one is not:
//
//  - It only bounded the second byte from above after an 0xED or 0xF4 lead, so
//    0xED 0x21 0x95 passed for well-formed and decoded to a garbage code point.
//    Here it is sourceIllegal.
//  - A lead byte no longer part of the encoding (0xF5 and up, or a 5/6 byte
//    form) that ran past the end of the input was reported as sourceExhausted,
//    inviting the caller to wait for bytes that cannot help. Here it is
//    sourceIllegal too.

#ifndef REPLXX_UTF8_CONVERSION_HPP_INCLUDED
#define REPLXX_UTF8_CONVERSION_HPP_INCLUDED 1

#include <cstdint>

namespace replxx {

typedef uint8_t UTF8;
typedef uint32_t UTF32;

// Unscoped on purpose: replxx spells the values both bare (terminal.cxx) and
// qualified (conversion.cxx).
enum ConversionResult {
  conversionOK,     // the whole input was converted
  sourceExhausted,  // the input ends in the middle of a well-formed sequence
  targetExhausted,  // the output buffer filled up before the input ran out
  sourceIllegal     // the input is malformed
};

enum ConversionFlags { strictConversion = 0, lenientConversion };

// Converts [*sourceStart, sourceEnd) into [*targetStart, targetEnd), advancing
// both start pointers past what was consumed and produced. Stops at the first
// problem and reports it; on sourceExhausted and targetExhausted the offending
// sequence is left unconsumed, so the caller can resume once it has more input
// or more room.
//
// flags is accepted for symmetry with ConvertUTF32toUTF8 and ignored: the
// sequences it would treat differently (encoded surrogates, values above
// U+10FFFF) are not well-formed UTF-8 in the first place and are rejected as
// sourceIllegal either way.
ConversionResult ConvertUTF8toUTF32(const UTF8** sourceStart, const UTF8* sourceEnd, UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags);

// The other direction. strictConversion rejects surrogate code points;
// lenientConversion encodes them as-is. A value above U+10FFFF cannot be
// encoded at all: it is replaced with U+FFFD, and the conversion carries on but
// reports sourceIllegal.
ConversionResult ConvertUTF32toUTF8(const UTF32** sourceStart, const UTF32* sourceEnd, UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags);

}  // namespace replxx

#endif
