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

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include <error/error.hpp>
#include <str/format.hpp>

#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp>

#include <string>

namespace check_cpu_filter {

	struct filter_obj {
		std::string time;
		std::string core;

		filter_obj(std::string time, std::string core) : time(time), core(core) {}

		long long get_total() const {
			return 0;
		}
		long long get_idle() const {
			return 0;
		}
		long long get_kernel() const {
			return 0;
		}
		std::string get_time() const {
			return time;
		}
		std::string get_core_s() const {
			return core;
		}
		std::string get_core_id() const {
			return boost::replace_all_copy(core, " ", "_");
		}
		long long get_core_i() const {
			return 0;
		}
	};
	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};


	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}

namespace check_mem_filter {

	struct filter_obj {
		std::string type;
		unsigned long long free;
		unsigned long long total;

		filter_obj(std::string type, unsigned long long free, unsigned long long total) : type(type), free(free), total(total) {}
		filter_obj(const filter_obj &other) : type(other.type), free(other.free), total(other.total) {}

		long long get_total() const {
			return total;
		}
		long long get_used() const {
			return total-free;
		}
		long long get_free() const {
			return free;
		}
		std::string get_type() const {
			return type;
		}

		std::string get_total_human() const {
			return str::format::format_byte_units(get_total());
		}
		std::string get_used_human() const {
			return str::format::format_byte_units(get_used());
		}
		std::string get_free_human() const {
			return str::format::format_byte_units(get_free());
		}
	};

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}
/*
namespace check_proc_filter {
	typedef process_helper::process_info filter_obj;

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}
*/
namespace os_version_filter {

	struct filter_obj {
		std::string kernel_name;
		std::string nodename;
		std::string kernel_version;
		std::string kernel_release;
		std::string machine;
		std::string processor;
		std::string os;

		filter_obj() {}

		std::string get_kernel_name() const {
			return kernel_name;
		}
		std::string get_nodename() const {
			return nodename;
		}
		std::string get_kernel_version() const {
			return kernel_version;
		}
		std::string get_kernel_release() const {
			return kernel_release;
		}
		std::string get_machine() const {
			return machine;
		}
		std::string get_processor() const {
			return processor;
		}
		std::string get_os() const {
			return os;
		}
	};
	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;

	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};


	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}

namespace check_uptime_filter {

	struct filter_obj {
		long long uptime;
		long long now;
		boost::posix_time::ptime boot;

		filter_obj(long long uptime, long long now, boost::posix_time::ptime boot) : uptime(uptime), now(now), boot(boot) {}

		long long get_uptime() const {
			return uptime;
		}
		long long get_boot() const {
			return now-uptime;
		}
		std::string get_boot_s() const {
			return str::format::format_date(boot);
		}
		std::string get_uptime_s() const {
			return str::format::itos_as_time(get_uptime()*1000);
		}
	};

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}
