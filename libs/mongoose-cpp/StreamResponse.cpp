#include "StreamResponse.h"

namespace Mongoose
{
    std::string StreamResponse::getBody() {
        return ss.str();
    }

	void StreamResponse::append(std::string data) {
		ss << data;
	}

	void StreamResponse::write(const char* buffer, std::size_t len) {
		ss.write(buffer, len);
	}
}
