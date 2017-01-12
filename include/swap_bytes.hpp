/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <types.hpp>

#include <boost/static_assert.hpp>
#include <boost/type_traits/is_arithmetic.hpp>

namespace swap_bytes {
	// this function swap the bytes of values given it's size as a template
	// parameter (could sizeof be used?).
	template <class T, unsigned int size>
	inline T SwapBytes(T value) {
		union {
			T value;
			char bytes[size];
		} in, out;

		in.value = value;

		for (unsigned int i = 0; i < size / 2; ++i) {
			out.bytes[i] = in.bytes[size - 1 - i];
			out.bytes[size - 1 - i] = in.bytes[i];
		}

		return out.value;
	}
#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
#endif
	template<EEndian from, EEndian to, class T>
	inline T EndianSwapBytes(T value) {
		BOOST_STATIC_ASSERT(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);
		BOOST_STATIC_ASSERT(boost::is_arithmetic<T>::value);
		if (from == to)
			return value;
		return SwapBytes<T, sizeof(T)>(value);
	}

	template<class T>
	inline T ntoh(T value) {
		return EndianSwapBytes<BIG_ENDIAN_ORDER, HOST_ENDIAN_ORDER, T >(value);
	}
	template<typename T>
	inline T hton(T value) {
		return EndianSwapBytes<HOST_ENDIAN_ORDER, BIG_ENDIAN_ORDER, T >(value);
	}
	template<class T>
	inline T ltoh(T value) {
		return EndianSwapBytes<LITTLE_ENDIAN_ORDER, HOST_ENDIAN_ORDER, T >(value);
	}
	template<typename T>
	inline T htol(T value) {
		return EndianSwapBytes<HOST_ENDIAN_ORDER, LITTLE_ENDIAN_ORDER, T >(value);
	}
#ifdef WIN32
#pragma warning( pop )
#endif
}