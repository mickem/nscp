/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should  have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include "stdafx.h"
#include "module.hpp"
#include "CheckSystem.h"

#include <map>
#include <set>
 
#include <boost/regex.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
 
#include <utils.h>
#include <EnumNtSrv.h>
#include <EnumProcess.h>
#include <sysinfo.h>
#include <simple_registry.hpp>
#include <settings/client/settings_client.hpp>
#include <win_sysinfo/win_sysinfo.hpp>
//#include <config.h>

#include <pdh/pdh_enumerations.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>

#include "filter.hpp"
#include "counter_filter.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

std::pair<bool,std::string> validate_counter(std::string counter) {
	/*
	std::wstring error;
	if (!PDH::PDHResolver::validate(counter, error, false)) {
		NSC_DEBUG_MSG(_T("not found (but due to bugs in pdh this is common): ") + error);
	}
	*/

	PDH::PDHQuery pdh;
	PDH::pdh_instance instance;
	try {
		std::size_t pos = counter.find("($INSTANCE$)");
		if (pos != std::string::npos) {
			std::string c = counter;
			strEx::replace(c, "$INSTANCE$", "*");
			std::string err;
			bool status = true;
			BOOST_FOREACH(std::string s, PDH::Enumerations::expand_wild_card_path(c, err)) {
				std::string::size_type pos1 = s.find('(');
				std::string tag = s;
				if (pos1 != std::string::npos) {
					std::string::size_type pos2 = s.find(')', pos1);
					if (pos2 != std::string::npos)
						tag = s.substr(pos1+1, pos2-pos1-1);
				}
				std::pair<bool,std::string> ret = validate_counter(s);
				status &= ret.first;
				if (!err.empty())
					err += ", ";
				err += tag + "=" + ret.second;
			}
			return std::make_pair(status, err);
		}
		instance = PDH::factory::create(counter);
		pdh.addCounter(instance);
		pdh.open();
		pdh.gatherData();
		pdh.close();
 		return std::make_pair(true, "ok(" + strEx::s::xtos(instance->get_value()) + ")");
	} catch (const PDH::pdh_exception &e) {
		try {
			pdh.gatherData();
			pdh.close();
 			return std::make_pair(true, "ok-rate(" + strEx::s::xtos(instance->get_value()) + ")");
		} catch (const std::exception&) {
			std::pair<bool,std::string> p(false, "query failed: EXCEPTION" + e.reason());
			return p;
		}
	} catch (const std::exception &e) {
		std::pair<bool,std::string> p(false, "query failed: EXCEPTION" + utf8::utf8_from_native(e.what()));
		return p;
	}
}

void load_counters(std::map<std::string,std::string> &counters, sh::settings_registry &settings) {
	settings.alias().add_path_to_settings()
		("counters", sh::string_map_path(&counters)
		, "PDH COUNTERS", "Define various PDH counters to check.")
		;

	settings.register_all();
	settings.notify();
	settings.clear();

	std::string path = settings.alias().get_settings_path("counters");
}

