#pragma once

#include <nscapi/nscapi_protobuf.hpp>

namespace nscapi {
	namespace metrics {
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, long long value) {
			Plugin::Common::Metric *m = b->add_value();
			m->set_key(key);
			m->mutable_value()->set_int_data(value);
		}
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, unsigned long long value) {
			Plugin::Common::Metric *m = b->add_value();
			m->set_key(key);
			m->mutable_value()->set_int_data(value);
		}
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, std::string value) {
			Plugin::Common::Metric *m = b->add_value();
			m->set_key(key);
			m->mutable_value()->set_string_data(value);
		}
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, double value) {
			Plugin::Common::Metric *m = b->add_value();
			m->set_key(key);
			m->mutable_value()->set_float_data(value);
		}
	}
}
