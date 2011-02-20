#pragma once

#include <boost/static_assert.hpp>

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

}