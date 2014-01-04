#include "realtime_data.hpp"

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include <strEx.h>

#include <nscapi/nscapi_plugin_interface.hpp>
/*
namespace check_cpu_filter {
	void runtime_data::add(const std::string &time) {
		container c;
		c.alias = time;
		c.time = format::decode_time<long>(time, 1);
		checks.push_back(c);
	}

	bool runtime_data::process_item(filter_type &filter, transient_data_type thread) {
		bool matched = false;
		BOOST_FOREACH(container &c, checks) {
			std::map<std::string,windows::system_info::load_entry> vals = thread->get_cpu_load(c.time);
			typedef std::map<std::string,windows::system_info::load_entry>::value_type vt;
			BOOST_FOREACH(vt v, vals) {
				boost::shared_ptr<check_cpu_filter::filter_obj> record(new check_cpu_filter::filter_obj(c.alias, v.first, v.second));
				boost::tuple<bool,bool> ret = filter.match(record);
				if (ret.get<0>()) {
					matched = true;
					if (ret.get<1>()) {
						break;
					}
				}
			}
		}
		return matched;
	}
}
namespace check_mem_filter {
	void runtime_data::add(const std::string &data) {
		checks.push_back(data);
	}

	bool runtime_data::process_item(filter_type &filter, transient_data_type memoryChecker) {
		bool matched = false;
		CheckMemory::memData mem_data;
		try {
			mem_data = memoryChecker->getMemoryStatus();
		} catch (const CheckMemoryException &e) {
		}
		BOOST_FOREACH(const std::string &type, checks) {
			unsigned long long used(0), total(0);
			if (type == "commited") {
				used = mem_data.commited.total-mem_data.commited.avail;
				total = mem_data.commited.total;
			} else if (type == "physical") {
				used = mem_data.phys.total-mem_data.phys.avail;
				total = mem_data.phys.total;
			} else if (type == "virtual") {
				used = mem_data.virt.total-mem_data.virt.avail;
				total = mem_data.virt.total;
			}
			boost::shared_ptr<check_mem_filter::filter_obj> record(new check_mem_filter::filter_obj(type, used, total));
			boost::tuple<bool,bool> ret = filter.match(record);

			if (ret.get<0>()) {
				matched = true;
				if (ret.get<1>()) {
					break;
				}
			}
		}
		return matched;
	}
}
*/
