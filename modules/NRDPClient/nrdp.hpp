#pragma once

#include <list>
#include <string>

#include <boost/noncopyable.hpp>

#include <NSCAPI.h>

namespace nrdp {
	struct data : boost::noncopyable {
		enum item_type_type {
			type_service,
			type_host,
			type_command
		};
		struct item_type {
			item_type_type type;
			std::string host;
			std::string service;
			NSCAPI::nagiosReturn result;
			std::string message;
			std::string perf;
		};
		std::list<item_type> items;
		void add_host(std::string host, NSCAPI::nagiosReturn result, std::string message, std::string perf);
		void add_service(std::string host, std::string service, NSCAPI::nagiosReturn result, std::string message, std::string perf);
		void add_command(std::string command, std::list<std::string> args);
		std::string render_request() const;
	};
}
