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

#include <metrics/metrics_store_map.hpp>

#include <boost/foreach.hpp>
#include <strEx.h>

namespace metrics {

	void build_metrics(metrics_store::values_map &metrics, const Plugin::Common::MetricsBundle &b, const std::string &path) {
		std::string p = "";
		if (!path.empty())
			p += path + ".";
		p += b.key();

		BOOST_FOREACH(const Plugin::Common::MetricsBundle &b2, b.children()) {
			build_metrics(metrics, b2, p);
		}

		BOOST_FOREACH(const Plugin::Common::Metric &v, b.value()) {
			if (v.value().has_int_data())
				metrics[ p + "." + v.key()] = strEx::s::xtos(v.value().int_data());
			else if (v.value().has_string_data())
				metrics[p + "." + v.key()] = v.value().string_data();
			else if (v.value().has_float_data())
				metrics[p + "." + v.key()] = strEx::s::xtos(v.value().int_data());
		}
	}


	void metrics_store::set(const Plugin::MetricsMessage &response) {
		metrics_store::values_map tmp;

		BOOST_FOREACH(const Plugin::MetricsMessage::Response &p, response.payload()) {
			BOOST_FOREACH(const Plugin::Common::MetricsBundle &b, p.bundles()) {
				build_metrics(tmp, b, "");
			}
		}
		{
			boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!lock.owns_lock())
				return;
			values_ = tmp;
		}
	}

	metrics_store::values_map metrics_store::get(const std::string &filter) {
		bool f = !filter.empty();
		metrics_store::values_map ret;
		{
			boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!lock.owns_lock())
				return ret;

			BOOST_FOREACH(const values_map::value_type &v, values_) {
				if (!f || v.first.find(filter) != std::string::npos)
					ret[v.first] = v.second;
			}
		}
		return ret;
	}

}