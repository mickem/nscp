#pragma once

namespace hlp {
	template<class T>
	struct service_handle {
		T handle;
		service_handle() : handle(NULL) {}
		service_handle(T handle) : handle(handle) {}
		~service_handle() {
			close();
		}
		void close() {
			if (handle != NULL)
				CloseServiceHandle(handle);
			handle = NULL;
		}
		T get() {
			return handle;
		}
		operator T() {
			return handle;
		}
		operator bool() {
			return handle != NULL;
		}
		const service_handle<T>* operator = (const T &handle_) {
			close();
			handle = handle_;
		}
	};

	template<class T=HANDLE>
	struct generic_handle {
		T handle;
		generic_handle() : handle(NULL) {}
		generic_handle(T handle) : handle(handle) {}
		~generic_handle() {
			close();
		}
		void close() {
			if (handle != NULL)
				CloseHandle(handle);
			handle = NULL;
		}
		T get() {
			return handle;
		}
		T* ref() {
			return &handle;
		}
		operator T() {
			return handle;
		}
		operator bool() {
			return handle != NULL;
		}
		const generic_handle<T>* operator = (const T &handle_) {
			close();
			handle = handle_;
			return this;
		}
	};
}