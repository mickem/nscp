#include "string_response.hpp"

namespace mcp {

	string_response::string_response()
		: response_code(0) {}
	string_response::string_response(int response_code, std::string &data)
		: response_code(response_code)
		, data(data) {}
	string_response::string_response(int &response_code, std::string data)
		: response_code(response_code)
		, data(data) {}

	std::string string_response::getBody() {
		return data;
	}

	int string_response::get_response_code() const {
		return response_code;
	}


	void string_response::set(std::string new_data) {
		data = new_data;
	}

}


