/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
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