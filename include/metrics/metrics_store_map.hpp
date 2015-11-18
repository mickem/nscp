#pragma once

#include <map>
#include <string>
#include <nscapi/nscapi_protobuf.hpp>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/mutex.hpp>

namespace metrics {

	struct metrics_store {
		typedef std::map<std::string, std::string> values_map;
		void set(const Plugin::MetricsMessage &response);
		values_map get(const std::string &filter);
	private:
		values_map values_;
		boost::timed_mutex mutex_;
	};

}
