#pragma once

#include <nscapi/nscapi_protobuf.hpp>

namespace nscapi {
	namespace metrics {
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, long long value);
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, unsigned long long value);
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, std::string value);
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, double value);
	}
}
