#pragma once


namespace dll {

	class dll_exception : public std::exception {
		std::string what_;
	public:
		dll_exception(std::string what) : what_(what) {}
		~dll_exception() throw() {}
		const char* what() const throw() {
			return what_.c_str();
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
	typedef ::dll::win32::impl dll_impl;
#else
	typedef dll::iunix::impl dll_impl;
#endif
}




