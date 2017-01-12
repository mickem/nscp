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

#include <net/pinger.hpp>

#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/helpers.hpp>
#include <parsers/where.hpp>

#include <error/error.hpp>
#include <str/format.hpp>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>

#include <map>
#include <string>

namespace ping_filter {
	struct filter_obj {
		filter_obj(result_container result)
			: is_total_(false)
			, result(result) {}
		filter_obj()
			: is_total_(true) {}
		const filter_obj& operator=(const filter_obj&other) {
			result = other.result;
			return *this;
		}

		static boost::shared_ptr<ping_filter::filter_obj> get_total();

		//		std::string get_filename() { return filename; }
			//	std::string get_path(parsers::where::evaluation_context) { return path.string(); }

		std::string get_host(parsers::where::evaluation_context) { if (is_total_) return "total"; return result.destination_; }
		std::string get_ip(parsers::where::evaluation_context) { if (is_total_) return "total"; return result.ip_; }
	public:
		void add(boost::shared_ptr<filter_obj> info);
		void make_total() { is_total_ = true; }
		bool is_total() const { return is_total_; }
		long long get_sent(parsers::where::evaluation_context) {
			return result.num_send_;
		}
		long long get_recv(parsers::where::evaluation_context) {
			return result.num_replies_;
		}
		long long get_timeout(parsers::where::evaluation_context) {
			return result.num_timeouts_;
		}

		long long get_loss(parsers::where::evaluation_context c) {
			if (result.num_send_ == 0) {
				c->error("No packages were sent");
				return 0;
			}
			return result.num_timeouts_ * 100 / result.num_send_;
		}
		long long get_time(parsers::where::evaluation_context) { return result.time_; }

		bool is_total_;
		result_container result;
	};

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}