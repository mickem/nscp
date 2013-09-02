#pragma once

#include <string>

#include <error.hpp>
#include <format.hpp>

#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp>

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include <win_sysinfo/win_sysinfo.hpp>
#include <EnumNtSrv.h>
#include <EnumProcess.h>

namespace check_cpu_filter {

	struct filter_obj {
		std::string time;
		std::string core;
		const windows::system_info::load_entry &value;

		filter_obj(std::string time, std::string core, const windows::system_info::load_entry &value) : time(time), core(core), value(value) {}

		long long get_total() const {
			return static_cast<long long>(value.total);
		}
		long long get_idle() const {
			return static_cast<long long>(value.idle);
		}
		long long get_kernel() const {
			return static_cast<long long>(value.kernel);
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
			return value.core;
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
		unsigned long long used;
		unsigned long long total;

		filter_obj(std::string type, unsigned long long used, unsigned long long total) : type(type), used(used), total(total) {}

		long long get_total() const {
			return total;
		}
		long long get_used() const {
			return used;
		}
		long long get_free() const {
			return total-used;
		}
		std::string get_type() const {
			return type;
		}

		std::string get_total_human() const {
			return format::format_byte_units(get_total());
		}
		std::string get_used_human() const {
			return format::format_byte_units(get_used());
		}
		std::string get_free_human() const {
			return format::format_byte_units(get_free());
		}
	};

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}



namespace check_svc_filter {
	typedef services_helper::service_info filter_obj;
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
		std::string get_uptime_s() const {
			return format::itos_as_time((now-get_uptime())*1000);
		}
		std::string get_boottime_s() const {
			return format::format_date(boot);
		}
	};

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}



namespace check_proc_filter {
	typedef process_helper::process_info filter_obj;

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}
