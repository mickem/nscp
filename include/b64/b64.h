/* /////////////////////////////////////////////////////////////////////////////
 * File:        b64/b64.h
 *
 * Purpose:     Header file for the b64 library
 *
 * Created:     18th October 2004
 * Updated:     19th February 2005
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

/** \file b64/b64.h Header file for the b64 library
 */

#ifndef B64_INCL_B64_H_B64
#define B64_INCL_B64_H_B64

/* /////////////////////////////////////////////////////////////////////////////
 * Version information
 */

#ifndef B64_DOCUMENTATION_SKIP_SECTION
#define B64_VER_B64_H_B64_MAJOR 1
#define B64_VER_B64_H_B64_MINOR 0
#define B64_VER_B64_H_B64_REVISION 4
#define B64_VER_B64_H_B64_EDIT 9
#endif /* !B64_DOCUMENTATION_SKIP_SECTION */

/** \def B64_VER_MAJOR
 * The major version number of b64
 */

/** \def B64_VER_MINOR
 * The minor version number of b64
 */

/** \def B64_VER_REVISION
 * The revision version number of b64
 */

/** \def B64_VER
 * The current composite version number of b64
 */

#define B64_VER_MAJOR 1
#define B64_VER_MINOR 0
#define B64_VER_REVISION 2

#ifndef B64_DOCUMENTATION_SKIP_SECTION
#define B64_VER_1_0_1 0x01000100
#define B64_VER_1_0_2 0x01000200

#define B64_VER B64_VER_1_0_2
#else /* ? B64_DOCUMENTATION_SKIP_SECTION */
#define B64_VER 0x01000200
#endif /* !B64_DOCUMENTATION_SKIP_SECTION */

/* /////////////////////////////////////////////////////////////////////////////
 * Includes
 */

#include <stddef.h>

/* /////////////////////////////////////////////////////////////////////////////
 * Namespace
 */

#ifdef __cplusplus
namespace b64 {
#endif /* __cplusplus */

/* /////////////////////////////////////////////////////////////////////////////
 * Functions
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** \brief Encodes a block of binary data into base64
 *
 * \param src Pointer to the block to be encoded. May not be NULL, except when
 * \c dest is NULL, in which case it is ignored.
 * \param srcSize Length of block to be encoded
 * \param dest Pointer to the buffer into which the result is to be written. May
 * be NULL, in which case the function returns the required length
 * \param destLen Length of the buffer into which the result is to be written. Must
 * be at least as large as that indicated by the return value from
 * \c b64_encode()(NULL, srcSize, NULL, 0).
 *
 * \return 0 if the size of the buffer was insufficient, or the length of the
 * converted buffer was longer than \c destLen
 *
 * \note The function returns the required length if \c dest is NULL
 *
 * \note Threading: The function is fully re-entrant.
 */
size_t b64_encode(void const *src, size_t srcSize, char *dest, size_t destLen);

/** \brief Decodes a sequence of base64 into a block of binary data
 *
 * \param src Pointer to the base64 block to be decoded. May not be NULL, except when
 * \c dest is NULL, in which case it is ignored. If \c dest is NULL, and \c src is
 * <b>not</b> NULL, then the returned value is calculated exactly, otherwise a value
 * is returned that is guaranteed to be large enough to hold the decoded block.
 *
 * \param srcLen Length of block to be encoded. Must be an integral of 4, the base64
 * encoding quantum, otherwise the base64 block is assumed to be invalid
 * \param dest Pointer to the buffer into which the result is to be written. May
 * be NULL, in which case the function returns the required length
 * \param destSize Length of the buffer into which the result is to be written. Must
 * be at least as large as that indicated by the return value from
 * \c b64_decode(src, srcSize, NULL, 0).
 *
 * \return 0 if the size of the buffer was insufficient, or the length of the
 * converted buffer was longer than \c destSize
 *
 * \note Threading: The function is fully re-entrant.
 */
size_t b64_decode(char const *src, size_t srcLen, void *dest, size_t destSize);
// std::wstring b64_decode(std::wstring);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

/** \example C.c
 * This is an example of how to use the b64_encode() and b64_decode() functions.
 */

/* /////////////////////////////////////////////////////////////////////////////
 * Namespace
 */

#ifdef __cplusplus
} /* namespace b64 */
#endif /* __cplusplus */

