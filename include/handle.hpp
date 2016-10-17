#pragma once

namespace hlp {
	template<class THandle, class TCloser>
	struct handle {
		THandle handle_;
		handle() : handle_(NULL) {}
		handle(THandle handle) : handle_(handle) {}
		~handle() {
			close();
		}
		void close() {
			if (handle_ != NULL)
				TCloser::close(handle_);
			handle_ = NULL;
		}
		THandle get() const {
			return handle_;
		}
		THandle* ref() {
			return &handle_;
		}
		operator THandle() const {
			return handle_;
		}
		operator bool() const {
			return handle_ != NULL;
		}
		const handle<THandle, TCloser>& operator = (const THandle &other) {
			close();
			handle_ = other;
			return *this;
		}
	};


}