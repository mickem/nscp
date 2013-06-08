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
 
#include <utils.h>
#include <EnumNtSrv.h>
#include <EnumProcess.h>
#include <checkHelpers.hpp>
#include <sysinfo.h>
#include <filter_framework.hpp>
#include <simple_registry.hpp>
#include <settings/client/settings_client.hpp>
#include <config.h>


template <class TFilterType>
class FilterBounds {
public:
	TFilterType filter;
	typedef typename TFilterType::TValueType TValueType;
	typedef FilterBounds<TFilterType> TMyType;

	FilterBounds() {}
	FilterBounds(const FilterBounds &other) {
		filter = other.filter;
	}
	void reset() {
		filter.reset();
	}
	bool hasBounds() {
		return filter.hasFilter();
	}

	static std::wstring toStringLong(typename TValueType &value) {
		//return filter.to_string() + _T(" matches ") + value;
		// TODO FIx this;
		return value;
		//return TNumericHolder::toStringLong(value.count) + _T(", ") + TStateHolder::toStringLong(value.state);
	}
	static std::wstring toStringShort(typename TValueType &value) {
		// TODO FIx this;
		return value;
		//return TNumericHolder::toStringShort(value.count);
	}
	std::wstring gatherPerfData(std::wstring alias, std::wstring unit, typename TValueType &value, TMyType &warn, TMyType &crit) {
		return _T("");
	}
	std::wstring gatherPerfData(std::wstring alias, std::wstring unit, typename TValueType &value) {
		return _T("");
	}
	bool check(typename TValueType &value, std::wstring lable, std::wstring &message, checkHolders::ResultType type) {
		if (filter.hasFilter()) {
			if (!filter.matchFilter(value))
				return false;
			message = lable + _T(": ") + filter.to_string() + _T(" matches ") + value;
			return true;
		} else {
			NSC_LOG_MESSAGE_STD("Missing bounds for filter check: ", utf8::cvt<std::string>(lable));
		}
		return false;
	}
	const TMyType & operator=(std::wstring value) {
		filter = value;
		return *this;
	}

};

namespace sh = nscapi::settings_helper;

std::pair<bool,std::string> validate_counter(std::string counter) {
	/*
	std::wstring error;
	if (!PDH::PDHResolver::validate(counter, error, false)) {
		NSC_DEBUG_MSG(_T("not found (but due to bugs in pdh this is common): ") + error);
	}
	*/

	typedef boost::shared_ptr<PDH::PDHCounter> counter_ptr;
	counter_ptr pCounter;
	PDH::PDHQuery pdh;
	typedef PDHCollectors::StaticPDHCounterListener<double, PDH_FMT_DOUBLE> counter_type;
	boost::shared_ptr<counter_type> collector(new counter_type());
	try {
		pdh.addCounter(utf8::cvt<std::wstring>(counter), collector);
		pdh.open();
		pdh.gatherData();
		pdh.close();
 		return std::make_pair(true, "ok(" + strEx::s::xtos(collector->getValue()) + ")");
	} catch (const std::exception &e) {
		try {
			pdh.gatherData();
			pdh.close();
 			return std::make_pair(true, "ok-rate(" + strEx::s::xtos(collector->getValue()) + ")");
		} catch (const std::exception &e2) {
			std::pair<bool,std::string> p(false, "query failed: EXCEPTION" + utf8::utf8_from_native(e.what()));
			return p;
		}
	}
}
std::string find_system_counter(std::string counter) {
	if (counter == PDH_SYSTEM_KEY_UPT) {
		char *keys[] = {"\\2\\674", "\\System\\System Up Time", "\\System\\Systembetriebszeit", "\\Sistema\\Tempo di funzionamento sistema", "\\Système\\Temps d'activité système"};
		BOOST_FOREACH(const char *key, keys) {
			std::pair<bool,std::string> result = validate_counter(key);
			if (result.first) {
				return key;
			}
		}
		return keys[0];
	}
	if (counter == PDH_SYSTEM_KEY_MCL) {
		char *keys[] = {"\\4\\30", "\\Memory\\Commit Limit", "\\Speicher\\Zusagegrenze", "\\Memoria\\Limite memoria vincolata", "\\Mémoire\\Limite de mémoire dédiée"};
		BOOST_FOREACH(const char *key, keys) {
			std::pair<bool,std::string> result = validate_counter(key);
			if (result.first) {
				return key;
			}
		}
		return keys[0];
	}
	if (counter == PDH_SYSTEM_KEY_MCB) {
		char *keys[] = {"\\4\\26", "\\Memory\\Committed Bytes", "\\Speicher\\Zugesicherte Bytes", "\\Memoria\\Byte vincolati", "\\Mémoire\\Octets dédiés"};
		BOOST_FOREACH(const char *key, keys) {
			std::pair<bool,std::string> result = validate_counter(key);
			if (result.first) {
				return key;
			}
		}
		return keys[0];
	}
	if (counter == PDH_SYSTEM_KEY_CPU) {
		char *keys[] = {"\\238(_total)\\6", "\\Processor(_total)\\% Processor Time", "\\Prozessor(_Total)\\Prozessorzeit (%)", "\\Processore(_total)\\% Tempo processore", "\\Processeur(_Total)\\% Temps processeur"};
		BOOST_FOREACH(const char *key, keys) {
			std::pair<bool,std::string> result = validate_counter(key);
			if (result.first) {
				return key;
			}
		}
		return keys[0];
	}
}


void load_counters(std::map<std::string,std::string> &counters, sh::settings_registry &settings) {
	settings.alias().add_path_to_settings()
		("pdh/counters", sh::string_map_path(&counters)
		, "PDH COUNTERS", "Define various PDH counters to check.")
		;

	settings.register_all();
	settings.notify();
	settings.clear();

	std::string path = settings.alias().get_settings_path("pdh/counters");
	if (counters[PDH_SYSTEM_KEY_CPU] == "") {
		settings.register_key(path + "/" + PDH_SYSTEM_KEY_CPU, "collection strategy", NSCAPI::key_string, "Collection Strategy", "Collection strategy for CPU is usually round robin, for others static.", "round robin", false);
		settings.set_static_key(path + "/" + PDH_SYSTEM_KEY_CPU, "collection strategy", "round robin");
	}
	char *keys[] = {PDH_SYSTEM_KEY_UPT, PDH_SYSTEM_KEY_MCL, PDH_SYSTEM_KEY_MCB, PDH_SYSTEM_KEY_CPU};
	BOOST_FOREACH(const char *key, keys) {
		if (counters[key] == "") {
			counters[key] = find_system_counter(key);
			settings.register_key(path, key, NSCAPI::key_string, key, "System counter for check_xx commands..", counters[key], false);
		}
	}
}

