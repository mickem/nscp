#pragma once

template <typename T>
class buffer {
	T *buffer_;
	unsigned int len_;
public:
	buffer(unsigned int len) : buffer_(NULL), len_(len) {
		buffer_ = new T[len+1];
	}
	buffer() : buffer_(NULL), len_(0) {}
	void realloc(unsigned int len) {
		T *tmp = buffer_;
		buffer_ = new T[len+1];
		memcpy(buffer_, tmp, len_);
		len_ = len;
		delete [] tmp;
	}
	~buffer() {
		delete [] buffer_;
	}
	operator T* () const {
		return buffer_;
	}
	T* unsafe_get_buffer() {
		return buffer_;
	}
	unsigned int length() {
		return len_;
	}
};
