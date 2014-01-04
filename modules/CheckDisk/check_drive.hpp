#include <list>
#include <vector>
#include <string>

#include <nscapi/nscapi_protobuf.hpp>


struct check_drive {
	static void check(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
};

