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
