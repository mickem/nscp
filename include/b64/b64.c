/* /////////////////////////////////////////////////////////////////////////////
 * File:        b64.c
 *
 * Purpose:     Implementation file for the b64 library
 *
 * Created:     18th October 2004
 * Updated:     15th February 2005
 *
 * Home:        http://synesis.com.au/software/
 *
 * Copyright 2004-2005, Matthew Wilson and Synesis Software
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name(s) of Matthew Wilson and Synesis Software nor the names of
 *   any contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * ////////////////////////////////////////////////////////////////////////// */

 /** \file b64.c Implementation file for the b64 library
  */

  /* /////////////////////////////////////////////////////////////////////////////
   * Version information
   */

#ifndef B64_DOCUMENTATION_SKIP_SECTION
# define B64_VER_C_B64_MAJOR    1
# define B64_VER_C_B64_MINOR    0
# define B64_VER_C_B64_REVISION 8
# define B64_VER_C_B64_EDIT     10
#endif /* !B64_DOCUMENTATION_SKIP_SECTION */

   /* /////////////////////////////////////////////////////////////////////////////
	* Includes
	*/

#include <b64/b64.h>

#include <assert.h>
#include <string.h>

	/* /////////////////////////////////////////////////////////////////////////////
	 * Constants and definitions
	 */

#ifndef B64_DOCUMENTATION_SKIP_SECTION
# define NUM_PLAIN_DATA_BYTES        (3)
# define NUM_ENCODED_DATA_BYTES      (4)
#endif /* !B64_DOCUMENTATION_SKIP_SECTION */

	 /* /////////////////////////////////////////////////////////////////////////////
	  * Warnings
	  */

#if defined(_MSC_VER) && \
    _MSC_VER < 1000
# pragma warning(disable : 4705)
#endif /* _MSC_VER < 1000 */

	  /* /////////////////////////////////////////////////////////////////////////////
	   * Data
	   */

static const char           b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const signed char    b64_indexes[] =
{
	/* 0 - 31 / 0x00 - 0x1f */
		-1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
	/* 32 - 63 / 0x20 - 0x3f */
	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, 62, -1, -1, -1, 63  /* ... , '+', ... '/' */
	,   52, 53, 54, 55, 56, 57, 58, 59  /* '0' - '7' */
	,   60, 61, -1, -1, -1, -1, -1, -1  /* '8', '9', ... */
	/* 64 - 95 / 0x40 - 0x5f */
	,   -1, 0,  1,  2,  3,  4,  5,  6   /* ..., 'A' - 'G' */
	,   7,  8,  9,  10, 11, 12, 13, 14  /* 'H' - 'O' */
	,   15, 16, 17, 18, 19, 20, 21, 22  /* 'P' - 'W' */
	,   23, 24, 25, -1, -1, -1, -1, -1  /* 'X', 'Y', 'Z', ... */
	/* 96 - 127 / 0x60 - 0x7f */
	,   -1, 26, 27, 28, 29, 30, 31, 32  /* ..., 'a' - 'g' */
	,   33, 34, 35, 36, 37, 38, 39, 40  /* 'h' - 'o' */
	,   41, 42, 43, 44, 45, 46, 47, 48  /* 'p' - 'w' */
	,   49, 50, 51, -1, -1, -1, -1, -1  /* 'x', 'y', 'z', ... */

	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1

	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1

	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1

	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
	,   -1, -1, -1, -1, -1, -1, -1, -1
};

/* /////////////////////////////////////////////////////////////////////////////
 * Helper functions
 */

 /** This function reads in 3 bytes at a time, and translates them into 4
  * characters.
  */
