#pragma once
#include <iostream>


namespace service_helper {
	class service_exception {
		std::wstring what_;
	public:
		service_exception(std::wstring what) : what_(what) {
#ifdef WIN32
			OutputDebugString((std::wstring(_T("ERROR:")) + what).c_str());
#else
			std::wcout << what << std::endl;
#endif
		}
		std::wstring what() {
			return what_;
		}
	};
}

#ifdef WIN32
#include <ServiceCmd.h>
#include <NTService.h>
#else 
#include <service/unix_service.hpp>
#endif

namespace service_helper {
	template<class T>
	class impl {
	public:
#ifdef WIN32
		typedef service_helper_impl::NTService<T> system_service;
#else
		typedef service_helper_impl::unix_service<T> system_service;
#endif
	};
}