// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_BYTES_BASE64_H
#define NSCP_BYTES_BASE64_H

#include <stddef.h>

#ifdef __cplusplus
namespace b64 {
extern "C" {
#endif /* __cplusplus */

/**
 * Encode a block of binary data as standard (RFC 4648) base64 with '='
 * padding.
 *
 * @param src     Pointer to the bytes to encode. Ignored when @p dest is NULL.
 * @param srcSize Number of bytes to encode.
 * @param dest    Destination buffer, or NULL to query the required length.
 * @param destLen Size of @p dest.
 *
 * @return The encoded length (ceil(srcSize / 3) * 4). When @p dest is NULL this
 *         is the size of buffer the caller must provide. Returns 0 if @p dest is
 *         too small, or if @p srcSize is large enough to overflow the size
 *         calculation. Fully re-entrant.
 */
size_t b64_encode(void const *src, size_t srcSize, char *dest, size_t destLen);

/**
 * Decode a base64 sequence back into binary data.
 *
 * @param src      Pointer to the base64 characters. May be NULL only on the
 *                 size-query path (NULL @p dest), in which case an upper-bound
 *                 length is returned without scanning for padding.
 * @param srcLen   Number of characters in @p src. Must be a multiple of 4, the
 *                 base64 quantum, otherwise the input is treated as invalid.
 * @param dest     Destination buffer, or NULL to query the required length.
 * @param destSize Size of @p dest.
 *
 * @return The decoded length. When @p dest is NULL this is the size of buffer
 *         the caller must provide. Returns 0 on invalid length, an invalid
 *         character, a NULL @p src paired with a non-NULL @p dest, or a @p dest
 *         that is too small. Fully re-entrant.
 */
size_t b64_decode(char const *src, size_t srcLen, void *dest, size_t destSize);

#ifdef __cplusplus
} /* extern "C" */
} /* namespace b64 */
#endif /* __cplusplus */

#endif /* NSCP_BYTES_BASE64_H */
