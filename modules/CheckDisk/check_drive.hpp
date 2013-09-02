#include <list>
#include <vector>
#include <string>

#include <protobuf/plugin.pb.h>


struct check_drive {
	static void check(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
};

