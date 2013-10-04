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
			memcpy(data, srcdata, size);
		}
		buffer(const buffer<T,U> &other) : size_(other.size_) {
			data = new T[size_];
			memcpy(data, other.data, size_);
		}
		buffer<T,U>* operator= (const buffer<T,U> &other) {
			size_ = other.size;
			data = new T[size_];
			memcpy(data, other.data, size_);
		}
		T& operator[] (const std::size_t pos) const {
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
		U get() const {
			return reinterpret_cast<U>(data);
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