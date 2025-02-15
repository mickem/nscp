/* /////////////////////////////////////////////////////////////////////////////
 * File:        b64/cpp/b64.hpp
 *
 * Purpose:     Header file for the b64 C++ mapping
 *
 * Created:     18th October 2004
 * Updated:     16th February 2005
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

/** \file b64/cpp/b64.hpp Header file for the b64 C++ mapping
 */

#ifndef B64_INCL_B64_CPP_HPP_B64
#define B64_INCL_B64_CPP_HPP_B64

/* /////////////////////////////////////////////////////////////////////////////
 * Version information
 */

#ifndef B64_DOCUMENTATION_SKIP_SECTION
#define B64_VER_B64_CPP_HPP_B64_MAJOR 1
#define B64_VER_B64_CPP_HPP_B64_MINOR 1
#define B64_VER_B64_CPP_HPP_B64_REVISION 3
#define B64_VER_B64_CPP_HPP_B64_EDIT 8
#endif /* !B64_DOCUMENTATION_SKIP_SECTION */

/* /////////////////////////////////////////////////////////////////////////////
 * Includes
 */

#include <b64/b64.h>

#include <stlsoft.h>

#if defined(B64_USE_CUSTOM_STRING)
#include B64_CUSTOM_STRING_INCLUDE
#else /* B64_USE_CUSTOM_STRING */
#include <string>
#endif /* !B64_USE_CUSTOM_STRING */

#if defined(B64_USE_CUSTOM_VECTOR)
#include B64_CUSTOM_VECTOR_INCLUDE
#else /* B64_USE_CUSTOM_VECTOR */
#include <vector>
#endif /* !B64_USE_CUSTOM_VECTOR */

#if !defined(B64_STRING_TYPE_IS_CONTIGUOUS)
#include <stlsoft_auto_buffer.h>
#include <stlsoft_new_allocator.h>
#endif /* !B64_STRING_TYPE_IS_CONTIGUOUS */

#include <stlsoft_string_access.h>

/* /////////////////////////////////////////////////////////////////////////////
 * Namespace
 */

