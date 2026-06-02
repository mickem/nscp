// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <bytes/base64.h>

#include <stddef.h>

/* Standard RFC 4648 base64 alphabet. */
static const char b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Map a single character to its 6-bit value, or -1 if it is not part of the
 * base64 alphabet. Padding ('=') and NUL are handled by the caller. */
static int b64_sextet(unsigned char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

size_t b64_encode(void const *src, size_t srcSize, char *dest, size_t destLen) {
  /* The output length is ceil(srcSize / 3) * 4. Reject inputs large enough to
   * overflow that computation before touching `src`; the cap sits far above
   * any realistic payload. */
  if (srcSize > ((size_t)-1) / 2) {
    return 0;
  }

  const size_t out_len = ((srcSize + 2) / 3) * 4;
  if (dest == NULL) {
    return out_len;
  }
  if (destLen < out_len) {
    return 0;
  }

  const unsigned char *in = (const unsigned char *)src;
  char *out = dest;
  size_t i = 0;

  /* Full 3-byte groups -> 4 characters. */
  for (; i + 3 <= srcSize; i += 3) {
    const unsigned long v = ((unsigned long)in[i] << 16) | ((unsigned long)in[i + 1] << 8) | in[i + 2];
    *out++ = b64_alphabet[(v >> 18) & 0x3f];
    *out++ = b64_alphabet[(v >> 12) & 0x3f];
    *out++ = b64_alphabet[(v >> 6) & 0x3f];
    *out++ = b64_alphabet[v & 0x3f];
  }

  /* Trailing 1 or 2 bytes, padded with '='. */
  const size_t rem = srcSize - i;
  if (rem == 1) {
    const unsigned long v = (unsigned long)in[i] << 16;
    *out++ = b64_alphabet[(v >> 18) & 0x3f];
    *out++ = b64_alphabet[(v >> 12) & 0x3f];
    *out++ = '=';
    *out++ = '=';
  } else if (rem == 2) {
    const unsigned long v = ((unsigned long)in[i] << 16) | ((unsigned long)in[i + 1] << 8);
    *out++ = b64_alphabet[(v >> 18) & 0x3f];
    *out++ = b64_alphabet[(v >> 12) & 0x3f];
    *out++ = b64_alphabet[(v >> 6) & 0x3f];
    *out++ = '=';
  }

  return out_len;
}

size_t b64_decode(char const *src, size_t srcLen, void *dest, size_t destSize) {
  const size_t whole_chunks = srcLen / 4;
  const size_t remainder = srcLen % 4;

  /* base64 is decoded in 4-character quanta; anything else is invalid. */
  if (whole_chunks == 0 || remainder != 0) {
    return 0;
  }

  size_t total = whole_chunks * 3;

  /* Trim the size for padding in the final quantum. The first '=' (or embedded
   * NUL) marks the end of the real data; every character from there to the end
   * of the chunk drops one decoded byte. Skipped when `src` is NULL: that path
   * only ever returns an upper bound. */
  if (src != NULL) {
    const char *last = src + srcLen - 4;
    size_t k;
    for (k = 0; k < 4; ++k) {
      if (last[k] == '=' || last[k] == '\0') {
        /* A quantum decodes to at most 3 bytes. A quantum that is padding from
         * its very first character (e.g. "====") contributes nothing, so clamp
         * the drop to 3: `total` is size_t and an unclamped `4 - 0` would
         * underflow it, making the size-query path report a near-SIZE_MAX
         * length and invite a huge allocation. */
        size_t drop = 4 - k;
        if (drop > 3) drop = 3;
        total -= drop;
        break;
      }
    }
  }

  if (dest == NULL) {
    return total;
  }
  if (destSize < total) {
    return 0;
  }
  /* Committing to a write means reading from `src`; refuse a NULL rather than
   * dereference it. */
  if (src == NULL) {
    return 0;
  }

  unsigned char *out = (unsigned char *)dest;
  size_t produced = 0;
  size_t c;

  for (c = 0; c < whole_chunks; ++c) {
    const char *q = src + c * 4;
    const int is_last = (c + 1 == whole_chunks);
    int v[4];
    int k;

    for (k = 0; k < 4; ++k) {
      const unsigned char ch = (unsigned char)q[k];
      if (is_last && (ch == '=' || ch == '\0')) {
        /* Padding / terminator in the final quantum decodes to zero bits; the
         * corresponding output byte is dropped via `total` below. */
        v[k] = 0;
      } else {
        const int s = b64_sextet(ch);
        if (s < 0) {
          return 0;
        }
        v[k] = s;
      }
    }

    const unsigned char triple[3] = {
        (unsigned char)((v[0] << 2) | (v[1] >> 4)),
        (unsigned char)(((v[1] & 0x0f) << 4) | (v[2] >> 2)),
        (unsigned char)(((v[2] & 0x03) << 6) | v[3]),
    };

    size_t emit = total - produced;
    if (emit > 3) {
      emit = 3;
    }
    for (k = 0; (size_t)k < emit; ++k) {
      out[produced + k] = triple[k];
    }
    produced += emit;
  }

  return total;
}