/**
 * New version of the load call.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool CheckSystem::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	boost::shared_ptr<PDHCollector::system_counter_data> data(new PDHCollector::system_counter_data);
	data->check_intervall = 1;

	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(alias, "system/windows");
	std::string counter_path = settings.alias().get_settings_path("pdh/counters");

	if (mode == NSCAPI::normalStart) {
		load_counters(counters, settings);
	}

	settings.alias().add_path_to_settings()
		("WINDOWS CHECK SYSTEM", "Section for system checks and system settings")

		("service mapping", "SERVICE MAPPING SECTION", "Confiure which services has to be in which state")

		("pdh", "PDH COUNTER INFORMATION", "")

		;

	settings.alias().add_key_to_settings()
		("default buffer length", sh::string_key(&data->buffer_length, "1h"),
		"DEFAULT LENGTH", "Used to define the default interval for range buffer checks (ie. CPU).")

		("default intervall", sh::uint_key(&data->check_intervall, 1),
		"DEFAULT INTERVALL", "Used to define the default interval for range buffer checks (ie. CPU).", true)

		("subsystem", sh::wstring_key(&data->subsystem, _T("default")),
		"PDH SUBSYSTEM", "Set which pdh subsystem to use.", true)
		;

	settings.alias().add_key_to_settings("service mapping")

		("BOOT_START", sh::string_vector_key(&lookups_, SERVICE_BOOT_START, "ignored"),
		"SERVICE_BOOT_START", "TODO", true)

		("SYSTEM_START", sh::string_vector_key(&lookups_, SERVICE_SYSTEM_START, "ignored"),
		"SERVICE_SYSTEM_START", "TODO", true)

		("AUTO_START", sh::string_vector_key(&lookups_, SERVICE_AUTO_START, "started"),
		"SERVICE_AUTO_START", "TODO", true)

		("DEMAND_START", sh::string_vector_key(&lookups_, SERVICE_DEMAND_START, "ignored"),
		"SERVICE_DEMAND_START", "TODO", true)

		("DISABLED", sh::string_vector_key(&lookups_, SERVICE_DISABLED, "stopped"),
		"SERVICE_DISABLED", "TODO", true)

		("DELAYED", sh::string_vector_key(&lookups_, NSCP_SERVICE_DELAYED, "ignored"),
		"SERVICE_DELAYED", "TODO", true)
		;

	bool reg_alias;
	settings.alias().add_parent("/settings/default").add_key_to_settings()

		("modern commands", sh::bool_key(&reg_alias, true),
		"Register modern aliases for built-in commands", "Register modern alias for commands (ccheck_xxx as opposed of CheckXXX) these are the names which will be used in future version of NSClient++", true)
		;

		settings.register_all();
		settings.notify();

		
	if (mode == NSCAPI::normalStart) {
		typedef PDHCollector::system_counter_data::counter cnt;
		BOOST_FOREACH(counter_map_type::value_type c, counters) {
			std::string path = c.second;
			std::pair<bool, std::string> result = validate_counter(path);
			if (!result.first) {
				NSC_LOG_ERROR("Failed to load counter " + c.first + "(" + path + ": " + result.second);
			}
			std::string strategy = settings.get_static_string(counter_path + "/" + c.first, "collection strategy", "static");
			if (strategy == "static") {
				data->counters.push_back(cnt(c.first, utf8::cvt<std::wstring>(path), cnt::type_int64, cnt::format_large, cnt::value));
			} else if (strategy == "round robin") {
				std::string size = settings.get_static_string(counter_path + "/" + c.first, "size", "");
				if (size.empty())
					data->counters.push_back(cnt(c.first, utf8::cvt<std::wstring>(path), cnt::type_int64, cnt::format_large, cnt::rrd));
				else
					data->counters.push_back(cnt(c.first, utf8::cvt<std::wstring>(path), cnt::type_int64, cnt::format_large, cnt::rrd, size));
			} else {
				NSC_LOG_ERROR("Failed to load counter " + c.first + " invalid collection strategy: " + strategy);
			}
		}
	}


	if (mode == NSCAPI::normalStart) {
		pdh_collector.start(data);
	}

	return true;
}
/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool CheckSystem::unloadModule() {
	if (!pdh_collector.stop()) {
		NSC_LOG_ERROR("Could not exit the thread, memory leak and potential corruption may be the result...");
	}
	return true;
}

std::wstring qoute(const std::wstring &s) {
	if (s.find(L',') == std::wstring::npos)
		return s;
	return _T("\"") + s + _T("\"");
}
bool render_list(const PDH::Enumerations::Objects &list, bool validate, bool porcelain, std::wstring filter, std::wstring &result) {
	if (!porcelain) {
		result += _T("Listing counters\n");
		result += _T("---------------------------\n");
	}
	try {
		int total = 0, match = 0;
		BOOST_FOREACH(const PDH::Enumerations::Object &obj, list) {
			if (porcelain) {
				BOOST_FOREACH(const std::wstring &inst, obj.instances) {
					std::wstring line = _T("\\") + obj.name + _T("(") + inst + _T(")\\") ;
					total++;
					if (!filter.empty() && line.find(filter) == std::wstring::npos)
						continue;
					result += _T("instance,") + qoute(obj.name) + _T(",") + qoute(inst) + _T("\n");
					match++;
				}
				BOOST_FOREACH(const std::wstring &count, obj.counters) {
					std::wstring line = _T("\\") + obj.name + _T("\\") + count;
					total++;
					if (!filter.empty() && line.find(filter) == std::wstring::npos)
						continue;
					result += _T("counter,") + qoute(obj.name) + _T(",") + qoute(count) + _T("\n");
					match++;
				}
				if (obj.instances.empty() && obj.counters.empty()) {
					std::wstring line = _T("\\") + obj.name + _T("\\");
					total++;
					if (!filter.empty() && line.find(filter) == std::wstring::npos)
						continue;
					result += _T("counter,") + qoute(obj.name) + _T(",") + _T(",\n");
					match++;
				} else if (!obj.error.empty()) {
					result += _T("error,") + obj.name + _T(",") + utf8::to_unicode(obj.error) + _T("\n");
				}
			} else if (!obj.error.empty()) {
				result += _T("Failed to enumerate counter ") + obj.name + _T(": ") + utf8::to_unicode(obj.error) + _T("\n");
			} else if (obj.instances.size() > 0) {
				BOOST_FOREACH(const std::wstring &inst, obj.instances) {
					BOOST_FOREACH(const std::wstring &count, obj.counters) {
						std::wstring line = _T("\\") + obj.name + _T("(") + inst + _T(")\\") + count;
						total++;
						if (!filter.empty() && line.find(filter) == std::wstring::npos)
							continue;
						boost::tuple<bool,std::string> status;
						if (validate) {
							status = validate_counter(utf8::cvt<std::string>(line));
							result += line + _T(": ") + utf8::cvt<std::wstring>(status.get<1>()) + _T("\n");
						} else
							result += line + _T("\n");
						match++;
					}
				}
			} else {
				BOOST_FOREACH(const std::wstring &count, obj.counters) {
					std::wstring line = _T("\\") + obj.name + _T("\\") + count;
					total++;
					if (!filter.empty() && line.find(filter) == std::wstring::npos)
						continue;
					boost::tuple<bool,std::string> status;
					if (validate) {
						status = validate_counter(utf8::cvt<std::string>(line));
						result += line + _T(": ") + utf8::cvt<std::wstring>(status.get<1>()) + _T("\n");
					} else 
						result += line + _T("\n");
					match++;
				}
			}
		}
		if (!porcelain) {
			result += _T("---------------------------\n");
			result += _T("Listed ") + strEx::itos(match) + _T(" of ") + strEx::itos(total) + _T(" counters.");
		}
		return true;
	} catch (const PDH::pdh_exception &e) {
		result = _T("ERROR: Service enumeration failed: ") + utf8::cvt<std::wstring>(e.reason());
		return false;
	}
}

int CheckSystem::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	if (command == _T("pdh") || command == _T("help") || command.empty()) {
		namespace po = boost::program_options;

		std::wstring lookup, counter, list_string;
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h", "Show help screen")
			("porcelain", "Computer parsable format")
			("lookup-index", po::wvalue<std::wstring>(&lookup), "Lookup a numeric value in the PDH index table")
			("lookup-name", po::wvalue<std::wstring>(&lookup), "Lookup a string value in the PDH index table")
			("expand-path", po::wvalue<std::wstring>(&lookup), "Expand a counter path contaning wildcards into corresponding objects (for instance --expand-path \\System\\*)")
			("check", "Check that performance counters are working")
			("list", po::wvalue<std::wstring>(&list_string)->implicit_value(_T("")), "List counters and/or instances")
			("validate", po::wvalue<std::wstring>(&list_string)->implicit_value(_T("")), "List counters and/or instances")
			("all", "List/check all counters not configured counter")
			("no-counters", "Do not recurse and list/validate counters for any matching items")
			("no-instances", "Do not recurse and list/validate instances for any matching items")
			("counter", po::wvalue<std::wstring>(&counter)->implicit_value(_T("")), "Specify which counter to work with")
			("filter", po::wvalue<std::wstring>(&counter)->implicit_value(_T("")), "Specify a filter to match (substring matching)")
			;
		boost::program_options::variables_map vm;

		if (command == _T("help")) {
			std::stringstream ss;
			ss << "pdh Command line syntax:" << std::endl;
			ss << desc;
			result = utf8::cvt<std::wstring>(ss.str());
			return NSCAPI::isSuccess;
		}

		std::vector<std::wstring> args(arguments.begin(), arguments.end());
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(args).options(desc).run();
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
			ss << "pdh Command line syntax:" << std::endl;
			ss << desc;
			result = utf8::cvt<std::wstring>(ss.str());
			return NSCAPI::isSuccess;
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
						settings.set_alias("check", "system/windows", "system/windows");
						load_counters(counters, settings);
					}
					if (!porcelain) {
						result += _T("Listing configured counters\n");
						result += _T("---------------------------\n");
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
						result += utf8::cvt<std::wstring>(line) + _T("\n");
						match++;
					}
					if (!porcelain) {
						result += _T("---------------------------\n");
						result += _T("Listed ") + strEx::itos(match) + _T(" of ") + strEx::itos(count) + _T(" counters.");
						if (match == 0) {
							result += _T("No counters was found (perhaps you wanted the --all option to make this a global query, the default is so only look in configured counters).");
						}
					}
				}
			}
			return NSCAPI::isSuccess;
		} else if (vm.count("lookup-index")) {
			try {
				DWORD dw = PDH::PDHResolver::lookupIndex(lookup);
				if (porcelain) {
					result += strEx::itos(dw);
				} else {
					result += _T("--+--[ Lookup Result ]----------------------------------------\n");
					result += _T("  | Index for '") + lookup + _T("' is ") + strEx::itos(dw) + _T("\n");
					result += _T("--+-----------------------------------------------------------");
				}
			} catch (const PDH::pdh_exception &e) {
				result += _T("Index not found: ") + lookup + _T("\n");
				return NSCAPI::hasFailed;
			}
		} else if (vm.count("lookup-name")) {
			try {
				std::wstring name = PDH::PDHResolver::lookupIndex(strEx::stoi(lookup));
				if (porcelain) {
					result += name;
				} else {
					result += _T("--+--[ Lookup Result ]----------------------------------------\n");
					result += _T("  | Index for '") + lookup + _T("' is ") + name + _T("\n");
					result += _T("--+-----------------------------------------------------------");
				}
			} catch (const PDH::pdh_exception &e) {
				result += _T("Failed to lookup index: ") + utf8::cvt<std::wstring>(e.reason());
				return NSCAPI::hasFailed;
			}
		} else if (vm.count("expand-path")) {
			try {
				if (porcelain) {
					BOOST_FOREACH(const std::wstring &s, PDH::PDHResolver::PdhExpandCounterPath(lookup)) {
						result += s + _T("\n");
					}
				} else {
					result += _T("--+--[ Lookup Result ]----------------------------------------");
					BOOST_FOREACH(const std::wstring &s, PDH::PDHResolver::PdhExpandCounterPath(lookup)) {
						result += _T("  | Found '") + s + _T("\n");
					}
				}
			} catch (const PDH::pdh_exception &e) {
				result += _T("Failed to lookup index: ") + utf8::cvt<std::wstring>(e.reason());
				return NSCAPI::hasFailed;
			}
		} else {
			std::stringstream ss;
			ss << "pdh Command line syntax:" << std::endl;
			ss << desc;
			result = utf8::cvt<std::wstring>(ss.str());
			return NSCAPI::isSuccess;
		}
	}
	return 0;
}

class cpuload_handler {
public:
	static int parse(std::string s) {
		std::string::size_type pos = s.find_first_not_of("0123456789");
		return strEx::s::stox<int>(s.substr(0, pos));
	}
	static int parse_percent(std::string s) {
		std::string::size_type pos = s.find_first_not_of("0123456789");
		return strEx::s::stox<int>(s.substr(0, pos));
	}
	static std::string print(int value) {
		return strEx::s::xtos(value) + "%";
	}
	static std::string print_unformated(int value) {
		return strEx::s::xtos(value);
	}

	static std::string get_perf_unit(__int64 value) {
		return "%";
	}
	static std::string print_perf(__int64 value, std::string unit) {
		return strEx::s::xtos(value);
	}
	static std::string print_percent(int value) {
		return strEx::s::xtos(value) + "%";
	}
	static std::string key_prefix() {
		return "average load ";
	}
	static std::string key_postfix() {
		return "";
	}
};
NSCAPI::nagiosReturn CheckSystem::check_cpu(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf) {
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, cpuload_handler> > > CPULoadContainer;

	if (arguments.empty()) {
		msg = "ERROR: Usage: check_cpu <threshold> <time1> [<time2>...] (check_cpu MaxWarn=80 time=5m)";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<CPULoadContainer> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	CPULoadContainer tmpObject;

	tmpObject.data = "cpuload";

	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_NUMERIC_ALL(tmpObject, "")
		MAP_OPTIONS_STR("warn", tmpObject.warn.max_)
		MAP_OPTIONS_STR("crit", tmpObject.crit.max_)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR_AND("time", tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_STR_AND("Time", tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
			else if (p2.first == "Time") {
				tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				list.push_back(tmpObject);
			}
	MAP_OPTIONS_MISSING_EX(p2, msg, "Unknown argument: ")
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_FALLBACK_AND(tmpObject.data, list.push_back(tmpObject))
	MAP_OPTIONS_END()

	for (std::list<CPULoadContainer>::const_iterator it = list.begin(); it != list.end(); ++it) {
		CPULoadContainer load = (*it);
		int value = pdh_collector.getCPUAvrage(load.data + "m");
		if (value == -1) {
			msg = "ERROR: Could not get data for " + load.getAlias() + " please check log for details";
			return NSCAPI::returnUNKNOWN;
		}
		if (bNSClient) {
			if (!msg.empty()) msg += "&";
			msg += strEx::s::xtos(value);
		} else {
			load.setDefault(tmpObject);
			load.perfData = bPerfData;
			load.runCheck(value, returnCode, msg, perf);
		}
	}

	if (msg.empty())
		msg = "OK CPU Load ok.";
	else if (!bNSClient)
		msg = nscapi::plugin_helper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}

NSCAPI::nagiosReturn CheckSystem::check_uptime(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsTime> UpTimeContainer;

	if (arguments.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	UpTimeContainer bounds;

	bounds.data = "uptime";

	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_NUMERIC_ALL(bounds, "")
		MAP_OPTIONS_STR("warn", bounds.warn.min_)
		MAP_OPTIONS_STR("crit", bounds.crit.min_)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR("Alias", bounds.data)
		MAP_OPTIONS_SHOWALL(bounds)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_MISSING(msg, "Unknown argument: ")
		MAP_OPTIONS_END()


	unsigned long long value = pdh_collector.getUptime();
	if (value == -1) {
		msg = "ERROR: Could not get value";
		return NSCAPI::returnUNKNOWN;
	}
	if (bNSClient) {
		msg = strEx::s::xtos(value);
	} else {
		value *= 1000;
		bounds.perfData = bPerfData;
		bounds.runCheck(value, returnCode, msg, perf);
	}

	if (msg.empty())
		msg = "OK all counters within bounds.";
	else if (!bNSClient)
		msg = nscapi::plugin_helper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}

// @todo state_handler


inline int get_state(DWORD state) {
	if (state == SERVICE_RUNNING)
		return checkHolders::state_started;
	else if (state == SERVICE_STOPPED)
		return checkHolders::state_stopped;
	else if (state == MY_SERVICE_NOT_FOUND)
		return checkHolders::state_not_found;
	return checkHolders::state_none;
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
NSCAPI::nagiosReturn CheckSystem::check_service(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::SimpleBoundsStateBoundsInteger> StateContainer;
	if (arguments.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<StateContainer> list;
	std::set<std::string> excludeList;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	StateContainer tmpObject;
	bool bPerfData = true;
	bool bAutoStart = false;
	unsigned int truncate = 0;

	tmpObject.data = "service";
	tmpObject.crit.state = "started";
	//{{
	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_STR("Alias", tmpObject.data)
		MAP_OPTIONS_STR2INT("truncate", truncate)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE("CheckAll", bAutoStart)
		MAP_OPTIONS_INSERT("exclude", excludeList)
		//MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
		//MAP_OPTIONS_MISSING_EX(p2, msg, _T("Unknown argument: "))
		//MAP_OPTIONS_SECONDARY_END()
		else { 
			tmpObject.data = p__.first;
			if (p__.second.empty())
				tmpObject.crit.state = "started"; 
			else
				tmpObject.crit.state = p__.second; 
			list.push_back(tmpObject); 
		}
	MAP_OPTIONS_END()
	//}}
	if (bAutoStart) {
		// get a list of all service with startup type Automatic 
// 		;check_all_services[SERVICE_BOOT_START]=ignored
// 		;check_all_services[SERVICE_SYSTEM_START]=ignored
// 		;check_all_services[SERVICE_AUTO_START]=started
// 		;check_all_services[SERVICE_DEMAND_START]=ignored
// 		;check_all_services[SERVICE_DISABLED]=stopped
// 		std::wstring wantedMethod = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_ENUMPROC_METHOD, C_SYSTEM_ENUMPROC_METHOD_DEFAULT);

		bool vista = systemInfo::isAboveVista(systemInfo::getOSVersion());
		std::list<TNtServiceInfo> service_list_automatic = TNtServiceInfo::EnumServices(SERVICE_WIN32,SERVICE_INACTIVE|SERVICE_ACTIVE, vista); 
		BOOST_FOREACH(const TNtServiceInfo &service, service_list_automatic) {
			if (excludeList.find(service.m_strServiceName) == excludeList.end()) {
				tmpObject.data = service.m_strServiceName;
				std::string x = lookups_[service.m_dwStartType];
				if (x != "ignored") {
					tmpObject.crit.state = x;
					list.push_back(tmpObject); 
				}
			}
		} 
		tmpObject.crit.state = "ignored";
	}
	for (std::list<StateContainer>::iterator it = list.begin(); it != list.end(); ++it) {
		TNtServiceInfo info;
		if (bNSClient) {
			try {
				info = TNtServiceInfo::GetService((*it).data.c_str());
			} catch (NTServiceException e) {
				if (!msg.empty()) msg += " - ";
				msg += (*it).data + ": Error";
				nscapi::plugin_helper::escalteReturnCodeToWARN(returnCode);
				continue;
			}
			if ((*it).crit.state.hasBounds()) {
				bool ok = (*it).crit.state.check(get_state(info.m_dwCurrentState));
				if (!ok || (*it).showAll()) {
					if (info.m_dwCurrentState == SERVICE_RUNNING) {
						if (!msg.empty()) msg += " - ";
						msg += (*it).data + ": Started";
					} else if (info.m_dwCurrentState == SERVICE_STOPPED) {
						if (!msg.empty()) msg += " - ";
						msg += (*it).data + ": Stopped";
					} else if (info.m_dwCurrentState == MY_SERVICE_NOT_FOUND) {
						if (!msg.empty()) msg += " - ";
						msg += (*it).data + ": Not found";
					} else if (info.m_dwCurrentState == SERVICE_START_PENDING) {
						if (!msg.empty()) msg += " - ";
						msg += (*it).data + ": Start pending";
					} else if (info.m_dwCurrentState == SERVICE_STOP_PENDING) {
						if (!msg.empty()) msg += " - ";
						msg += (*it).data + ": Stop pending";
					} else {
						if (!msg.empty()) msg += " - ";
						msg += (*it).data + ": Unknown";
					}
					if (!ok) 
						nscapi::plugin_helper::escalteReturnCodeToCRIT(returnCode);
				}
			}
		} else {
			try {
				info = TNtServiceInfo::GetService((*it).data.c_str());
			} catch (const NTServiceException &e) {
				NSC_LOG_ERROR_EXR("Service enumeration faied", e);
				msg = e.what();
				return NSCAPI::returnUNKNOWN;
			}
			checkHolders::state_type value;
			if (info.m_dwCurrentState == SERVICE_RUNNING)
				value = checkHolders::state_started;
			else if (info.m_dwCurrentState == SERVICE_STOPPED)
				value = checkHolders::state_stopped;
			else if (info.m_dwCurrentState == SERVICE_STOP_PENDING)
				value = checkHolders::state_started|checkHolders::state_pending_other;
			else if (info.m_dwCurrentState == SERVICE_START_PENDING)
				value = checkHolders::state_stopped|checkHolders::state_pending_other;
			else if (info.m_dwCurrentState == MY_SERVICE_NOT_FOUND)
				value = checkHolders::state_not_found;
			else {
				NSC_LOG_MESSAGE("Service had no (valid) state: " + utf8::cvt<std::string>((*it).data) + " (" + strEx::s::xtos(info.m_dwCurrentState) + ")");
				value = checkHolders::state_none;
			}
			unsigned int x = returnCode;
			(*it).perfData = bPerfData;
			(*it).setDefault(tmpObject);
			(*it).runCheck(value, returnCode, msg, perf);
//			NSC_LOG_MESSAGE(_T("Service: ") + (*it).data + _T(" (") + strEx::itos(info.m_dwCurrentState) + _T(":") + strEx::itos((*it).warn.state.value_) + _T(":") + strEx::itos((*it).crit.state.value_) + _T(") -- (") + strEx::itos(returnCode) + _T(":") + strEx::itos(x) + _T(")"));
		}
	}
	if ((truncate > 0) && (msg.length() > (truncate-4)))
		msg = msg.substr(0, truncate-4) + "...";
	if (msg.empty() && returnCode == NSCAPI::returnOK)
		msg = "OK: All services are in their appropriate state.";
	else if (msg.empty())
		msg = nscapi::plugin_helper::translateReturn(returnCode) + ": Whooha this is odd.";
	else if (!bNSClient)
		msg = nscapi::plugin_helper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
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
NSCAPI::nagiosReturn CheckSystem::check_memory(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericPercentageBounds<checkHolders::PercentageValueType<unsigned __int64, unsigned __int64>, checkHolders::disk_size_handler<unsigned __int64> > > > MemoryContainer;
	if (arguments.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}

	std::list<MemoryContainer> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bPerfData = true;
	bool bNSClient = false;
	MemoryContainer tmpObject;

	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_STR_AND("type", tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_STR_AND("Type", tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
		MAP_OPTIONS_SECONDARY_STR_AND(p2,"type", tmpObject.data, tmpObject.alias, list.push_back(tmpObject))
			MAP_OPTIONS_MISSING_EX(p2, msg, "Unknown argument: ")
			MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_DISK_ALL(tmpObject, "", "Free", "Used")
		MAP_OPTIONS_STR("Alias", tmpObject.data)
		MAP_OPTIONS_STR("perf-unit", tmpObject.perf_unit)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_MISSING(msg, "Unknown argument: ")
		MAP_OPTIONS_END()

	if (bNSClient) {
		tmpObject.data = "paged";
		list.push_back(tmpObject);
	}

	checkHolders::PercentageValueType<unsigned long long, unsigned long long> dataPaged;
	CheckMemory::memData data;
	bool firstPaged = true;
	bool firstMem = true;
	for (std::list<MemoryContainer>::const_iterator pit = list.begin(); pit != list.end(); ++pit) {
		MemoryContainer check = (*pit);
		check.setDefault(tmpObject);
		checkHolders::PercentageValueType<unsigned long long, unsigned long long> value;
		if (firstPaged && (check.data == "paged")) {
			firstPaged = false;
			dataPaged.value = pdh_collector.getMemCommit();
			if (dataPaged.value == -1) {
				msg = "ERROR: Failed to get PDH value.";
				return NSCAPI::returnUNKNOWN;
			}
			dataPaged.total = pdh_collector.getMemCommitLimit();
			if (dataPaged.total == -1) {
				msg = "ERROR: Failed to get PDH value.";
				return NSCAPI::returnUNKNOWN;
			}
		} else if (firstMem) {
			try {
				data = memoryChecker.getMemoryStatus();
			} catch (CheckMemoryException e) {
				msg = e.reason();
				return NSCAPI::returnCRIT;
			}
		}

		if (check.data == "page") {
			value.value = data.pageFile.total-data.pageFile.avail; // mem.dwTotalPageFile-mem.dwAvailPageFile;
			value.total = data.pageFile.total; //mem.dwTotalPageFile;
			if (check.alias.empty())
				check.alias = "page file";
		} else if (check.data == "physical") {
			value.value = data.phys.total-data.phys.avail; //mem.dwTotalPhys-mem.dwAvailPhys;
			value.total = data.phys.total; //mem.dwTotalPhys;
			if (check.alias.empty())
				check.alias = "physical memory";
		} else if (check.data == "virtual") {
			value.value = data.virtualMem.total-data.virtualMem.avail;//mem.dwTotalVirtual-mem.dwAvailVirtual;
			value.total = data.virtualMem.total;//mem.dwTotalVirtual;
			if (check.alias.empty())
				check.alias = "virtual memory";
		} else  if (check.data == "paged") {
			value.value = dataPaged.value;
			value.total = dataPaged.total;
			if (check.alias.empty())
				check.alias = "paged bytes";
		} else {
			msg = check.data + " is not a known check...";
			return NSCAPI::returnCRIT;
		}
		if (bNSClient) {
			msg = strEx::s::xtos(value.total) + "&" + strEx::s::xtos(value.value);
			return NSCAPI::returnOK;
		} else {
			check.perfData = bPerfData;
			check.runCheck(value, returnCode, msg, perf);
		}
	}

	if (msg.empty())
		msg = "OK memory within bounds.";
	else
		msg = nscapi::plugin_helper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}
typedef struct NSPROCDATA__ {
	unsigned int count;
	unsigned int hung_count;
	CEnumProcess::CProcessEntry entry;
	std::string key;

	NSPROCDATA__() : count(0), hung_count(0) {}
	NSPROCDATA__(const NSPROCDATA__ &other) : count(other.count), hung_count(other.hung_count), entry(other.entry), key(other.key) {}
} NSPROCDATA;
typedef std::map<std::string,NSPROCDATA> NSPROCLST;

class NSC_error : public CEnumProcess::error_reporter {
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

/**
* Get a hash_map with all running processes.
* @return a hash_map with all running processes
*/
NSPROCLST GetProcessList(bool getCmdLines, bool use16Bit)
{
	NSPROCLST ret;
	CEnumProcess enumeration;
	if (!enumeration.has_PSAPI()) {
		NSC_LOG_ERROR_STD("Failed to enumerat processes");
		NSC_LOG_ERROR_STD("PSAPI method not availabletry installing \"Platform SDK Redistributable: PSAPI for Windows NT\" from Microsoft.");
		NSC_LOG_ERROR_STD("Try this URL: http://www.microsoft.com/downloads/details.aspx?FamilyID=3d1fbaed-d122-45cf-9d46-1cae384097ac");
		throw CEnumProcess::process_enumeration_exception("PSAPI not available, please see error log for details.");
	}
	NSC_error err;
	CEnumProcess::process_list list = enumeration.enumerate_processes(getCmdLines, use16Bit, &err);
	for (CEnumProcess::process_list::const_iterator entry = list.begin(); entry != list.end(); ++entry) {
		std::string key;
		if (getCmdLines && !(*entry).command_line.empty())
			key = (*entry).command_line;
		else
			key = boost::to_lower_copy((*entry).filename);
		NSPROCLST::iterator it = ret.find(key);
		if (it == ret.end()) {
			ret[key].entry = (*entry);
			ret[key].count = 1;
			ret[key].hung_count = (*entry).hung?1:0;
			ret[key].key = key;
		} else {
			if ((*entry).hung) 
				(*it).second.hung_count++;
			(*it).second.count++;
		}
	}
	return ret;
}