/* /////////////////////////////////////////////////////////////////////////////
 * Documentation
 */

/** \mainpage What is b64?
 *
 * \section section_introduction Introduction
 *
 * b64 is a very small and simple library that provides Base-64 encoding and
 * decoding, according to <a href = "http://rfc.net/rfc1113.html">RFC-1113</a>,
 * in C and C++.
 *
 * \section section_license License
 *
 * b64 is released under the <a href = "http://www.opensource.org/licenses/bsd-license.html">BSD</a>
 * license, which basically means its free for any use, but you can't claim it's yours.
 *
 * \section section_c_api C-API
 *
 * The library is implemented in C, and provides a simple C-API, consisting of
 * the functions b64_encode() and b64_decode().
 *
 * b64_encode() encodes a binary block into a block of printable characters,
 * as follows:
 *
\htmlonly
<pre>
  int     ints[]  =   { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
  size_t  cch     =   b64_encode(&ints[0], sizeof(ints), NULL, 0);  &#x2f;&#x2a; Ask for length required &#x2a;&#x2f;
  char    *enc    =   (char*)malloc(cch);                           &#x2f;&#x2a; No error checking here ... &#x2a;&#x2f;

  b64_encode(&ints[0], sizeof(ints), enc, cch);

  printf("Converted: %.*s\n", cch, enc);
</pre>
\endhtmlonly
 *
 * b64_decode() decodes a block of printable characters into a binary block,
 * as follows:
 *
\htmlonly
<pre>
  size_t  cb      =   b64_decode(enc, cch, NULL, 0);                &#x2f;&#x2a; Ask for size required &#x2a;&#x2f;
  int     *dec    =   (int*)alloca(cb);                             &#x2f;&#x2a; No error checking here ... &#x2a;&#x2f;

  b64_decode(enc, cch, dec, cb);

  assert(0 == memcmp(&ints[0], &dec[0], sizeof(ints)));
</pre>
\endhtmlonly
 *
 * \section section_cpp_api C++ mapping
 *
 * The C++ mapping is a thin wrapper over the C-API, providing for simplified
 * client-code via manipulation of instances of the b64::cpp::string_t and
 * b64::cpp::blob_t types.
 *
 * The b64::cpp::encode() and b64::cpp::decode() functions correspond to the
 * b64::b64_encode() and b64::b64_decode() ones from the C-API. They are used
 * as follows:
\htmlonly
<pre>
  int                 ints[]  =   { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
  b64::cpp::string_t  enc =   b64::cpp::encode(&ints[0], sizeof(ints));

  std::wcout << enc << endl;

  b64::cpp::blob_t    dec =   b64::cpp::decode(enc);

  assert(0 == memcmp(&ints[0], &dec[0], sizeof(ints)));
</pre>
\endhtmlonly
 *
 * There are three overloads of the b64::b64_encode() and b64::b64_decode()
 * functions, to provide for encoding from:
 * \li a pointer (<tt>void const*</tt>) and a length
 * \li an array; note that this is only supported for compilers that support
 * discrimination between arrays and pointers
 * \li a \c blob_t instance
 *
 * and from decoding from:
 * \li a C-string (<tt>char const*</tt>) and a length
 * \li a \c string_t instance
 * \li any type for which string <a href = "http://www.cuj.com/documents/s=8681/cuj0308wilson/">Access Shims</a>
 * \c c_str_ptr() and \c c_str_len() have been defined; this includes many types within the
 * <a href = "http://stlsoft.org/">STLSoft</a> libraries
 *
 */

/* ////////////////////////////////////////////////////////////////////////// */

#endif /* B64_INCL_B64_H_B64 */

/* ////////////////////////////////////////////////////////////////////////// */
