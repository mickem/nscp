#pragma once

#include "Response.h"

#include "dll_defines.hpp"

#include <map>
#include <sstream>
#include <iostream>

/**
 * A stream response to a request
 */
namespace mcp {
	class NSCAPI_EXPORT string_response : public Mongoose::Response {
	private:
		std::string data;
		int response_code;

	public:
		string_response();
		string_response(int response_code, std::string &data);
		string_response(int &response_code, std::string data);

		virtual std::string getBody();
		virtual int get_response_code() const;
		void set(std::string data);
	};
}
