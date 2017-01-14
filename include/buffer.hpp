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

namespace hlp {
	template<class T, class U=T*>
	struct buffer {
		T *data;
		std::size_t size_;
		buffer(std::size_t size) : size_(size) {
			data = new T[size];
		}
		buffer(std::size_t size, const T* srcdata) : size_(size) {
			data = new T[size];
			memcpy(data, srcdata, size*sizeof(T));
		}
		buffer(const buffer<T,U> &other) : size_(other.size_) {
			data = new T[size_];
			memcpy(data, other.data, size_*sizeof(T));
		}
		buffer<T,U>* operator= (const buffer<T,U> &other) {
			size_ = other.size;
			data = new T[size_];
			memcpy(data, other.data, size_*sizeof(T));
		}
		T& operator[] (const std::size_t pos) {
			return data[pos];
		}
		~buffer() {
			delete [] data;
		}
		operator T*() const {
			return data;
		}
		std::size_t size() const {
			return size_;
		}
		std::size_t size_in_bytes() const {
			return  size_*sizeof(T);
		}
		U get(std::size_t offset = 0) const {
			return reinterpret_cast<U>(&data[offset]);
		}
		template<class V>
		V get_t(std::size_t offset = 0) const {
			return reinterpret_cast<V>(&data[offset]);
		}
		void resize(std::size_t size) {
			size_ = size;
			delete [] data;
			data = new T[size_];
		}
	};
}