static size_t b64_encode_(unsigned char const *src, size_t srcSize, char *const dest, size_t destLen) {
	size_t  total = ((srcSize + (NUM_PLAIN_DATA_BYTES - 1)) / NUM_PLAIN_DATA_BYTES) * NUM_ENCODED_DATA_BYTES;

	if (NULL == dest) {
		return total;
	} else if (destLen < total) {
		return 0;
	} else {
		char    *p = dest;

		for (; NUM_PLAIN_DATA_BYTES <= srcSize; srcSize -= NUM_PLAIN_DATA_BYTES) {
			char    characters[NUM_ENCODED_DATA_BYTES];

			characters[0] = (char)((src[0] & 0xfc) >> 2);
			characters[1] = (char)(((src[0] & 0x03) << 4) + ((src[1] & 0xf0) >> 4));
			characters[2] = (char)(((src[1] & 0x0f) << 2) + ((src[2] & 0xc0) >> 6));
			characters[3] = (char)(src[2] & 0x3f);

#ifndef __WATCOMC__
			assert(characters[0] >= 0 && characters[0] < 64);
			assert(characters[1] >= 0 && characters[1] < 64);
			assert(characters[2] >= 0 && characters[2] < 64);
			assert(characters[3] >= 0 && characters[3] < 64);
#endif /* __WATCOMC__ */

			src += NUM_PLAIN_DATA_BYTES;
			*p++ = b64_chars[(unsigned char)characters[0]];
			assert(NULL != strchr(b64_chars, *(p - 1)));
			*p++ = b64_chars[(unsigned char)characters[1]];
			assert(NULL != strchr(b64_chars, *(p - 1)));
			*p++ = b64_chars[(unsigned char)characters[2]];
			assert(NULL != strchr(b64_chars, *(p - 1)));
			*p++ = b64_chars[(unsigned char)characters[3]];
			assert(NULL != strchr(b64_chars, *(p - 1)));
		}

		if (0 != srcSize) {
			/* Deal with the overspill, by boosting it up to three bytes (using 0s)
			 * and then appending '=' for any missing characters.
			 *
			 * This is done into a temporary buffer, so we can call ourselves and
			 * have the output continue to be written direct to the destination.
			 */

			unsigned char   dummy[NUM_PLAIN_DATA_BYTES];
			size_t          i;

			for (i = 0; i < srcSize; ++i) {
				dummy[i] = *src++;
			}

			for (; i < NUM_PLAIN_DATA_BYTES; ++i) {
				dummy[i] = '\0';
			}

			b64_encode_(&dummy[0], NUM_PLAIN_DATA_BYTES, p, NUM_ENCODED_DATA_BYTES);

			for (p += 1 + srcSize; srcSize++ < NUM_PLAIN_DATA_BYTES; ) {
				*p++ = '=';
			}
		}

		return total;
	}
}

/** This function reads in a character string in 4-character chunks, and writes
 * out the converted form in 3-byte chunks to the destination.
 */
