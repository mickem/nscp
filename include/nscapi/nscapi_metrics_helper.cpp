/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