struct process_count_result {
	unsigned long running;
	unsigned long hung;
	process_count_result() : running(0), hung(0) {}

	std::string format_value(std::string tag, unsigned long count) {
		if (count > 1)
			return tag + "(" + strEx::s::xtos(count) + ")";
		if (count == 1)
			return tag;
		return "";
	}
	std::string to_string() {
		if (running > 0 && hung > 0)
			return format_value("running", running) + ", " + format_value("hung", hung);
		if (running > 0)
			return format_value("running", running);
		if (hung > 0)
			return format_value("hung", hung);
		return "stopped";
	}
	std::string to_string_short() {
		if (running > 0 && hung > 0)
			return "running, hung";
		if (running > 0)
			return "running";
		if (hung > 0)
			return "hung";
		return "stopped";
	}

};

class ProcessBound {
public:
	checkHolders::ExactBounds<checkHolders::NumericBounds<unsigned long, checkHolders::int_handler> > running;
	checkHolders::ExactBounds<checkHolders::NumericBounds<unsigned long, checkHolders::int_handler> > hung;
	typedef checkHolders::NumericBounds<unsigned long, checkHolders::int_handler> THolder;

	typedef ProcessBound TMyType;
	typedef process_count_result TValueType;

	ProcessBound() {}
	ProcessBound(const ProcessBound &other) {
		running = other.running;
		hung = other.hung;
	}