static size_t b64_decode_(char const *src, size_t srcLen, unsigned char *dest, size_t destSize) {
	size_t  total = 0;
	size_t  wholeChunks = (srcLen / NUM_ENCODED_DATA_BYTES);
	size_t  remainderBytes = (srcLen % NUM_ENCODED_DATA_BYTES);

	/* The total is the sum of the size for the wholeChunks, along with the
	 * size of the remainder.
	 *
	 * The remainder is either stipulated as a whole chunk (4 chars) if the
	 * src is not specified, or it is the precise number required
	 */

	if (0 == wholeChunks ||
		0 != remainderBytes) {
		return 0;
	} else {
		assert(wholeChunks > 0);

		/* 1. Whole chunks */
		total += wholeChunks * NUM_PLAIN_DATA_BYTES;

		/* 2. Remainder */
		if (NULL != src) {
			char const  *const  lastChunk = src + srcLen;
			char const          *p = lastChunk - NUM_ENCODED_DATA_BYTES;

			for (; p != lastChunk; ++p) {
				if ('=' == *p ||
					'\0' == *p) {
					total -= (size_t)(lastChunk - p);

					break;
				}
			}
		}
	}

	if (NULL == dest) {
		return total;
	} else if (destSize < total) {
		return 0;
	} else {
		/* Three things we need to account for:
		 *
		 * 1. invalid length
		 * 2. trailing '=' characters
		 * 3. invalid characters
		 */

		size_t  i;
		char    dummy[NUM_ENCODED_DATA_BYTES];

		destSize = total;

		for (; NUM_ENCODED_DATA_BYTES < srcLen; srcLen -= NUM_ENCODED_DATA_BYTES) {
			unsigned char   bytes[NUM_PLAIN_DATA_BYTES];
			signed char     indexes[NUM_ENCODED_DATA_BYTES];

			indexes[0] = b64_indexes[(unsigned char)src[0]];
			indexes[1] = b64_indexes[(unsigned char)src[1]];
			indexes[2] = b64_indexes[(unsigned char)src[2]];
			indexes[3] = b64_indexes[(unsigned char)src[3]];

			if (indexes[0] < 0 ||
				indexes[1] < 0 ||
				indexes[2] < 0 ||
				indexes[3] < 0) {
				return 0;
			}

			bytes[0] = (unsigned char)((indexes[0] << 2) + ((indexes[1] & 0x30) >> 4));
			bytes[1] = (unsigned char)(((indexes[1] & 0xf) << 4) + ((indexes[2] & 0x3c) >> 2));
			bytes[2] = (unsigned char)(((indexes[2] & 0x3) << 6) + indexes[3]);

#if 0
			printf("0x%02x, 0x%02x, 0x%02x, 0x%02x\t=>\t", src[0], src[1], src[2], src[3]);
			printf("0x%02x, 0x%02x, 0x%02x, 0x%02x\t=>\t", indexes[0], indexes[1], indexes[2], indexes[3]);
			printf("\"%c%c%c\"\n", bytes[0], bytes[1], bytes[2]);
#endif /* 0 */

			assert(NUM_PLAIN_DATA_BYTES <= destSize);

			src += NUM_ENCODED_DATA_BYTES;
			*dest++ = bytes[0];
			*dest++ = bytes[1];
			*dest++ = bytes[2];
			destSize -= NUM_PLAIN_DATA_BYTES;
		}

		assert(NUM_ENCODED_DATA_BYTES == srcLen);

		/* Replace all '=' with '\0' */

		if ((i = 0, '=' == (dummy[i] = src[i])) ||
			(++i, '=' == (dummy[i] = src[i])) ||
			(++i, '=' == (dummy[i] = src[i])) ||
			(++i, '=' == (dummy[i] = src[i]))) {
			for (; i < NUM_ENCODED_DATA_BYTES; ++i) {
				dummy[i] = 0;
			}

			src = &dummy[0];
		}

		/* Handle the terminating case */
		{
			unsigned char   bytes[NUM_PLAIN_DATA_BYTES];
			signed char     indexes[NUM_ENCODED_DATA_BYTES];
			size_t          j;

			for (j = 0; j < NUM_ENCODED_DATA_BYTES; ++j) {
				if (0 == src[j]) {
					indexes[j] = 0;
				} else if ((indexes[j] = b64_indexes[(unsigned char)src[j]]) < 0) {
					return 0;
				}
			}

			bytes[0] = (unsigned char)((indexes[0] << 2) + ((indexes[1] & 0x30) >> 4));
			bytes[1] = (unsigned char)(((indexes[1] & 0xf) << 4) + ((indexes[2] & 0x3c) >> 2));
			bytes[2] = (unsigned char)(((indexes[2] & 0x3) << 6) + indexes[3]);

#ifdef _DEBUG
			src += NUM_ENCODED_DATA_BYTES;

			((void)src);
#endif /* _DEBUG */

			assert(destSize <= NUM_PLAIN_DATA_BYTES);

			for (j = 0; 0 < destSize--; ++j) {
				assert(j < NUM_PLAIN_DATA_BYTES);

				*dest++ = bytes[j];
			}
		}

		return total;
	}
}

/* /////////////////////////////////////////////////////////////////////////////
 * API functions
 */

size_t b64_encode(void const *src, size_t srcSize, char *dest, size_t destLen) {
	return b64_encode_((unsigned char const*)src, srcSize, dest, destLen);
}

size_t b64_decode(char const *src, size_t srcLen, void *dest, size_t destSize) {
	return b64_decode_(src, srcLen, (unsigned char*)dest, destSize);
}

/* ////////////////////////////////////////////////////////////////////////// */