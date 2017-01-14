/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
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