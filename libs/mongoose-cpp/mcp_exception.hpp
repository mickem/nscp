#include "dll_defines.hpp"

#include <string>

namespace mcp
{

	class NSCAPI_EXPORT mcp_exception : public std::exception {
	private:
		std::string error;
	public:
		mcp_exception(std::string error) : error(error) {};
		~mcp_exception() throw() {}

		const char* what() const throw() {
			return error.c_str();
		}
		std::string reason() const throw() {
			return error;
		}
	};

}