/**
 * New version of the load call.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool CheckSystem::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias("system", alias, "windows");
	std::string counter_path = settings.alias().get_settings_path("counters");



// 	if (mode == NSCAPI::normalStart) {
// 		load_counters(counters, settings);
// 	}

	settings.alias().add_path_to_settings()
		("WINDOWS CHECK SYSTEM", "Section for system checks and system settings")

		("service mapping", "SERVICE MAPPING SECTION", "Configure which services has to be in which state")

		("counters", sh::fun_values_path(boost::bind(&CheckSystem::add_counter, this, get_settings_proxy(), counter_path, _1, _2)), 
		"COUNTERS", "Add counters to check")
		;

	settings.alias().add_key_to_settings()
		("default buffer length", sh::string_key(&collector.default_buffer_size, "1h"),
		"DEFAULT LENGTH", "Used to define the default interval for range buffer checks (ie. CPU).")

		("subsystem", sh::string_key(&collector.subsystem, "default"),
		"PDH SUBSYSTEM", "Set which pdh subsystem to use.", true)
		;

// 	settings.alias().add_key_to_settings("service mapping")
// 
// 		("BOOT_START", sh::string_vector_key(&lookups_, SERVICE_BOOT_START, "ignored"),
// 		"SERVICE_BOOT_START", "TODO", true)
// 
// 		("SYSTEM_START", sh::string_vector_key(&lookups_, SERVICE_SYSTEM_START, "ignored"),
// 		"SERVICE_SYSTEM_START", "TODO", true)
// 
// 		("AUTO_START", sh::string_vector_key(&lookups_, SERVICE_AUTO_START, "started"),
// 		"SERVICE_AUTO_START", "TODO", true)
// 
// 		("DEMAND_START", sh::string_vector_key(&lookups_, SERVICE_DEMAND_START, "ignored"),
// 		"SERVICE_DEMAND_START", "TODO", true)
// 
// 		("DISABLED", sh::string_vector_key(&lookups_, SERVICE_DISABLED, "stopped"),
// 		"SERVICE_DISABLED", "TODO", true)
// 
// 		("DELAYED", sh::string_vector_key(&lookups_, NSCP_SERVICE_DELAYED, "ignored"),
// 		"SERVICE_DELAYED", "TODO", true)
// 		;


	bool reg_alias;
	settings.alias().add_parent("/settings/default").add_key_to_settings()

		("modern commands", sh::bool_key(&reg_alias, true),
		"Register modern aliases for built-in commands", "Register modern alias for commands (ccheck_xxx as opposed of CheckXXX) these are the names which will be used in future version of NSClient++", true)
		;

		settings.register_all();
		settings.notify();

		
	if (mode == NSCAPI::normalStart) {

		BOOST_FOREACH(const check_pdh::counter_config_object &object, pdh_checker.counters_.get_object_list()) {
			PDH::pdh_object counter;
			counter.alias = object.alias;
			counter.path = object.counter;

			counter.set_strategy(object.collection_strategy);
			counter.set_instances(object.instances);
			counter.set_buffer_size(object.buffer_size);
			counter.set_type(object.type);

			collector.add_counter(counter);
		}
	}


	if (mode == NSCAPI::normalStart) {
		collector.start();
	}

	return true;
}


/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool CheckSystem::unloadModule() {
	if (!collector.stop()) {
		NSC_LOG_ERROR("Could not exit the thread, memory leak and potential corruption may be the result...");
	}
	return true;
}

std::string qoute(const std::string &s) {
	if (s.find(',') == std::string::npos)
		return s;
	return "\"" + s + "\"";
}
bool render_list(const PDH::Enumerations::Objects &list, bool validate, bool porcelain, std::string filter, std::string &result) {
	if (!porcelain) {
		result += "Listing counters\n";
		result += "---------------------------\n";
	}
	try {
		int total = 0, match = 0;
		BOOST_FOREACH(const PDH::Enumerations::Object &obj, list) {
			if (porcelain) {
				BOOST_FOREACH(const std::string &inst, obj.instances) {
					std::string line = "\\" + obj.name + "(" + inst + ")\\";
					total++;
					if (!filter.empty() && line.find(filter) == std::string::npos)
						continue;
					result += "instance," + qoute(obj.name) + "," + qoute(inst) + "\n";
					match++;
				}
				BOOST_FOREACH(const std::string &count, obj.counters) {
					std::string line = "\\" + obj.name + "\\" + count;
					total++;
					if (!filter.empty() && line.find(filter) == std::string::npos)
						continue;
					result += "counter," + qoute(obj.name) + "," + qoute(count) + "\n";
					match++;
				}
				if (obj.instances.empty() && obj.counters.empty()) {
					std::string line = "\\" + obj.name + "\\";
					total++;
					if (!filter.empty() && line.find(filter) == std::string::npos)
						continue;
					result += "counter," + qoute(obj.name) + ",,\n";
					match++;
				} else if (!obj.error.empty()) {
					result += "error," + obj.name + "," + utf8::utf8_from_native(obj.error) + "\n";
				}
			} else if (!obj.error.empty()) {
				result += "Failed to enumerate counter " + obj.name + ": " + utf8::utf8_from_native(obj.error) + "\n";
			} else if (obj.instances.size() > 0) {
				BOOST_FOREACH(const std::string &inst, obj.instances) {
					BOOST_FOREACH(const std::string &count, obj.counters) {
						std::string line = "\\" + obj.name + "(" + inst + ")\\" + count;
						total++;
						if (!filter.empty() && line.find(filter) == std::string::npos)
							continue;
						boost::tuple<bool,std::string> status;
						if (validate) {
							status = validate_counter(line);
							result += line + ": " + status.get<1>() + "\n";
						} else
							result += line + "\n";
						match++;
					}
				}
			} else {
				BOOST_FOREACH(const std::string &count, obj.counters) {
					std::string line = "\\" + obj.name + "\\" + count;
					total++;
					if (!filter.empty() && line.find(filter) == std::string::npos)
						continue;
					boost::tuple<bool,std::string> status;
					if (validate) {
						status = validate_counter(line);
						result += line + ": " + status.get<1>() + "\n";
					} else 
						result += line + "\n";
					match++;
				}
			}
		}
		if (!porcelain) {
			result += "---------------------------\n";
			result += "Listed " + strEx::s::xtos(match) + " of " + strEx::s::xtos(total) + " counters.";
		}
		return true;
	} catch (const PDH::pdh_exception &e) {
		result = "ERROR: Service enumeration failed: " + e.reason();
		return false;
	}
}

int CheckSystem::commandLineExec(const std::string &command, const std::list<std::string> &arguments, std::string &result) {
	if (command == "pdh" || command == "help" || command.empty()) {
		namespace po = boost::program_options;

		std::string lookup, counter, list_string, computer, username, password;
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h", "Show help screen")
			("porcelain", "Computer parsable format")
			("computer", po::value<std::string>(&computer), "The computer to fetch values from")
			("user", po::value<std::string>(&username), "The username to login with (only meaningful if computer is specified)")
			("password", po::value<std::string>(&password), "The password to login with (only meaningful if computer is specified)")
			("lookup-index", po::value<std::string>(&lookup), "Lookup a numeric value in the PDH index table")
			("lookup-name", po::value<std::string>(&lookup), "Lookup a string value in the PDH index table")
			("expand-path", po::value<std::string>(&lookup), "Expand a counter path contaning wildcards into corresponding objects (for instance --expand-path \\System\\*)")
			("check", "Check that performance counters are working")
			("list", po::value<std::string>(&list_string)->implicit_value(""), "List counters and/or instances")
			("validate", po::value<std::string>(&list_string)->implicit_value(""), "List counters and/or instances")
			("all", "List/check all counters not configured counter")
			("no-counters", "Do not recurse and list/validate counters for any matching items")
			("no-instances", "Do not recurse and list/validate instances for any matching items")
			("counter", po::value<std::string>(&counter)->implicit_value(""), "Specify which counter to work with")
			("filter", po::value<std::string>(&counter)->implicit_value(""), "Specify a filter to match (substring matching)")
			;
		boost::program_options::variables_map vm;

		if (command == "help") {
			std::stringstream ss;
			ss << "system helper Command line syntax:" << std::endl;
			ss << desc;
			result = ss.str();
			return NSCAPI::returnOK;
		}

		std::vector<std::string> args(arguments.begin(), arguments.end());
		po::parsed_options parsed = po::basic_command_line_parser<char>(args).options(desc).run();
		po::store(parsed, vm);
		po::notify(vm);

		bool porcelain = vm.count("porcelain");
		bool all = vm.count("all");
		bool validate = vm.count("validate");
		bool no_objects = vm.count("no-counters");
		bool no_instances = vm.count("no-instances");
		bool list = vm.count("list") || (validate && counter.empty());
		if (counter.empty())
			counter = list_string;

		if (vm.count("help") || (vm.count("check") == 0 && vm.count("list") == 0 && vm.count("validate") == 0 && lookup.empty())) {
			std::stringstream ss;
			ss << "system helper Command line syntax:" << std::endl;
			ss << desc;
			result = ss.str();
			return NSCAPI::returnCRIT;
		}


		if (list) {
			if (all) {
				// If we specified all list all counters
				PDH::Enumerations::Objects lst = PDH::Enumerations::EnumObjects(!no_instances, !no_objects);
				return render_list(lst, validate, porcelain, counter, result)?NSCAPI::isSuccess:NSCAPI::hasFailed;
			} else {
				if (vm.count("counter")) {
					// If we specify a counter object we will only list instances of that
					PDH::Enumerations::Objects lst;
					lst.push_back(PDH::Enumerations::EnumObject(counter, !no_instances, !no_objects));
					return render_list(lst, validate, porcelain, counter, result)?NSCAPI::isSuccess:NSCAPI::hasFailed;
				} else {
					// If we specify no query we will list all configured counters 
					int count = 0, match = 0;
					if (counters.empty()) {
						sh::settings_registry settings(get_settings_proxy());
						settings.set_alias("system", "system/windows", "windows");
						load_counters(counters, settings);
					}
					if (!porcelain) {
						result += "Listing configured counters\n";
						result += "---------------------------\n";
					} 
					BOOST_FOREACH(const counter_map_type::value_type v, counters) {
						std::string line = v.first + " = " + v.second;
						boost::tuple<bool,std::string> status;
						count++;
						if (!counter.empty() && line.find(utf8::cvt<std::string>(counter)) == std::string::npos)
							continue;

						if (validate)
							status = validate_counter(v.second);

						if (porcelain) 
							line = v.first + "," + v.second + "," + status.get<1>();
						else if (validate)
							line = v.first + " = " + v.second + ": " + status.get<1>();
						else 
							line = v.first + " = " + v.second;
						result += line + "\n";
						match++;
					}
					if (!porcelain) {
						result += "---------------------------\n";
						result += "Listed " + strEx::s::xtos(match) + " of " + strEx::s::xtos(count) + " counters.";
						if (match == 0) {
							result += "No counters was found (perhaps you wanted the --all option to make this a global query, the default is so only look in configured counters).";
						}
					}
				}
			}
			return NSCAPI::isSuccess;
		} else if (vm.count("lookup-index")) {
			try {
				DWORD dw = PDH::PDHResolver::lookupIndex(lookup);
				if (porcelain) {
					result += strEx::s::xtos(dw);
				} else {
					result += "--+--[ Lookup Result ]----------------------------------------\n";
					result += "  | Index for '" + lookup + "' is " + strEx::s::xtos(dw) + "\n";
					result += "--+-----------------------------------------------------------";
				}
			} catch (const PDH::pdh_exception &e) {
				result += "Index not found: " + lookup + ": " + e.reason() + "\n";
				return NSCAPI::hasFailed;
			}
		} else if (vm.count("lookup-name")) {
			try {
				std::string name = PDH::PDHResolver::lookupIndex(strEx::s::stox<DWORD>(lookup));
				if (porcelain) {
					result += name;
				} else {
					result += "--+--[ Lookup Result ]----------------------------------------\n";
					result += "  | Index for '" + lookup + "' is " + name + "\n";
					result += "--+-----------------------------------------------------------";
				}
			} catch (const PDH::pdh_exception &e) {
				result += "Failed to lookup index: " + e.reason();
				return NSCAPI::hasFailed;
			}
		} else if (vm.count("expand-path")) {
			try {
				if (porcelain) {
					BOOST_FOREACH(const std::string &s, PDH::PDHResolver::PdhExpandCounterPath(lookup)) {
						result += s + "\n";
					}
				} else {
					result += "--+--[ Lookup Result ]----------------------------------------";
					BOOST_FOREACH(const std::string &s, PDH::PDHResolver::PdhExpandCounterPath(lookup)) {
						result += "  | Found '" + s + "\n";
					}
				}
			} catch (const PDH::pdh_exception &e) {
				result += "Failed to lookup index: " + e.reason();
				return NSCAPI::hasFailed;
			}
		} else {
			std::stringstream ss;
			ss << "pdh Command line syntax:" << std::endl;
			ss << desc;
			result = ss.str();
			return NSCAPI::isSuccess;
		}
	}
	return 0;
}

void CheckSystem::check_cpu(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef check_cpu_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> times;

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "CPU Load ok");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${core}>${load}%", "${core} ${time}");
	filter_helper.get_desc().add_options()
		("time", po::value<std::vector<std::string>>(&times), "The time to check")
		;

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		data.filter_string = "core = 'total'";
		filter_helper.set_default("load > 80", "load > 90");
	}

	if (times.empty()) {
		times.push_back("5m");
		times.push_back("1m");
		times.push_back("5s");
	}

	if (!filter_helper.build_filter(filter))
		return;

	BOOST_FOREACH(const std::string &time, times) {
		std::map<std::string,windows::system_info::load_entry> vals = collector.get_cpu_load(format::decode_time<long>(time, 1));
		typedef std::map<std::string,windows::system_info::load_entry>::value_type vt;
		BOOST_FOREACH(vt v, vals) {
			boost::shared_ptr<check_cpu_filter::filter_obj> record(new check_cpu_filter::filter_obj(time, v.first, v.second));
			boost::tuple<bool,bool> ret = filter.match(record);
		}
	}
	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
}



typedef ULONGLONG (*tGetTickCount64)();

tGetTickCount64 pGetTickCount64 = NULL;

BOOL nscpGetTickCount64() {
	if (pGetTickCount64 == NULL) {
		HMODULE hMod = ::LoadLibrary(_TEXT("kernel32"));
		if (hMod == NULL)
			return 0;
		pGetTickCount64 = reinterpret_cast<tGetTickCount64>(GetProcAddress(hMod, "GetTickCount64"));
		if (pGetTickCount64 == NULL)
			return 0;
	}
	return pGetTickCount64();
}

void CheckSystem::check_uptime(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef check_uptime_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> times;

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "Uptime ok");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "uptime: -${uptime}, boot: ${boot} (UCT)", "uptime");

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		filter_helper.set_default("uptime > -24h", "uptime > -12h");
	}

	if (!filter_helper.build_filter(filter))
		return;

	unsigned long long value = nscpGetTickCount64();
	if (value == 0)
		value = GetTickCount();
	value /=1000;

	boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
	boost::posix_time::ptime epoch(boost::gregorian::date(1970,1,1));
	boost::posix_time::ptime boot = now - boost::posix_time::time_duration(0, 0, value);

	long long now_delta = (now-epoch).total_seconds();
	value =  now_delta - value;


	long long uptime = static_cast<long long>(value);
	boost::shared_ptr<check_uptime_filter::filter_obj> record(new check_uptime_filter::filter_obj(uptime, now_delta, boot));
	boost::tuple<bool,bool> ret = filter.match(record);

	modern_filter::perf_writer scaler(response);
	filter_helper.post_process(filter, &scaler);
}

void CheckSystem::check_os_version(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef os_version_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "Version ok");
	filter_helper.add_syntax("${list}", filter.get_format_syntax(), "${version} (${major}.${minor}.${build})", "version");

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		filter_helper.set_default("version > 50", "version > 50");
	}

	if (!filter_helper.build_filter(filter))
		return;


	boost::shared_ptr<os_version_filter::filter_obj> record(new os_version_filter::filter_obj());
	OSVERSIONINFOEX *info = windows::system_info::get_versioninfo();
	record->major_version = info->dwMajorVersion;
	record->minor_version = info->dwMinorVersion;
	record->build = info->dwBuildNumber;
	record->plattform = info->dwPlatformId;
	record->version_s = windows::system_info::get_version_string();
	record->version_i = windows::system_info::get_version();

	boost::tuple<bool,bool> ret = filter.match(record);

	modern_filter::perf_writer scaler(response);
	filter_helper.post_process(filter, &scaler);
}


/**
 * Retrieve the service state of one or more services (by name).
 * Parse a list with a service names and verify that all named services are running.
 * <pre>
 * Syntax:
 * request: checkServiceState <option> [<option> [...]]
 * Return: <return state>&<service1> : <state1> - <service2> : <state2> - ...
 * Available options:
 *		<name>=<state>	Check if a service has a specific state
 *			State can be wither started or stopped
 *		ShowAll			Show the state of all listed service. If not set only critical services are listed.
 * Examples:
 * checkServiceState showAll myService MyService
 *</pre>
 *
 * @param command Command to execute
 * @param argLen The length of the argument buffer
 * @param **char_args The argument buffer
 * @param &msg String to put message in
 * @param &perf String to put performance data in 
 * @return The status of the command
 */
