#pragma once
#include <unicode_char.hpp>
#include <string>
#include <strEx.h>
#include <vector>

#ifdef WIN32
#include <error_impl_w32.hpp>
#else
#include <error_impl_unix.hpp>
#endif

class nscp_exception : public std::exception {
private:
	std::string error;
public:
	nscp_exception(std::string error) : error(error) {};
	~nscp_exception() throw() {}

	const char* what() const throw() {
		return error.c_str();
	}
	std::string reason() const throw() {
		return error;
	}
};

