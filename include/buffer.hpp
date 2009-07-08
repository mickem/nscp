#pragma once

template <typename T>
class buffer {
	T *buffer_;
public:
	buffer(unsigned int len) : buffer_(NULL) {
		buffer_ = new T[len+1];

	}
	~buffer() {
		delete [] buffer_;
	}
	operator T* () const {
		return buffer_;
	}
};
