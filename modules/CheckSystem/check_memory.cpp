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

#include "check_memory.hpp"

#include <CheckMemory.h>

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/filter/cli_helper.hpp>

#include <parsers/filter/realtime_helper.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>

#include <str/format.hpp>

#include <string>

CheckMemory memchecker;


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
			return total - used;
		}
		long long get_used_pct() const {
			return total == 0 ? 0 : get_used() * 100 / total;
		}
		long long get_free_pct() const {
			return total == 0 ? 0 : get_free() * 100 / total;
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

	parsers::where::node_type calculate_free(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
		parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
		double number = value.get<1>();
		std::string unit = value.get<2>();

		if (unit == "%") {
			number = (static_cast<double>(object->get_total())*number) / 100.0;
		} else {
			number = str::format::decode_byte_units(number, unit);
		}
		return parsers::where::factory::create_int(number);
	}

	long long get_zero() {
		return 0;
	}

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {




		filter_obj_handler() {
			static const parsers::where::value_type type_custom_used = parsers::where::type_custom_int_1;
			static const parsers::where::value_type type_custom_free = parsers::where::type_custom_int_2;

			registry_.add_string()
				("type", boost::bind(&filter_obj::get_type, _1), "The type of memory to check")
				;
			registry_.add_int()
				("size", boost::bind(&filter_obj::get_total, _1), "Total size of memory")
				("free", type_custom_free, boost::bind(&filter_obj::get_free, _1), "Free memory in bytes (g,m,k,b) or percentages %")
				.add_scaled_byte(boost::bind(&get_zero), boost::bind(&filter_obj::get_total, _1))
				.add_percentage(boost::bind(&filter_obj::get_total, _1), "", " %")

				("used", type_custom_used, boost::bind(&filter_obj::get_used, _1), "Used memory in bytes (g,m,k,b) or percentages %")
				.add_scaled_byte(boost::bind(&get_zero), boost::bind(&filter_obj::get_total, _1))
				.add_percentage(boost::bind(&filter_obj::get_total, _1), "", " %")
				("free_pct", boost::bind(&filter_obj::get_free_pct, _1), "% free memory")
				("used_pct", boost::bind(&filter_obj::get_used_pct, _1), "% used memory")
				;
			registry_.add_human_string()
				("size", boost::bind(&filter_obj::get_total_human, _1), "")
				("free", boost::bind(&filter_obj::get_free_human, _1), "")
				("used", boost::bind(&filter_obj::get_used_human, _1), "")
				;

			registry_.add_converter()
				(type_custom_free, &calculate_free)
				(type_custom_used, &calculate_free)
				;
				
		}
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}

namespace memory_checks {
	namespace realtime {


		struct runtime_data {
			typedef check_mem_filter::filter filter_type;
			typedef CheckMemory* transient_data_type;

			std::list<std::string> checks;

			void boot() {}
			void touch(boost::posix_time::ptime now) {}
			bool has_changed(transient_data_type) const { return true; }
			modern_filter::match_result process_item(filter_type &filter, transient_data_type);
			void add(const std::string &data);
		};

		struct mem_filter_helper_wrapper {
			typedef parsers::where::realtime_filter_helper<runtime_data, filters::mem::filter_config_object> mem_filter_helper;
			mem_filter_helper helper;

			mem_filter_helper_wrapper(nscapi::core_wrapper *core, int plugin_id) : helper(core, plugin_id) {}

		};

		void runtime_data::add(const std::string &data) {
			checks.push_back(data);
		}

		modern_filter::match_result runtime_data::process_item(filter_type &filter, transient_data_type memoryChecker) {
			modern_filter::match_result ret;
			CheckMemory::memData mem_data;
			try {
				mem_data = memoryChecker->getMemoryStatus();
			} catch (const CheckMemoryException &e) {
			}
			BOOST_FOREACH(const std::string &type, checks) {
				unsigned long long used(0), total(0);
				if (type == "commited") {
					used = mem_data.commited.total - mem_data.commited.avail;
					total = mem_data.commited.total;
				} else if (type == "physical") {
					used = mem_data.phys.total - mem_data.phys.avail;
					total = mem_data.phys.total;
				} else if (type == "virtual") {
					used = mem_data.virt.total - mem_data.virt.avail;
					total = mem_data.virt.total;
				}
				boost::shared_ptr<check_mem_filter::filter_obj> record(new check_mem_filter::filter_obj(type, used, total));
				ret.append(filter.match(record));
			}
			return ret;
		}


		helper::helper(nscapi::core_wrapper *core, int plugin_id) : memory_helper(new mem_filter_helper_wrapper(core, plugin_id)) {
		}

		void helper::add_obj(boost::shared_ptr<filters::mem::filter_config_object> object) {
			runtime_data data;
			BOOST_FOREACH(const std::string &d, object->data) {
				data.add(d);
			}
			memory_helper->helper.add_item(object, data, "system.memory");

		}

		void helper::boot() {
			memory_helper->helper.touch_all();
		}

		void helper::check() {
			memory_helper->helper.process_items(&memchecker);
		}


	}
	namespace memory {

		namespace po = boost::program_options;
		/**
		 * Check available memory and return various check results
		 * Example: checkMem showAll maxWarn=50 maxCrit=75
		 *
		 * @param command Command to execute
		 * @param argLen The length of the argument buffer
		 * @param **char_args The argument buffer
		 * @param &msg String to put message in
		 * @param &perf String to put performance data in
		 * @return The status of the command
		 */
		void check(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
			typedef check_mem_filter::filter filter_type;
			modern_filter::data_container data;
			modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
			std::vector<std::string> types;

			filter_type filter;
			filter_helper.add_options("used > 80%", "used > 90%", "", filter.get_filter_syntax(), "ignored");
			filter_helper.add_syntax("${status}: ${list}", "${type} = ${used}", "${type}", "", "");
			filter_helper.get_desc().add_options()
				("type", po::value<std::vector<std::string>>(&types), "The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)")
				;

			if (!filter_helper.parse_options())
				return;

			if (types.empty()) {
				types.push_back("committed");
				types.push_back("physical");
			}

			if (!filter_helper.build_filter(filter))
				return;

			CheckMemory::memData mem_data;
			try {
				mem_data = memchecker.getMemoryStatus();
			} catch (CheckMemoryException e) {
				return nscapi::protobuf::functions::set_response_bad(*response, e.reason());
			}

			BOOST_FOREACH(const std::string &type, types) {
				unsigned long long used(0), total(0);
				if (type == "committed") {
					used = mem_data.commited.total - mem_data.commited.avail;
					total = mem_data.commited.total;
				} else if (type == "physical") {
					used = mem_data.phys.total - mem_data.phys.avail;
					total = mem_data.phys.total;
				} else if (type == "virtual") {
					used = mem_data.virt.total - mem_data.virt.avail;
					total = mem_data.virt.total;
				} else {
					return nscapi::protobuf::functions::set_response_bad(*response, "Invalid type: " + type);
				}
				boost::shared_ptr<check_mem_filter::filter_obj> record(new check_mem_filter::filter_obj(type, used, total));
				filter.match(record);
			}

			filter_helper.post_process(filter);
		}
	}
}
