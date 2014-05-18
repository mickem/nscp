#pragma once
#include <iostream>
#ifdef _WIN32
#include <utf8.hpp>
#endif

namespace service_helper {
	class service_exception {
		std::string what_;
	public:
		service_exception(std::string what) : what_(what) {
#ifdef _WIN32
			OutputDebugString(utf8::cvt<std::wstring>(std::string("ERROR:") + what).c_str());
#else
			std::cout << what << std::endl;
#endif
		}
		std::string what() {
			return what_;
		}
	};
}

#ifdef _WIN32
#include <service/win32_service.hpp>
#else 
#include <service/unix_service.hpp>
#endif

namespace service_helper {
	template<class T>
	class impl {
	public:
#ifdef _WIN32
		typedef service_helper_impl::win32_service<T> system_service;
#else
		typedef service_helper_impl::unix_service<T> system_service;
#endif
	};
}