	void reset() {
		running.reset();
		hung.reset();
	}
	bool hasBounds() {
		return running.hasBounds() || hung.hasBounds();
	}
	static std::string toStringLong(TValueType &value) {
		return value.to_string();
	}
	static std::string toStringShort(TValueType &value) {
		return value.to_string_short();
	}
	std::string gatherPerfData(std::string alias, std::string unit, TValueType &value, TMyType &warn, TMyType &crit) {
		if (hung.hasBounds())
			return hung.gatherPerfData(alias, unit, value.hung, warn.hung, crit.hung);
		return running.gatherPerfData(alias, unit, value.running, warn.running, crit.running);
	}
	std::string gatherPerfData(std::string alias, std::string unit, TValueType &value) {
		THolder tmp;
		if (hung.hasBounds())
			return tmp.gatherPerfData(alias, unit, value.hung);
		return tmp.gatherPerfData(alias, unit, value.running);
	}
	bool check(TValueType &value, std::string lable, std::string &message, checkHolders::ResultType type) {
		if (hung.hasBounds()) {
			return hung.check_preformatted(value.hung, value.to_string(), lable, message, type);
		} else {
			return running.check_preformatted(value.running, value.to_string(), lable, message, type);
		}
	}

};
/**
 * Check process state and return result
 *
 * @param command Command to execute
 * @param argLen The length of the argument buffer
 * @param **char_args The argument buffer
 * @param &msg String to put message in
 * @param &perf String to put performance data in 
 * @return The status of the command
 */
