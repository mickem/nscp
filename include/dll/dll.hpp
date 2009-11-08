#pragma once


namespace dll {

	class dll_exception {
		std::wstring what_;
	public:
		dll_exception(std::wstring what) : what_(what) {}
		std::wstring what() {
			return what_;
		}
	};
}

#ifdef WIN32
#include <dll/impl_w32.hpp>
#else
#include <dll/impl_unix.hpp>
#endif
namespace dll {

#ifdef WIN32
	typedef dll::win32::impl dll;
#else
	typedef dll::iunix::impl dll;
#endif
}