namespace b64 {

namespace cpp {

/* /////////////////////////////////////////////////////////////////////////////
 * Typedefs
 */

/** The string type for the b64::cpp namespace
 *
 * \note This defaults to <tt>::std::string</tt>. It is possible to override this, using
 * preprocessor symbol definitions. To do this, you must define
 * the symbol \c B64_USE_CUSTOM_STRING to instruct the compiler that the <b>b64</b>
 * library is to use a custom string type. In that case you <b>must also</b>
 * define the \c B64_CUSTOM_STRING_INCLUDE and \c B64_CUSTOM_STRING_TYPE and symbols.
 *
 * \c B64_CUSTOM_STRING_INCLUDE specifies the \c <> or \c "" surrounded include file name, e.g.
 *
 * <tt>&nbsp;&nbsp;\#define B64_CUSTOM_STRING_INCLUDE <stlsoft/simple_string.hpp></tt>
 *
 * \c B64_CUSTOM_STRING_TYPE specifies the string type, e.g.
 *
 * <tt>&nbsp;&nbsp;\#define B64_CUSTOM_STRING_TYPE&nbsp;&nbsp;&nbsp;&nbsp;::stlsoft::basic_simple_string<char></tt>
 */
#if defined(B64_USE_CUSTOM_STRING)
typedef B64_CUSTOM_STRING_TYPE string_t;
#else  /* B64_USE_CUSTOM_STRING */
typedef std::string string_t;
#endif /* !B64_USE_CUSTOM_STRING */

/** The blob type for the b64::cpp namespace
 *
 * \note This defaults to <tt>::std::vector<::stlsoft::byte_t></tt>. It is possible to
 * override this, using
 * preprocessor symbol definitions. To do this, you must define
 * the symbol \c B64_USE_CUSTOM_VECTOR to instruct the compiler that the <b>b64</b>
 * library is to use a custom string type. In that case you <b>must also</b>
 * define the \c B64_CUSTOM_VECTOR_INCLUDE and \c B64_CUSTOM_BLOB_TYPE and symbols.
 *
 * \c B64_CUSTOM_VECTOR_INCLUDE specifies the \c <> or \c "" surrounded include file name, e.g.
 *
 * <tt>&nbsp;&nbsp;\#define B64_CUSTOM_VECTOR_INCLUDE <stlsoft/pod_vector.hpp></tt>
 *
 * \c B64_CUSTOM_BLOB_TYPE specifies the string type, e.g.
 *
 * <tt>&nbsp;&nbsp;\#define B64_CUSTOM_BLOB_TYPE&nbsp;&nbsp;&nbsp;&nbsp;::stlsoft::pod_vector<unsigned char></tt>
 */
#if defined(B64_USE_CUSTOM_VECTOR)
typedef B64_CUSTOM_BLOB_TYPE blob_t;
#else /* B64_USE_CUSTOM_VECTOR */
#ifndef B64_DOCUMENTATION_SKIP_SECTION
typedef ::stlsoft::byte_t byte_t_;
typedef std::vector<byte_t_> blob_t;
#else  /* !B64_DOCUMENTATION_SKIP_SECTION */
typedef std::vector< ::stlsoft::byte_t> blob_t;
#endif /* !B64_DOCUMENTATION_SKIP_SECTION */
#endif /* !B64_USE_CUSTOM_VECTOR */

/* /////////////////////////////////////////////////////////////////////////////
 * Functions
 */

/** \brief Encodes the given block of memory into base-64.
 *
 * This function takes a pointer to a memory block to be encoded, and a number of
 * bytes to be encoded, and carries out a base-64 encoding on it, returning the
 * results in an instance of the string type \link #string_t string_t \endlink. See the
 * \ref section_cpp_api "example" from the main page
 *
 * \param src Pointer to the block to be encoded
 * \param srcSize Number of bytes in the block to be encoded
 *
 * \return The string form of the block
 *
 * \note There is no error return. If insufficient memory can be allocated, an
 * instance of \c std::bad_alloc will be thrown
 *
 * \note Threading: The function is fully re-entrant, assuming that the heap for the
 * system is re-entrant.
 *
 * \note Exceptions: Provides the strong guarantee, assuming that the constructor for
 * the string type (\c string_t) does so.
 */
inline string_t encode(void const *src, size_t srcSize) {
  size_t n = ::b64::b64_encode(src, srcSize, NULL, 0);

#ifdef B64_STRING_TYPE_IS_CONTIGUOUS
  string_t s(n, '~');  // ~ is used for an invalid / eyecatcher

  ::b64::b64_encode(src, srcSize, &s[0], s.length());
#else  /* ? B64_STRING_TYPE_IS_CONTIGUOUS */

  typedef ::stlsoft::auto_buffer<char, ::stlsoft::new_allocator<char>, 256> buffer_t;

  buffer_t buffer(n);

  ::b64::b64_encode(src, srcSize, &buffer[0], buffer.size());

  string_t s(&buffer[0], buffer.size());
#endif /* B64_STRING_TYPE_IS_CONTIGUOUS */

  return s;
}

#ifdef __STLSOFT_CF_STATIC_ARRAY_SIZE_DETERMINATION_SUPPORT
/** \brief Encodes the given array into base-64
 *
 * \param ar The array whose contents are to be encoded
 *
 * \return The string form of the block
 *
 * \note This function is only defined for compilers that are able to discriminate
 * between pointers and arrays. See Chapter 14 of <a href = "http://imperfectcplusplus.com/">Imperfect C++</a>
 * for details about this facility, and consult your <a href = "http://stlsoft.org/">STLSoft</a> header files
 * to find out to which compilers this applies.
 *
 * \note There is no error return. If insufficient memory can be allocated, an
 * instance of \c std::bad_alloc will be thrown
 *
 * \note Threading: The function is fully re-entrant, assuming that the heap for the
 * system is re-entrant.
 *
 * \note Exceptions: Provides the strong guarantee, assuming that the constructor for
 * the string type (\c string_t) does so.
 */
template <typename T, size_t N>
inline string_t encode(T (&ar)[N]) {
  return encode(&ar[0], sizeof(T) * N);
}
#endif /* __STLSOFT_CF_STATIC_ARRAY_SIZE_DETERMINATION_SUPPORT */

/** \brief Encodes the given blob into base-64
 *
 * \param blob The blob whose contents are to be encoded
 *
 * \return The string form of the block
 *
 * \note There is no error return. If insufficient memory can be allocated, an
 * instance of \c std::bad_alloc will be thrown
 *
 * \note Threading: The function is fully re-entrant, assuming that the heap for the
 * system is re-entrant.
 *
 * \note Exceptions: Provides the strong guarantee, assuming that the constructor for
 * the string type (\c string_t) does so.
 */
inline string_t encode(blob_t const &blob) { return encode(&blob[0], blob.size()); }

/** \brief Decodes the given base-64 block into binary
 *
 * \param src Pointer to the block to be decoded
 * \param srcLen Number of characters in the block to be decoded
 *
 * \return The binary form of the block, as a \c blob_t
 *
 * \note There is no error return. If insufficient memory can be allocated, an
 * instance of \c std::bad_alloc will be thrown
 *
 * \note Threading: The function is fully re-entrant, assuming that the heap for the
 * system is re-entrant.
 *
 * \note Exceptions: Provides the strong guarantee, assuming that the constructor for
 * the string type (\c string_t) does so.
 */
inline blob_t decode(char const *src, size_t srcLen) {
  size_t n = ::b64::b64_decode(src, srcLen, NULL, 0);
  blob_t v(n);

  ::b64::b64_decode(src, srcLen, &v[0], v.size());

  return v;
}

/** \brief Decodes the given string from base-64 into binary
 *
 * \param str The string whose contents are to be decoded
 *
 * \return The binary form of the block, as a \c blob_t
 *
 * \note There is no error return. If insufficient memory can be allocated, an
 * instance of \c std::bad_alloc will be thrown
 *
 * \note Threading: The function is fully re-entrant, assuming that the heap for the
 * system is re-entrant.
 *
 * \note Exceptions: Provides the strong guarantee, assuming that the constructor for
 * the string type (\c string_t) does so.
 */
template <class S>
inline blob_t decode(S const &str) {
  return decode(stlsoft_ns_qual(c_str_ptr)(str), stlsoft_ns_qual(c_str_size)(str));
}

/** \brief Decodes the given string from base-64 into binary
 *
 * \param str The string whose contents are to be decoded
 *
 * \return The binary form of the block, as a \c blob_t
 *
 * \note There is no error return. If insufficient memory can be allocated, an
 * instance of \c std::bad_alloc will be thrown
 *
 * \note Threading: The function is fully re-entrant, assuming that the heap for the
 * system is re-entrant.
 *
 * \note Exceptions: Provides the strong guarantee, assuming that the constructor for
 * the string type (\c string_t) does so.
 */
inline blob_t decode(string_t const &str) { return decode(stlsoft_ns_qual(c_str_ptr)(str), stlsoft_ns_qual(c_str_size)(str)); }

/** \example Cpp.cpp
 * This is an example of how to use the b64::encode() and b64::decode() functions.
 */

/* /////////////////////////////////////////////////////////////////////////////
 * Namespace
 */

} /* namespace cpp */

} /* namespace b64 */

/* ////////////////////////////////////////////////////////////////////////// */

#endif /* B64_INCL_B64_CPP_HPP_B64 */

/* ////////////////////////////////////////////////////////////////////////// */