NSCAPI::nagiosReturn CheckSystem::check_process(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf)
{
	typedef checkHolders::CheckContainer<ProcessBound> StateContainer;
	
//	typedef checkHolders::CheckContainer<checkHolders::ExactBoundsState> StateContainer2;

	if (arguments.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<StateContainer> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	StateContainer tmpObject;
	bool bPerfData = true;
	bool use16bit = false;
	bool useCmdLine = false;
	bool ignoreState = false;
	typedef enum {
		match_string, match_substring, match_regexp
	} match_type;
	match_type match = match_string;

	

	//tmpObject.data = _T("uptime");
	//tmpObject.crit.min = 1;

	MAP_OPTIONS_BEGIN(arguments)
		//MAP_OPTIONS_NUMERIC_ALL(tmpObject, _T("Count"))
		MAP_OPTIONS_EXACT_NUMERIC_ALL_EX(tmpObject, "Count", running)
		MAP_OPTIONS_EXACT_NUMERIC_LEGACY_EX(tmpObject, "Count", running)
		MAP_OPTIONS_EXACT_NUMERIC_ALL_EX(tmpObject, "HungCount", hung)
		MAP_OPTIONS_EXACT_NUMERIC_LEGACY_EX(tmpObject, "HungCount", hung)
		MAP_OPTIONS_STR("Alias", tmpObject.alias)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE("ignore-state", ignoreState)
		MAP_OPTIONS_BOOL_TRUE("cmdLine", useCmdLine)
		MAP_OPTIONS_BOOL_TRUE("16bit", use16bit)
		MAP_OPTIONS_MODE("match", "string", match,  match_string)
		MAP_OPTIONS_MODE("match", "regexp", match,  match_regexp)
		MAP_OPTIONS_MODE("match", "substr", match,  match_substring)
		MAP_OPTIONS_MODE("match", "substring", match,  match_substring)
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
	else if (p2.first == "Proc") {
			tmpObject.data = p__.second;
			tmpObject.alias = p2.second;
			
			list.push_back(tmpObject);
		}
	MAP_OPTIONS_MISSING_EX(p2, msg, "Unknown argument: ")
		MAP_OPTIONS_SECONDARY_END()
		else { 
			tmpObject.data = p__.first;
			if (p__.second.empty()) {
				if (!tmpObject.crit.running.hasBounds() && !tmpObject.warn.running.hasBounds())
					tmpObject.crit.running.min = "1"; 
			} else if (p__.second == "started") {
				if (!tmpObject.crit.running.hasBounds() && !tmpObject.warn.running.hasBounds())
					tmpObject.crit.running.min = "1";
			} else if (p__.second == "stopped") {
				if (!tmpObject.crit.running.hasBounds() && !tmpObject.warn.running.hasBounds())
					tmpObject.crit.running.max = "0";
			} else if (p__.second == "hung") {
				if (!tmpObject.crit.hung.hasBounds() && !tmpObject.warn.hung.hasBounds())
					tmpObject.crit.hung.max = "1";
			}
			list.push_back(tmpObject);
			tmpObject.reset();
		}
	MAP_OPTIONS_END()

	NSPROCLST runningProcs;
	try {
		runningProcs = GetProcessList(useCmdLine, use16bit);
	} catch (CEnumProcess::process_enumeration_exception &e) {
		NSC_LOG_ERROR("ERROR: " + e.reason());
		msg = "ERROR: " + e.reason();
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		NSC_LOG_ERROR_EX("Unhandled error when processing command");
		msg = "Unhandled error when processing command";
		return NSCAPI::returnUNKNOWN;
	}

	for (std::list<StateContainer>::iterator it = list.begin(); it != list.end(); ++it) {
		NSPROCLST::iterator proc;
		std::string key = boost::to_lower_copy((*it).data);
		if (match == match_string) {
			proc = runningProcs.find(key);
		} else if (match == match_substring) {
			for (proc=runningProcs.begin();proc!=runningProcs.end();++proc) {
				if ((*proc).first.find(key) != std::wstring::npos)
					break;
			}
		} else if (match == match_regexp) {
			try {
				boost::regex filter(key,boost::regex::icase);
				BOOST_FOREACH(const NSPROCLST::value_type &p, runningProcs) {
					if (boost::regex_match(p.first, filter))
						break;
				}
			} catch (const boost::bad_expression e) {
				NSC_LOG_ERROR_EXR("Failed to compile regular expression: " + proc->first, e);
				msg = "Failed to compile regular expression: " + proc->first;
				return NSCAPI::returnUNKNOWN;
			} catch (...) {
				NSC_LOG_ERROR_EX("Failed to compile regular expression");
				msg = "Failed to compile regular expression: " + proc->first;
				return NSCAPI::returnUNKNOWN;
			}
		} else {
			NSC_LOG_ERROR(std::string("Unsupported mode for: ") + proc->first);
			msg = "Unsupported mode for: " + (*proc).first;
			return NSCAPI::returnUNKNOWN;
		}
		bool bFound = (proc != runningProcs.end());
		if (bNSClient) {
			if (bFound && (*it).showAll()) {
				if (!msg.empty()) msg += " - ";
				msg += (*proc).first + ": Running";
			} else if (bFound) {
			} else {
				if (!msg.empty()) msg += " - ";
				msg += (*it).data + ": not running";
				nscapi::plugin_helper::escalteReturnCodeToCRIT(returnCode);
			}
		} else {
			process_count_result value;
			if (bFound) {
				value.hung = (*proc).second.hung_count;
				value.running = (*proc).second.count;
			} else {
				value.hung = 0;
				value.running = 0;
//				if (ignoreState)
//					value.state = checkHolders::state_stopped | checkHolders::state_started | checkHolders::state_hung;
//				else
//				value.state = checkHolders::state_stopped;
			}
			if (bFound && (*it).alias.empty()) {
				(*it).alias = (*proc).first;
			}
			(*it).perfData = bPerfData;
			(*it).runCheck(value, returnCode, msg, perf);
		}

	}
	if (msg.empty())
		msg = "OK: All processes are running.";
	else if (!bNSClient)
		msg = nscapi::plugin_helper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}

template<class T>
class PerfDataContainer : public checkHolders::CheckContainer<T> {
private:
	typedef PDHCollectors::StaticPDHCounterListener<double, PDH_FMT_DOUBLE> counter_type;
	typedef boost::shared_ptr<counter_type> ptr_lsnr_type;
	ptr_lsnr_type cDouble;
public:

	PerfDataContainer() : CheckContainer<T>() {}

	PerfDataContainer(const PerfDataContainer &other) : CheckContainer<T>(other), cDouble(other.cDouble) {}
	const PerfDataContainer& operator =(const PerfDataContainer &other) {
		*((CheckContainer<T>*)this) = other;
		cDouble = other.cDouble;
		return *this;
	}
	ptr_lsnr_type get_listener() {
		if (!cDouble)
			cDouble = ptr_lsnr_type(new counter_type());
		return cDouble;
	}
};

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
NSCAPI::nagiosReturn CheckSystem::check_pdh(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf)
{
	typedef PerfDataContainer<checkHolders::MaxMinBoundsDouble> CounterContainer;

	if (arguments.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<CounterContainer> counters;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	/* average maax */
	bool bCheckAverages = true; 
	std::string invalidStatus = "UNKNOWN";
	unsigned int averageDelay = 1000;
	CounterContainer tmpObject;
	bool bExpandIndex = false;
	bool bForceReload = false;
	std::string extra_format;

	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_STR("InvalidStatus", invalidStatus)
		MAP_OPTIONS_STR_AND("Counter", tmpObject.data, counters.push_back(tmpObject))
		MAP_OPTIONS_STR("MaxWarn", tmpObject.warn.max_)
		MAP_OPTIONS_STR("MinWarn", tmpObject.warn.min_)
		MAP_OPTIONS_STR("MaxCrit", tmpObject.crit.max_)
		MAP_OPTIONS_STR("MinCrit", tmpObject.crit.min_)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR("Alias", tmpObject.data)
		MAP_OPTIONS_STR("format", extra_format)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_EX("Averages", bCheckAverages, "true", "false")
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE("index", bExpandIndex)
		MAP_OPTIONS_BOOL_TRUE("reload", bForceReload)
		MAP_OPTIONS_FIRST_CHAR('\\', tmpObject.data, counters.push_back(tmpObject))
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
	else if (p2.first == "Counter") {
		tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				counters.push_back(tmpObject);
			}
	MAP_OPTIONS_MISSING_EX(p2, msg, "Unknown argument: ")
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_FALLBACK_AND(tmpObject.data, counters.push_back(tmpObject))
	MAP_OPTIONS_END()

	if (counters.empty()) {
		msg = "No counters specified";
		return NSCAPI::returnUNKNOWN;
	}
	PDH::PDHQuery pdh;

	bool has_counter = false;
	BOOST_FOREACH(CounterContainer &counter, counters) {
		try {
			if (counter.data.find('\\') == std::wstring::npos) {
			} else {
				std::wstring tstr;
				if (bExpandIndex) {
					PDH::PDHResolver::expand_index(utf8::cvt<std::wstring>(counter.data));
				}
				if (!PDH::PDHResolver::validate(utf8::cvt<std::wstring>(counter.data), tstr, bForceReload)) {
					NSC_LOG_ERROR("ERROR: Counter not found: " + counter.data + ": " + utf8::cvt<std::string>(tstr));
					if (bNSClient) {
						NSC_LOG_ERROR("ERROR: Counter not found: " + counter.data + ": " + utf8::cvt<std::string>(tstr));
						//msg = _T("0");
					} else {
						msg = "CRIT: Counter not found: " + counter.data + ": " + utf8::cvt<std::string>(tstr);
						return NSCAPI::returnCRIT;
					}
				}
				if (!extra_format.empty()) {
					boost::char_separator<char> sep(",");
					boost::tokenizer<boost::char_separator<char>, std::string::const_iterator, std::string> tokens(extra_format, sep);
					DWORD flags = 0;
					BOOST_FOREACH(const std::string &t, tokens) {
						if (t == "nocap100")
							flags |= PDH_FMT_NOCAP100;
						else if (t == "1000")
							flags |= PDH_FMT_1000;
						else if (t == "noscale")
							flags |= PDH_FMT_NOSCALE;
						else {
							NSC_LOG_ERROR("Unsupported extrta format: " + t);
						}
					}
					counter.get_listener()->set_extra_format(flags);
				}
				pdh.addCounter(utf8::cvt<std::wstring>(counter.data), counter.get_listener());
				has_counter = true;
			}
		} catch (const std::exception &e) {
			NSC_LOG_ERROR_EXR("Failed to poll counter", e);
			if (bNSClient)
				msg = "0";
			else
				msg = std::string("ERROR: ") + utf8::utf8_from_native(e.what()) + " (" + counter.getAlias() + "|" + counter.data + ")";
			return NSCAPI::returnUNKNOWN;
		}
	}
	if (has_counter) {
		try {
			pdh.open();
			if (bCheckAverages) {
				pdh.collect();
				Sleep(1000);
			}
			pdh.gatherData();
			pdh.close();
		} catch (const PDH::pdh_exception &e) {
			NSC_LOG_ERROR_EXR("Failed to poll counter", e);
			if (bNSClient)
				msg = "0";
			else
				msg = "ERROR: " + utf8::utf8_from_native(e.what());
			return NSCAPI::returnUNKNOWN;
		}
	}
	BOOST_FOREACH(CounterContainer &counter, counters) {
		try {
			double value = 0;
			if (counter.data.find('\\') == std::string::npos) {
				value = pdh_collector.get_double(counter.data);
				if (value == -1) {
					msg = "ERROR: Failed to get counter value: " + counter.data;
					return NSCAPI::returnUNKNOWN;
				}
			} else {
				value = counter.get_listener()->getValue();
			}

			if (bNSClient) {
				if (!msg.empty())
					msg += ",";
				msg += strEx::s::xtos(value);
			} else {
				counter.perfData = bPerfData;
				counter.setDefault(tmpObject);
				counter.runCheck(value, returnCode, msg, perf);
			}
		} catch (const PDH::pdh_exception &e) {
			NSC_LOG_ERROR_EXR("ERROR", e);
			if (bNSClient)
				msg = "0";
			else
				msg = std::string("ERROR: ") + e.what()+ " (" + counter.getAlias() + "|" + counter.data + ")";
			return NSCAPI::returnUNKNOWN;
		}
	}

	if (msg.empty() && !bNSClient)
		msg = "OK all counters within bounds.";
	else if (msg.empty()) {
		NSC_LOG_ERROR_STD("No value found returning 0?");
		msg = "0";
	}else if (!bNSClient)
		msg = nscapi::plugin_helper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

typedef checkHolders::CheckContainer<checkHolders::ExactBoundsULong> ExactULongContainer;
typedef checkHolders::CheckContainer<checkHolders::ExactBoundsLongLong> ExactLongLongContainer;
typedef checkHolders::CheckContainer<checkHolders::ExactBoundsTime> DateTimeContainer;
typedef checkHolders::CheckContainer<FilterBounds<filters::filter_all_strings> > StringContainer;