void CheckSystem::check_service(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef check_svc_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> services;
	std::string type;
	std::string state;
	std::string computer;

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "OK all services are ok.");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${name}=${state} (${start_type})", "${name}");
	filter_helper.get_desc().add_options()
		("computer", po::value<std::string>(&computer), "THe name of the remote computer to check")
		("service", po::value<std::vector<std::string>>(&services), "The service to check, set this to * to check all services")
		("type", po::value<std::string>(&type)->default_value("service"), "The types of services to enumerate available types are driver, file-system-driver, kernel-driver, service, service-own-process, service-share-process")
		("state", po::value<std::string>(&state)->default_value("all"), "The types of services to enumerate available states are active, inactive or all")
		;

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		filter_helper.set_default("not state_is_perfect()", "not state_is_ok()");
	}

	if (services.empty()) {
		services.push_back("*");
	}
	if (!filter_helper.build_filter(filter))
		return;

	BOOST_FOREACH(const std::string &service, services) {
		if (service == "*") {
			BOOST_FOREACH(const services_helper::service_info &info, services_helper::enum_services(computer, services_helper::parse_service_type(type), services_helper::parse_service_state(state))) {
				boost::shared_ptr<services_helper::service_info> record(new services_helper::service_info(info));
				boost::tuple<bool,bool> ret = filter.match(record);
			}
		} else {
			services_helper::service_info info = services_helper::get_service_info(computer, service);
			boost::shared_ptr<services_helper::service_info> record(new services_helper::service_info(info));
			boost::tuple<bool,bool> ret = filter.match(record);
		}
	}
	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
}

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
void CheckSystem::check_memory(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef check_mem_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> types;

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "OK memory within bounds.");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${type} > ${used}", "${type}");
	filter_helper.get_desc().add_options()
		("type", po::value<std::vector<std::string>>(&types), "The type of memory to check")
		;

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		filter_helper.set_default("used > 80%", "used > 90%");
	}

	if (types.empty()) {
		types.push_back("page");
		types.push_back("physical");
	}

	if (!filter_helper.build_filter(filter))
		return;

	CheckMemory::memData mem_data;
	try {
		mem_data = memoryChecker.getMemoryStatus();
	} catch (CheckMemoryException e) {
		return nscapi::protobuf::functions::set_response_bad(*response, e.reason());
	}

	BOOST_FOREACH(const std::string &type, types) {
		unsigned long long used(0), total(0);
		if (type == "page") {
			used = mem_data.pageFile.total-mem_data.pageFile.avail;
			total = mem_data.pageFile.total;
		} else if (type == "physical") {
			used = mem_data.phys.total-mem_data.phys.avail;
			total = mem_data.phys.total;
		} else if (type == "virtual") {
			used = mem_data.virtualMem.total-mem_data.virtualMem.avail;
			total = mem_data.virtualMem.total;
		} else {
			return nscapi::protobuf::functions::set_response_bad(*response, "Invalid type: " + type);
		}
		boost::shared_ptr<check_mem_filter::filter_obj> record(new check_mem_filter::filter_obj(type, used, total));
		boost::tuple<bool,bool> ret = filter.match(record);
	}

	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
//MAP_OPTIONS_STR("perf-unit", tmpObject.perf_unit)
}

class NSC_error : public process_helper::error_reporter {
	void report_error(std::string error) {
		NSC_LOG_ERROR(error);
	}
	void report_warning(std::string error) {
		NSC_LOG_MESSAGE(error);
	}
	void report_debug(std::string error) {
		NSC_DEBUG_MSG_STD(error);
	}
};

void CheckSystem::check_process(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef check_proc_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> processes;
	bool deep_scan = true;

	NSC_error err;
	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "OK all services are ok.");
	filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${exe}=${state}", "${exe}");
	filter_helper.get_desc().add_options()
		("process", po::value<std::vector<std::string>>(&processes), "The service to check, set this to * to check all services")
		("deep-scan", po::value<bool>(&deep_scan), "If all process metrics should be fetched (otherwise only status is fetched)")
		;

	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		if (data.filter_string.empty())
			data.filter_string = "state != 'unreadable'";
		filter_helper.set_default("state not in ('started')", "state = 'stopped'");
	}

	if (processes.empty()) {
		processes.push_back("*");
	}
	if (!filter_helper.build_filter(filter))
		return;

	std::set<std::string> procs;
	bool all = false;
	BOOST_FOREACH(const std::string &process, processes) {
		if (process == "*")
			all = true;
		else if (procs.count(process) == 0)
			procs.insert(process);
	}

	BOOST_FOREACH(const process_helper::process_info &info, process_helper::enumerate_processes(true, true, &err)) {
		bool wanted = procs.count(info.exe);
		if (all || wanted) {
			boost::shared_ptr<process_helper::process_info> record(new process_helper::process_info(info));
			boost::tuple<bool,bool> ret = filter.match(record);
		}
		if (wanted) {
			procs.erase(info.exe);
		}
	}
	BOOST_FOREACH(const std::string proc, procs) {
		boost::shared_ptr<process_helper::process_info> record(new process_helper::process_info(proc));
		boost::tuple<bool,bool> ret = filter.match(record);
	}
	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
}

/**
 * Check a counter and return the value
 *
 * @param command Command to execute
 * @param argLen The length of the argument buffer
 * @param **char_args The argument buffer
 * @param &msg String to put message in
 * @param &perf String to put performance data in 
 * @return The status of the command
 *
 * @todo add parsing support for NRPE
 */
void CheckSystem::check_pdh(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	pdh_checker.check_pdh(collector, request, response);
}

void CheckSystem::add_counter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string path, std::string key, std::string query) {
	pdh_checker.add_counter(proxy, path, key, query);
}
