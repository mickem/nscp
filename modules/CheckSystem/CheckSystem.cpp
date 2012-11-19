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

#include <tlhelp32.h>


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
			NSC_LOG_MESSAGE_STD(_T("Missing bounds for filter check: ") + lable);
		}
		return false;
	}
	const TMyType & operator=(std::wstring value) {
		filter = value;
		return *this;
	}

};

namespace sh = nscapi::settings_helper;

boost::tuple<bool,std::wstring> validate_counter(std::wstring counter) {
	std::pair<bool,std::wstring> ret;
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
		pdh.addCounter(counter, collector);
		pdh.open();
		pdh.gatherData();
		pdh.close();
		return boost::make_tuple(true, _T("ok(") + strEx::itos(collector->getValue()) + _T(")"));
	} catch (const PDH::PDHException e) {
		try {
			pdh.gatherData();
			pdh.close();
			return boost::make_tuple(true, _T("ok-rate(") + strEx::itos(collector->getValue()) + _T(")"));
		} catch (const PDH::PDHException e) {
			return boost::make_tuple(false, _T("query failed: ") + e.getError());
		}
	}
}
std::wstring find_system_counter(std::wstring counter) {
	if (counter == PDH_SYSTEM_KEY_UPT) {
		wchar_t *keys[] = {_T("\\2\\674"), _T("\\System\\System Up Time"), _T("\\System\\Systembetriebszeit"), _T("\\Sistema\\Tempo di funzionamento sistema"), _T("\\Système\\Temps d'activité système")};
		BOOST_FOREACH(const wchar_t *key, keys) {
			boost::tuple<bool,std::wstring> result = validate_counter(key);
			if (result.get<0>()) {
				NSC_DEBUG_MSG(_T("Found alternate key for ") + counter + _T(": ") + key);
				return key;
			}
		}
		return keys[0];
	}
	if (counter == PDH_SYSTEM_KEY_MCL) {
		wchar_t *keys[] = {_T("\\4\\30"), _T("\\Memory\\Commit Limit"), _T("\\Speicher\\Zusagegrenze"), _T("\\Memoria\\Limite memoria vincolata"), _T("\\Mémoire\\Limite de mémoire dédiée")};
		BOOST_FOREACH(const wchar_t *key, keys) {
			boost::tuple<bool,std::wstring> result = validate_counter(key);
			if (result.get<0>()) {
				NSC_DEBUG_MSG(_T("Found alternate key for ") + counter + _T(": ") + key);
				return key;
			}
		}
		return keys[0];
	}
	if (counter == PDH_SYSTEM_KEY_MCB) {
		wchar_t *keys[] = {_T("\\4\\26"), _T("\\Memory\\Committed Bytes"), _T("\\Speicher\\Zugesicherte Bytes"), _T("\\Memoria\\Byte vincolati"), _T("\\Mémoire\\Octets dédiés")};
		BOOST_FOREACH(const wchar_t *key, keys) {
			boost::tuple<bool,std::wstring> result = validate_counter(key);
			if (result.get<0>()) {
				NSC_DEBUG_MSG(_T("Found alternate key for ") + counter + _T(": ") + key);
				return key;
			}
		}
		return keys[0];
	}
	if (counter == PDH_SYSTEM_KEY_CPU) {
		wchar_t *keys[] = {_T("\\238(_total)\\6"), _T("\\Processor(_total)\\% Processor Time"), _T("\\Prozessor(_Total)\\Prozessorzeit (%)"), _T("\\Processore(_total)\\% Tempo processore"), _T("\\Processeur(_Total)\\% Temps processeur")};
		BOOST_FOREACH(const wchar_t *key, keys) {
			boost::tuple<bool,std::wstring> result = validate_counter(key);
			if (result.get<0>()) {
				NSC_DEBUG_MSG(_T("Found alternate key for ") + counter + _T(": ") + key);
				return key;
			}
		}
		return keys[0];
	}
}


void load_counters(std::map<std::wstring,std::wstring> &counters, sh::settings_registry &settings) {
	settings.alias().add_path_to_settings()
		(_T("pdh/counters"), sh::wstring_map_path(&counters)
		, _T("PDH COUNTERS"), _T("Define various PDH counters to check."))
		;

	settings.register_all();
	settings.notify();
	settings.clear();

	std::wstring path = settings.alias().get_settings_path(_T("pdh/counters"));
	if (counters[PDH_SYSTEM_KEY_CPU] == _T("")) {
		settings.register_key(path + _T("/") + PDH_SYSTEM_KEY_CPU, _T("collection strategy"), NSCAPI::key_string, _T("Collection Strategy"), _T("Collection strategy for CPU is usually round robin, for others static."), _T("round robin"), false);
		settings.set_static_key(path + _T("/") + PDH_SYSTEM_KEY_CPU, _T("collection strategy"), _T("round robin"));
	}
	wchar_t *keys[] = {PDH_SYSTEM_KEY_UPT, PDH_SYSTEM_KEY_MCL, PDH_SYSTEM_KEY_MCB, PDH_SYSTEM_KEY_CPU};
	BOOST_FOREACH(const wchar_t *key, keys) {
		if (counters[key] == _T("")) {
			counters[key] = find_system_counter(key);
			settings.register_key(path, key, NSCAPI::key_string, key, _T("System counter for check_xx commands.."), counters[key], false);
		}
	}
}

/**
 * New version of the load call.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool CheckSystem::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	boost::shared_ptr<PDHCollector::system_counter_data> data(new PDHCollector::system_counter_data);
	data->check_intervall = 1;

	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(alias, _T("system/windows"));
	std::wstring counter_path = settings.alias().get_settings_path(_T("pdh/counters"));

	if (mode == NSCAPI::normalStart) {
		load_counters(counters, settings);
	}

	settings.alias().add_path_to_settings()
		(_T("WINDOWS CHECK SYSTEM"), _T("Section for system checks and system settings"))

		(_T("service mapping"), _T("SERVICE MAPPING SECTION"), _T("Confiure which services has to be in which state"))

		(_T("pdh"), _T("PDH COUNTER INFORMATION"), _T(""))

		;

	settings.alias().add_key_to_settings()
		(_T("default buffer length"), sh::wstring_key(&data->buffer_length, _T("1h")),
		_T("DEFAULT LENGTH"), _T("Used to define the default intervall for range buffer checks (ie. CPU)."))

		(_T("default intervall"), sh::uint_key(&data->check_intervall, 1),
		_T("DEFAULT INTERVALL"), _T("Used to define the default intervall for range buffer checks (ie. CPU)."), true)

		(_T("subsystem"), sh::wstring_key(&data->subsystem, _T("default")),
		_T("PDH SUBSYSTEM"), _T("Set which pdh subsystem to use."), true)
		;

	settings.alias().add_key_to_settings(_T("service mapping"))

		(_T("BOOT_START"), sh::wstring_vector_key(&lookups_, SERVICE_BOOT_START, _T("ignored")),
		_T("SERVICE_BOOT_START"), _T("TODO"), true)

		(_T("SYSTEM_START"), sh::wstring_vector_key(&lookups_, SERVICE_SYSTEM_START, _T("ignored")),
		_T("SERVICE_SYSTEM_START"), _T("TODO"), true)

		(_T("AUTO_START"), sh::wstring_vector_key(&lookups_, SERVICE_AUTO_START, _T("started")),
		_T("SERVICE_AUTO_START"), _T("TODO"), true)

		(_T("DEMAND_START"), sh::wstring_vector_key(&lookups_, SERVICE_DEMAND_START, _T("ignored")),
		_T("SERVICE_DEMAND_START"), _T("TODO"), true)

		(_T("DISABLED"), sh::wstring_vector_key(&lookups_, SERVICE_DISABLED, _T("stopped")),
		_T("SERVICE_DISABLED"), _T("TODO"), true)

		(_T("DELAYED"), sh::wstring_vector_key(&lookups_, NSCP_SERVICE_DELAYED, _T("ignored")),
		_T("SERVICE_DELAYED"), _T("TODO"), true)

		;

	settings.register_all();
	settings.notify();

		
	if (mode == NSCAPI::normalStart) {
		typedef PDHCollector::system_counter_data::counter cnt;
		BOOST_FOREACH(counter_map_type::value_type c, counters) {
			std::wstring path = c.second;
			boost::tuple<bool, std::wstring> result = validate_counter(path);
			if (!result.get<0>()) {
				NSC_LOG_ERROR(_T("Failed to load counter ") + c.first + _T("(") + path + _T(": ") + result.get<1>());
			}
			std::wstring strategy = get_core()->getSettingsString(counter_path + _T("/") + c.first, _T("collection strategy"), _T("static"));
			if (strategy == _T("static")) {
				data->counters.push_back(cnt(c.first, path, cnt::type_int64, cnt::format_large, cnt::value));
			} else if (strategy == _T("round robin")) {
				std::wstring size = get_core()->getSettingsString(counter_path + _T("/") + c.first, _T("size"), _T(""));
				if (size.empty())
					data->counters.push_back(cnt(c.first, path, cnt::type_int64, cnt::format_large, cnt::rrd));
				else
					data->counters.push_back(cnt(c.first, path, cnt::type_int64, cnt::format_large, cnt::rrd, size));
			} else {
				NSC_LOG_ERROR(_T("Failed to load counter ") + c.first + _T(" invalid collection strategy: ") + strategy);
			}
		}
	}

	//register_command(_T("listCounterInstances"), _T("*DEPRECATED* List all instances for a counter."));

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
		NSC_LOG_ERROR(_T("Could not exit the thread, memory leak and potential corruption may be the result..."));
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
						boost::tuple<bool,std::wstring> status;
						if (validate) {
							status = validate_counter(line);
							result += line + _T(": ") + status.get<1>() + _T("\n");
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
					boost::tuple<bool,std::wstring> status;
					if (validate) {
						status = validate_counter(line);
						result += line + _T(": ") + status.get<1>() + _T("\n");
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
	} catch (const PDH::PDHException e) {
		result = _T("ERROR: Service enumeration failed: ") + e.getError();
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
						settings.set_alias(_T("check"), _T("system/windows"), _T("system/windows"));
						load_counters(counters, settings);
					}
					if (!porcelain) {
						result += _T("Listing configured counters\n");
						result += _T("---------------------------\n");
					} 
					BOOST_FOREACH(const counter_map_type::value_type v, counters) {
						std::wstring line = v.first + _T(" = ") + v.second;
						boost::tuple<bool,std::wstring> status;
						count++;
						if (!counter.empty() && line.find(counter) == std::wstring::npos)
							continue;

						if (validate)
							status = validate_counter(v.second);

						if (porcelain) 
							line = v.first + _T(",") + v.second + _T(",") + status.get<1>();
						else if (validate)
							line = v.first + _T(" = ") + v.second + _T(": ") + status.get<1>();
						else 
							line = v.first + _T(" = ") + v.second;
						result += line + _T("\n");
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
			} catch (const PDH::PDHException e) {
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
			} catch (const PDH::PDHException e) {
				result += _T("Failed to lookup index: ") + e.getError();
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
			} catch (const PDH::PDHException e) {
				result += _T("Failed to lookup index: ") + e.getError();
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
	static int parse(std::wstring s) {
		return strEx::stoi(s);
	}
	static int parse_percent(std::wstring s) {
		return strEx::stoi(s);
	}
	static std::wstring print(int value) {
		return strEx::itos(value) + _T("%");
	}
	static std::wstring print_unformated(int value) {
		return strEx::itos(value);
	}

	static std::wstring get_perf_unit(__int64 value) {
		return _T("%");
	}
	static std::wstring print_perf(__int64 value, std::wstring unit) {
		return boost::lexical_cast<std::wstring>(value);
	}
	static std::wstring print_percent(int value) {
		return boost::lexical_cast<std::wstring>(value) + _T("%");
	}
	static std::wstring key_prefix() {
		return _T("average load ");
	}
	static std::wstring key_postfix() {
		return _T("");
	}
};
NSCAPI::nagiosReturn CheckSystem::check_cpu(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf) {
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, cpuload_handler> > > CPULoadContainer;

	if (arguments.empty()) {
		msg = _T("ERROR: Usage: check_cpu <threshold> <time1> [<time2>...] (check_cpu MaxWarn=80 time=5m)");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<CPULoadContainer> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	CPULoadContainer tmpObject;

	tmpObject.data = _T("cpuload");

	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_NUMERIC_ALL(tmpObject, _T(""))
		MAP_OPTIONS_STR(_T("warn"), tmpObject.warn.max_)
		MAP_OPTIONS_STR(_T("crit"), tmpObject.crit.max_)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR_AND(_T("time"), tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_STR_AND(_T("Time"), tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
			else if (p2.first == _T("Time")) {
				tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				list.push_back(tmpObject);
			}
	MAP_OPTIONS_MISSING_EX(p2, msg, _T("Unknown argument: "))
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_FALLBACK_AND(tmpObject.data, list.push_back(tmpObject))
	MAP_OPTIONS_END()

	for (std::list<CPULoadContainer>::const_iterator it = list.begin(); it != list.end(); ++it) {
		CPULoadContainer load = (*it);
		int value = pdh_collector.getCPUAvrage(load.data + _T("m"));
		if (value == -1) {
			msg = _T("ERROR: Could not get data for ") + load.getAlias() + _T(" perhaps we don't collect data this far back?");
			return NSCAPI::returnUNKNOWN;
		}
		if (bNSClient) {
			if (!msg.empty()) msg += _T("&");
			msg += strEx::itos(value);
		} else {
			load.setDefault(tmpObject);
			load.perfData = bPerfData;
			load.runCheck(value, returnCode, msg, perf);
		}
	}

	if (msg.empty())
		msg = _T("OK CPU Load ok.");
	else if (!bNSClient)
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
	return returnCode;
}

NSCAPI::nagiosReturn CheckSystem::check_uptime(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsTime> UpTimeContainer;

	if (arguments.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	UpTimeContainer bounds;

	bounds.data = _T("uptime");

	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_NUMERIC_ALL(bounds, _T(""))
		MAP_OPTIONS_STR(_T("warn"), bounds.warn.min_)
		MAP_OPTIONS_STR(_T("crit"), bounds.crit.min_)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR(_T("Alias"), bounds.data)
		MAP_OPTIONS_SHOWALL(bounds)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_MISSING(msg, _T("Unknown argument: "))
		MAP_OPTIONS_END()


	unsigned long long value = pdh_collector.getUptime();
	if (value == -1) {
		msg = _T("ERROR: Could not get value");
		return NSCAPI::returnUNKNOWN;
	}
	if (bNSClient) {
		msg = strEx::itos(value);
	} else {
		value *= 1000;
		bounds.perfData = bPerfData;
		bounds.runCheck(value, returnCode, msg, perf);
	}

	if (msg.empty())
		msg = _T("OK all counters within bounds.");
	else if (!bNSClient)
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
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
NSCAPI::nagiosReturn CheckSystem::check_service(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::SimpleBoundsStateBoundsInteger> StateContainer;
	if (arguments.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<StateContainer> list;
	std::set<std::wstring> excludeList;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	StateContainer tmpObject;
	bool bPerfData = true;
	bool bAutoStart = false;
	unsigned int truncate = 0;

	tmpObject.data = _T("service");
	tmpObject.crit.state = _T("started");
	//{{
	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_STR(_T("Alias"), tmpObject.data)
		MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE(_T("CheckAll"), bAutoStart)
		MAP_OPTIONS_INSERT(_T("exclude"), excludeList)
		//MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
		//MAP_OPTIONS_MISSING_EX(p2, msg, _T("Unknown argument: "))
		//MAP_OPTIONS_SECONDARY_END()
		else { 
			tmpObject.data = p__.first;
			if (p__.second.empty())
				tmpObject.crit.state = _T("started"); 
			else
				tmpObject.crit.state = p__.second; 
			list.push_back(tmpObject); 
		}
	MAP_OPTIONS_END()
	//}}
	if (bAutoStart) {
		// get a list of all service with startup type Automatic 
		/*
		;check_all_services[SERVICE_BOOT_START]=ignored
		;check_all_services[SERVICE_SYSTEM_START]=ignored
		;check_all_services[SERVICE_AUTO_START]=started
		;check_all_services[SERVICE_DEMAND_START]=ignored
		;check_all_services[SERVICE_DISABLED]=stopped
		std::wstring wantedMethod = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_ENUMPROC_METHOD, C_SYSTEM_ENUMPROC_METHOD_DEFAULT);
		*/

		bool vista = systemInfo::isAboveVista(systemInfo::getOSVersion());
		std::list<TNtServiceInfo> service_list_automatic = TNtServiceInfo::EnumServices(SERVICE_WIN32,SERVICE_INACTIVE|SERVICE_ACTIVE, vista); 
		for (std::list<TNtServiceInfo>::const_iterator service =service_list_automatic.begin();service!=service_list_automatic.end();++service) { 
			if (excludeList.find((*service).m_strServiceName) == excludeList.end()) {
				tmpObject.data = (*service).m_strServiceName;
				std::wstring x = lookups_[(*service).m_dwStartType];
				if (x != _T("ignored")) {
					tmpObject.crit.state = x;
					list.push_back(tmpObject); 
				}
			}
		} 
		tmpObject.crit.state = _T("ignored");
	}
	for (std::list<StateContainer>::iterator it = list.begin(); it != list.end(); ++it) {
		TNtServiceInfo info;
		if (bNSClient) {
			try {
				info = TNtServiceInfo::GetService((*it).data.c_str());
			} catch (NTServiceException e) {
				if (!msg.empty()) msg += _T(" - ");
				msg += (*it).data + _T(": Error");
				nscapi::plugin_helper::escalteReturnCodeToWARN(returnCode);
				continue;
			}
			if ((*it).crit.state.hasBounds()) {
				bool ok = (*it).crit.state.check(get_state(info.m_dwCurrentState));
				if (!ok || (*it).showAll()) {
					if (info.m_dwCurrentState == SERVICE_RUNNING) {
						if (!msg.empty()) msg += _T(" - ");
						msg += (*it).data + _T(": Started");
					} else if (info.m_dwCurrentState == SERVICE_STOPPED) {
						if (!msg.empty()) msg += _T(" - ");
						msg += (*it).data + _T(": Stopped");
					} else if (info.m_dwCurrentState == MY_SERVICE_NOT_FOUND) {
						if (!msg.empty()) msg += _T(" - ");
						msg += (*it).data + _T(": Not found");
					} else if (info.m_dwCurrentState == SERVICE_START_PENDING) {
						if (!msg.empty()) msg += _T(" - ");
						msg += (*it).data + _T(": Start pending");
					} else if (info.m_dwCurrentState == SERVICE_STOP_PENDING) {
						if (!msg.empty()) msg += _T(" - ");
						msg += (*it).data + _T(": Stop pending");
					} else {
						if (!msg.empty()) msg += _T(" - ");
						msg += (*it).data + _T(": Unknown");
					}
					if (!ok) 
						nscapi::plugin_helper::escalteReturnCodeToCRIT(returnCode);
				}
			}
		} else {
			try {
				info = TNtServiceInfo::GetService((*it).data.c_str());
			} catch (NTServiceException e) {
				NSC_LOG_ERROR_STD(e.getError());
				msg = e.getError();
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
				NSC_LOG_MESSAGE(_T("Service had no (valid) state: ") + (*it).data + _T(" (") + strEx::itos(info.m_dwCurrentState) + _T(")"));
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
		msg = msg.substr(0, truncate-4) + _T("...");
	if (msg.empty() && returnCode == NSCAPI::returnOK)
		msg = _T("OK: All services are in their appropriate state.");
	else if (msg.empty())
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": Whooha this is odd.");
	else if (!bNSClient)
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
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
NSCAPI::nagiosReturn CheckSystem::check_memory(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericPercentageBounds<checkHolders::PercentageValueType<unsigned __int64, unsigned __int64>, checkHolders::disk_size_handler<unsigned __int64> > > > MemoryContainer;
	if (arguments.empty()) {
		msg = _T("ERROR: Missing argument exception.");
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
		MAP_OPTIONS_STR_AND(_T("type"), tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_STR_AND(_T("Type"), tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
		MAP_OPTIONS_SECONDARY_STR_AND(p2,_T("type"), tmpObject.data, tmpObject.alias, list.push_back(tmpObject))
			MAP_OPTIONS_MISSING_EX(p2, msg, _T("Unknown argument: "))
			MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_DISK_ALL(tmpObject, _T(""), _T("Free"), _T("Used"))
		MAP_OPTIONS_STR(_T("Alias"), tmpObject.data)
		MAP_OPTIONS_STR(_T("perf-unit"), tmpObject.perf_unit)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_MISSING(msg, _T("Unknown argument: "))
		MAP_OPTIONS_END()

	if (bNSClient) {
		tmpObject.data = _T("paged");
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
		if (firstPaged && (check.data == _T("paged"))) {
			firstPaged = false;
			dataPaged.value = pdh_collector.getMemCommit();
			if (dataPaged.value == -1) {
				msg = _T("ERROR: Failed to get PDH value.");
				return NSCAPI::returnUNKNOWN;
			}
			dataPaged.total = pdh_collector.getMemCommitLimit();
			if (dataPaged.total == -1) {
				msg = _T("ERROR: Failed to get PDH value.");
				return NSCAPI::returnUNKNOWN;
			}
		} else if (firstMem) {
			try {
				data = memoryChecker.getMemoryStatus();
			} catch (CheckMemoryException e) {
				msg = e.getError();
				return NSCAPI::returnCRIT;
			}
		}

		if (check.data == _T("page")) {
			value.value = data.pageFile.total-data.pageFile.avail; // mem.dwTotalPageFile-mem.dwAvailPageFile;
			value.total = data.pageFile.total; //mem.dwTotalPageFile;
			if (check.alias.empty())
				check.alias = _T("page file");
		} else if (check.data == _T("physical")) {
			value.value = data.phys.total-data.phys.avail; //mem.dwTotalPhys-mem.dwAvailPhys;
			value.total = data.phys.total; //mem.dwTotalPhys;
			if (check.alias.empty())
				check.alias = _T("physical memory");
		} else if (check.data == _T("virtual")) {
			value.value = data.virtualMem.total-data.virtualMem.avail;//mem.dwTotalVirtual-mem.dwAvailVirtual;
			value.total = data.virtualMem.total;//mem.dwTotalVirtual;
			if (check.alias.empty())
				check.alias = _T("virtual memory");
		} else  if (check.data == _T("paged")) {
			value.value = dataPaged.value;
			value.total = dataPaged.total;
			if (check.alias.empty())
				check.alias = _T("paged bytes");
		} else {
			msg = check.data + _T(" is not a known check...");
			return NSCAPI::returnCRIT;
		}
		if (bNSClient) {
			msg = strEx::itos(value.total) + _T("&") + strEx::itos(value.value);
			return NSCAPI::returnOK;
		} else {
			check.perfData = bPerfData;
			check.runCheck(value, returnCode, msg, perf);
		}
	}

	if (msg.empty())
		msg = _T("OK memory within bounds.");
	else
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
	return returnCode;
}
typedef struct NSPROCDATA__ {
	unsigned int count;
	unsigned int hung_count;
	CEnumProcess::CProcessEntry entry;
	std::wstring key;

	NSPROCDATA__() : count(0), hung_count(0) {}
	NSPROCDATA__(const NSPROCDATA__ &other) : count(other.count), hung_count(other.hung_count), entry(other.entry), key(other.key) {}
} NSPROCDATA;
typedef std::map<std::wstring,NSPROCDATA> NSPROCLST;

class NSC_error : public CEnumProcess::error_reporter {
	void report_error(std::wstring error) {
		NSC_LOG_ERROR(error);
	}
	void report_warning(std::wstring error) {
		NSC_LOG_MESSAGE(error);
	}
	void report_debug(std::wstring error) {
		NSC_DEBUG_MSG_STD(_T("PROC::: ") + error);
	}
	void report_debug_enter(std::wstring error) {
		NSC_DEBUG_MSG_STD(_T("PROC>>> ") + error);
	}
	void report_debug_exit(std::wstring error) {
		NSC_DEBUG_MSG_STD(_T("PROC<<<") + error);
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
		NSC_LOG_ERROR_STD(_T("Failed to enumerat processes"));
		NSC_LOG_ERROR_STD(_T("PSAPI method not availabletry installing \"Platform SDK Redistributable: PSAPI for Windows NT\" from Microsoft."));
		NSC_LOG_ERROR_STD(_T("Try this URL: http://www.microsoft.com/downloads/details.aspx?FamilyID=3d1fbaed-d122-45cf-9d46-1cae384097ac"));
		throw CEnumProcess::process_enumeration_exception(_T("PSAPI not avalible, please see eror log for details."));
	}
	NSC_error err;
	CEnumProcess::process_list list = enumeration.enumerate_processes(getCmdLines, use16Bit, &err);
	for (CEnumProcess::process_list::const_iterator entry = list.begin(); entry != list.end(); ++entry) {
		std::wstring key;
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

	std::wstring format_value(std::wstring tag, unsigned long count) {
		if (count > 1)
			return tag + _T("(") + strEx::itos(count) +_T(")");
		if (count == 1)
			return tag;
		return _T("");
	}
	std::wstring to_wstring() {
		if (running > 0 && hung > 0)
			return format_value(_T("running"), running) + _T(", ") + format_value(_T("hung"), hung);
		if (running > 0)
			return format_value(_T("running"), running);
		if (hung > 0)
			return format_value(_T("hung"), hung);
		return _T("stopped");
	}
	std::wstring to_wstring_short() {
		if (running > 0 && hung > 0)
			return _T("running, hung");
		if (running > 0)
			return _T("running");
		if (hung > 0)
			return _T("hung");
		return _T("stopped");
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
	static std::wstring toStringLong(TValueType &value) {
		return value.to_wstring();
	}
	static std::wstring toStringShort(TValueType &value) {
		return value.to_wstring_short();
	}
	std::wstring gatherPerfData(std::wstring alias, std::wstring unit, TValueType &value, TMyType &warn, TMyType &crit) {
		if (hung.hasBounds())
			return hung.gatherPerfData(alias, unit, value.hung, warn.hung, crit.hung);
		return running.gatherPerfData(alias, unit, value.running, warn.running, crit.running);
	}
	std::wstring gatherPerfData(std::wstring alias, std::wstring unit, TValueType &value) {
		THolder tmp;
		if (hung.hasBounds())
			return tmp.gatherPerfData(alias, unit, value.hung);
		return tmp.gatherPerfData(alias, unit, value.running);
	}
	bool check(TValueType &value, std::wstring lable, std::wstring &message, checkHolders::ResultType type) {
		if (hung.hasBounds()) {
			return hung.check_preformatted(value.hung, value.to_wstring(), lable, message, type);
		} else {
			return running.check_preformatted(value.running, value.to_wstring(), lable, message, type);
		}
		return false;
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
NSCAPI::nagiosReturn CheckSystem::check_process(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<ProcessBound> StateContainer;
	
//	typedef checkHolders::CheckContainer<checkHolders::ExactBoundsState> StateContainer2;

	if (arguments.empty()) {
		msg = _T("ERROR: Missing argument exception.");
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
		MAP_OPTIONS_EXACT_NUMERIC_ALL_EX(tmpObject, _T("Count"), running)
		MAP_OPTIONS_EXACT_NUMERIC_LEGACY_EX(tmpObject, _T("Count"), running)
		MAP_OPTIONS_EXACT_NUMERIC_ALL_EX(tmpObject, _T("HungCount"), hung)
		MAP_OPTIONS_EXACT_NUMERIC_LEGACY_EX(tmpObject, _T("HungCount"), hung)
		MAP_OPTIONS_STR(_T("Alias"), tmpObject.alias)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE(_T("ignore-state"), ignoreState)
		MAP_OPTIONS_BOOL_TRUE(_T("cmdLine"), useCmdLine)
		MAP_OPTIONS_BOOL_TRUE(_T("16bit"), use16bit)
		MAP_OPTIONS_MODE(_T("match"), _T("string"), match,  match_string)
		MAP_OPTIONS_MODE(_T("match"), _T("regexp"), match,  match_regexp)
		MAP_OPTIONS_MODE(_T("match"), _T("substr"), match,  match_substring)
		MAP_OPTIONS_MODE(_T("match"), _T("substring"), match,  match_substring)
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
	else if (p2.first == _T("Proc")) {
			tmpObject.data = p__.second;
			tmpObject.alias = p2.second;
			
			list.push_back(tmpObject);
		}
	MAP_OPTIONS_MISSING_EX(p2, msg, _T("Unknown argument: "))
		MAP_OPTIONS_SECONDARY_END()
		else { 
			tmpObject.data = p__.first;
			if (p__.second.empty()) {
				if (!tmpObject.crit.running.hasBounds() && !tmpObject.warn.running.hasBounds())
					tmpObject.crit.running.min = _T("1"); 
			} else if (p__.second == _T("started")) {
				if (!tmpObject.crit.running.hasBounds() && !tmpObject.warn.running.hasBounds())
					tmpObject.crit.running.min = _T("1");
			} else if (p__.second == _T("stopped")) {
				if (!tmpObject.crit.running.hasBounds() && !tmpObject.warn.running.hasBounds())
					tmpObject.crit.running.max = _T("0");
			} else if (p__.second == _T("hung")) {
				if (!tmpObject.crit.hung.hasBounds() && !tmpObject.warn.hung.hasBounds())
					tmpObject.crit.hung.max = _T("1");
			}
			list.push_back(tmpObject);
			tmpObject.reset();
		}
	MAP_OPTIONS_END()

	NSPROCLST runningProcs;
	try {
		runningProcs = GetProcessList(useCmdLine, use16bit);
	} catch (CEnumProcess::process_enumeration_exception &e) {
		NSC_LOG_ERROR_STD(_T("ERROR: ") + e.what());
		msg = static_cast<std::wstring>(_T("ERROR: ")) + e.what();
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unhandled error when processing command"));
		msg = _T("Unhandled error when processing command");
		return NSCAPI::returnUNKNOWN;
	}

	for (std::list<StateContainer>::iterator it = list.begin(); it != list.end(); ++it) {
		NSPROCLST::iterator proc;
		std::wstring key = boost::to_lower_copy((*it).data);
		if (match == match_string) {
			proc = runningProcs.find(key);
		} else if (match == match_substring) {
			for (proc=runningProcs.begin();proc!=runningProcs.end();++proc) {
				if ((*proc).first.find(key) != std::wstring::npos)
					break;
			}
		} else if (match == match_regexp) {
			try {
				boost::wregex filter(key,boost::regex::icase);
				for (proc=runningProcs.begin();proc!=runningProcs.end();++proc) {
					std::wstring value = (*proc).first;
					if (boost::regex_match(value, filter))
						break;
				}
			} catch (const boost::bad_expression e) {
				NSC_LOG_ERROR_STD(_T("Failed to compile regular expression: ") + (*proc).first);
				msg = _T("Failed to compile regular expression: ") + (*proc).first;
				return NSCAPI::returnUNKNOWN;
			} catch (...) {
				NSC_LOG_ERROR_STD(_T("Failed to compile regular expression: ") + (*proc).first);
				msg = _T("Failed to compile regular expression: ") + (*proc).first;
				return NSCAPI::returnUNKNOWN;
			}
		} else {
			NSC_LOG_ERROR_STD(_T("Unsupported mode for: ") + (*proc).first);
			msg = _T("Unsupported mode for: ") + (*proc).first;
			return NSCAPI::returnUNKNOWN;
		}
		bool bFound = (proc != runningProcs.end());
		if (bNSClient) {
			if (bFound && (*it).showAll()) {
				if (!msg.empty()) msg += _T(" - ");
				msg += (*proc).first + _T(": Running");
			} else if (bFound) {
			} else {
				if (!msg.empty()) msg += _T(" - ");
				msg += (*it).data + _T(": not running");
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
		msg = _T("OK: All processes are running.");
	else if (!bNSClient)
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
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
NSCAPI::nagiosReturn CheckSystem::check_pdh(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf)
{
	typedef PerfDataContainer<checkHolders::MaxMinBoundsDouble> CounterContainer;

	if (arguments.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<CounterContainer> counters;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	/* average maax */
	bool bCheckAverages = true; 
	std::wstring invalidStatus = _T("UNKNOWN");
	unsigned int averageDelay = 1000;
	CounterContainer tmpObject;
	bool bExpandIndex = false;
	bool bForceReload = false;
	std::wstring extra_format;

	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_STR(_T("InvalidStatus"), invalidStatus)
		MAP_OPTIONS_STR_AND(_T("Counter"), tmpObject.data, counters.push_back(tmpObject))
		MAP_OPTIONS_STR(_T("MaxWarn"), tmpObject.warn.max_)
		MAP_OPTIONS_STR(_T("MinWarn"), tmpObject.warn.min_)
		MAP_OPTIONS_STR(_T("MaxCrit"), tmpObject.crit.max_)
		MAP_OPTIONS_STR(_T("MinCrit"), tmpObject.crit.min_)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR(_T("Alias"), tmpObject.data)
		MAP_OPTIONS_STR(_T("format"), extra_format)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_EX(_T("Averages"), bCheckAverages, _T("true"), _T("false"))
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE(_T("index"), bExpandIndex)
		MAP_OPTIONS_BOOL_TRUE(_T("reload"), bForceReload)
		MAP_OPTIONS_FIRST_CHAR('\\', tmpObject.data, counters.push_back(tmpObject))
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
	else if (p2.first == _T("Counter")) {
		tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				counters.push_back(tmpObject);
			}
	MAP_OPTIONS_MISSING_EX(p2, msg, _T("Unknown argument: "))
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_FALLBACK_AND(tmpObject.data, counters.push_back(tmpObject))
	MAP_OPTIONS_END()

	if (counters.empty()) {
		msg = _T("No counters specified");
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
					PDH::PDHResolver::expand_index(counter.data);
				}
				if (!PDH::PDHResolver::validate(counter.data, tstr, bForceReload)) {
					NSC_LOG_ERROR_STD(_T("ERROR: Counter not found: ") + counter.data + _T(": ") + tstr);
					if (bNSClient) {
						NSC_LOG_ERROR_STD(_T("ERROR: Counter not found: ") + counter.data + _T(": ") + tstr);
						//msg = _T("0");
					} else {
						msg = _T("CRIT: Counter not found: ") + counter.data + _T(": ") + tstr;
						return NSCAPI::returnCRIT;
					}
				}
				if (!extra_format.empty()) {
					boost::char_separator<wchar_t> sep(_T(","));

					boost::tokenizer< boost::char_separator<wchar_t>, std::wstring::const_iterator, std::wstring > tokens(extra_format, sep);
					DWORD flags = 0;
					BOOST_FOREACH(std::wstring t, tokens) {
						if (extra_format == _T("nocap100"))
							flags |= PDH_FMT_NOCAP100;
						else if (extra_format == _T("1000"))
							flags |= PDH_FMT_1000;
						else if (extra_format == _T("noscale"))
							flags |= PDH_FMT_NOSCALE;
						else {
							NSC_LOG_ERROR_STD(_T("Unsupported extrta format: ") + extra_format);
						}
					}
					counter.get_listener()->set_extra_format(flags);
				}
				pdh.addCounter(counter.data, counter.get_listener());
				has_counter = true;
			}
		} catch (const PDH::PDHException e) {
			NSC_LOG_ERROR_STD(_T("ERROR: ") + e.getError() + _T(" (") + counter.getAlias() + _T("|") + counter.data + _T(")"));
			if (bNSClient)
				msg = _T("0");
			else
				msg = static_cast<std::wstring>(_T("ERROR: ")) + e.getError()+ _T(" (") + counter.getAlias() + _T("|") + counter.data + _T(")");
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
		} catch (const PDH::PDHException e) {
			NSC_LOG_ERROR_STD(_T("ERROR: ") + e.getError());
			if (bNSClient)
				msg = _T("0");
			else
				msg = _T("ERROR: ") + e.getError();
			return NSCAPI::returnUNKNOWN;
		}
	}
	BOOST_FOREACH(CounterContainer &counter, counters) {
		try {
			double value = 0;
			if (counter.data.find('\\') == std::wstring::npos) {
				value = pdh_collector.get_double(counter.data);
				if (value == -1) {
					msg = _T("ERROR: Failed to get counter value: ") + counter.data;
					return NSCAPI::returnUNKNOWN;
				}
			} else {
				value = counter.get_listener()->getValue();
			}

			if (bNSClient) {
				if (!msg.empty())
					msg += _T(",");
				msg += strEx::itos(static_cast<float>(value));
			} else {
				counter.perfData = bPerfData;
				counter.setDefault(tmpObject);
				counter.runCheck(value, returnCode, msg, perf);
			}
		} catch (const PDH::PDHException e) {
			NSC_LOG_ERROR_STD(_T("ERROR: ") + e.getError() + _T(" (") + counter.getAlias() + _T("|") + counter.data + _T(")"));
			if (bNSClient)
				msg = _T("0");
			else
				msg = static_cast<std::wstring>(_T("ERROR: ")) + e.getError()+ _T(" (") + counter.getAlias() + _T("|") + counter.data + _T(")");
			return NSCAPI::returnUNKNOWN;
		}
	}

	if (msg.empty() && !bNSClient)
		msg = _T("OK all counters within bounds.");
	else if (msg.empty()) {
		NSC_LOG_ERROR_STD(_T("No value found returning 0?"));
		msg = _T("0");
	}else if (!bNSClient)
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
	return returnCode;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct regkey_info {

	std::wstring error;

	static regkey_info get(__int64 now, std::wstring path) {
		return regkey_info(now, path);
	}

	regkey_info() 
		: ullLastWriteTime(0)
		, iType(0)
		, ullNow(0)
		, uiExists(0)
		, ullChildCount(0)
	{}
	regkey_info(__int64 now, std::wstring path) 
		: path(path)
		, ullLastWriteTime(0)
		, iType(0)
		, ullNow(now)
		, uiExists(0)
		, ullChildCount(0)
	{
		std::wstring key;
		try {
			std::wcout << _T("opening: ") << path << std::endl;
			std::wstring::size_type pos = path.find_first_of(L'\\');
			if (pos != std::wstring::npos)  {
				key = path.substr(0, pos);
				path = path.substr(pos+1);
				std::wcout << key << _T(":") << path << std::endl;
				simple_registry::registry_key rkey(simple_registry::parseHKEY(key), path);
				info = rkey.get_info();
				uiExists = 1;
			} else {
				error = _T("Failed to parse key");
			}
		} catch (simple_registry::registry_exception &e) {
			try {
				std::wstring::size_type pos = path.find_last_of(L'\\');
				if (pos != std::wstring::npos)  {
					std::wstring item = path.substr(pos+1);
					path = path.substr(0, pos);
					std::wcout << key << _T(":") << path << _T(".") << item << std::endl;
					simple_registry::registry_key rkey(simple_registry::parseHKEY(key), path);
					info = rkey.get_info(item);
					uiExists = 1;
				} else {
					error = _T("Failed to parse key");
				}
			} catch (simple_registry::registry_exception &e) {
				//error = e.what();
			}
		} catch (...) {
			error = _T("Unknown exception");
		}

		//HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\services\eventlog\Application\MaxSize

		// TODO get key info here!
		//ullLastWriteTime = ((info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime);
	};

	unsigned long long ullSize;
	__int64 ullLastWriteTime;
	__int64 ullNow;
	std::wstring filename;
	std::wstring path;
	unsigned long ullChildCount;
	unsigned long uiExists;
	unsigned int iType;
	simple_registry::registry_key::reg_info info;

	static const __int64 MSECS_TO_100NS = 10000;

	__int64 get_written() {
		return (ullNow-ullLastWriteTime)/MSECS_TO_100NS;
	}
	std::wstring render(std::wstring syntax) {
		strEx::replace(syntax, _T("%path%"), path);
		strEx::replace(syntax, _T("%key%"), filename);
		strEx::replace(syntax, _T("%write%"), strEx::format_filetime(ullLastWriteTime, DATE_FORMAT));
		strEx::replace(syntax, _T("%write-raw%"), strEx::itos(ullLastWriteTime));
		strEx::replace(syntax, _T("%now-raw%"), strEx::itos(ullNow));
		strEx::replace(syntax, _T("%type%"), format::format_byte_units(iType));
		strEx::replace(syntax, _T("%child-count%"), strEx::itos(ullChildCount));
		strEx::replace(syntax, _T("%exists%"), strEx::itos(uiExists));
		strEx::replace(syntax, _T("%int%"), strEx::itos(info.iValue));
		strEx::replace(syntax, _T("%int-value%"), strEx::itos(info.iValue));
		strEx::replace(syntax, _T("%string%"), info.sValue);
		strEx::replace(syntax, _T("%string-value%"), info.sValue);
		return syntax;
	}
};


struct regkey_filter {
	filters::filter_all_times written;
	filters::filter_all_num_ul type;
	filters::filter_all_num_ul exists;
	filters::filter_all_num_ul child_count;
	filters::filter_all_num_ll value_int;
	filters::filter_all_strings value_string;

	inline bool hasFilter() {
		return type.hasFilter() || exists.hasFilter() || written.hasFilter() || child_count.hasFilter() || value_int.hasFilter() || value_string.hasFilter();
	}
	bool matchFilter(regkey_info &value) const {
		if ((written.hasFilter())&&(written.matchFilter(value.get_written())))
			return true;
		else if (type.hasFilter()&&type.matchFilter(value.iType))
			return true;
		else if (exists.hasFilter()&&exists.matchFilter(value.uiExists))
			return true;
		else if ((child_count.hasFilter())&&(child_count.matchFilter(value.ullChildCount)))
			return true;
		else if ((value_int.hasFilter())&&(value_int.matchFilter(value.info.iValue)))
			return true;
		else if ((value_string.hasFilter())&&(value_string.matchFilter(value.info.sValue)))
			return true;
		return false;
	}

	std::wstring getValue() const {
		if (written.hasFilter())
			return _T("written: ") + written.getValue();
		if (type.hasFilter())
			return _T("type: ") + type.getValue();
		if (exists.hasFilter())
			return _T("exists: ") + exists.getValue();
		if (child_count.hasFilter())
			return _T("child_count: ") + child_count.getValue();
		if (value_int.hasFilter())
			return _T("value(i): ") + value_int.getValue();
		if (value_string.hasFilter())
			return _T("value(s): ") + value_string.getValue();
		return _T("UNknown...");
	}

};


struct regkey_container : public regkey_info {

	static regkey_container get(std::wstring path, unsigned long long now) {
		return regkey_container(now, path);
	}


	regkey_container(__int64 now, std::wstring path) : regkey_info(now, path) {}

	bool has_errors() {
		return !error.empty();
	}
	std::wstring get_error() {
		return error;
	}

};


class regkey_type_handler {
public:
	static int parse(std::wstring s) {
		return 1;
	}
	static std::wstring print(int value) {
		return _T("unknown");
	}
	static std::wstring print_unformated(int value) {
		return strEx::itos(value);
	}
	static std::wstring key_prefix() {
		return _T("");
	}
	static std::wstring key_postfix() {
		return _T("");
	}
	static std::wstring get_perf_unit(int  value) {
		return _T("");
	}
	static std::wstring print_perf(int  value, std::wstring unit) {
		return strEx::itos(value);
	}
};
class regkey_exists_handler {
public:
	static int parse(std::wstring s) {
		if (s  == _T("true"))
			return 1;
		return 0;
	}
	static std::wstring print(int value) {
		return value==1?_T("true"):_T("false");
	}
	static std::wstring print_unformated(int value) {
		return strEx::itos(value);
	}
	static std::wstring key_prefix() {
		return _T("");
	}
	static std::wstring key_postfix() {
		return _T("");
	}
	static std::wstring get_perf_unit(int value) {
		return _T("");
	}
	static std::wstring print_perf(int  value, std::wstring unit) {
		return strEx::itos(value);
	}
};

typedef checkHolders::CheckContainer<checkHolders::ExactBounds<checkHolders::NumericBounds<int, regkey_type_handler> > > RegTypeContainer;
typedef checkHolders::CheckContainer<checkHolders::ExactBounds<checkHolders::NumericBounds<int, regkey_exists_handler> > > RegExistsContainer;

typedef checkHolders::CheckContainer<checkHolders::ExactBoundsULong> ExactULongContainer;
typedef checkHolders::CheckContainer<checkHolders::ExactBoundsLongLong> ExactLongLongContainer;
typedef checkHolders::CheckContainer<checkHolders::ExactBoundsTime> DateTimeContainer;
typedef checkHolders::CheckContainer<FilterBounds<filters::filter_all_strings> > StringContainer;

struct check_regkey_child_count : public checkHolders::check_proxy_container<regkey_container, ExactULongContainer> {
	check_regkey_child_count() { set_alias(_T("child-count")); }
	unsigned long get_value(regkey_container &value) {
		return value.ullChildCount;
	}
};
struct check_regkey_int_value : public checkHolders::check_proxy_container<regkey_container, ExactLongLongContainer> {
	check_regkey_int_value() { set_alias(_T("value")); }
	long long get_value(regkey_container &value) {
		return value.info.iValue;
	}
};
struct check_regkey_string_value : public checkHolders::check_proxy_container<regkey_container, StringContainer> {
	check_regkey_string_value() { set_alias(_T("value")); }
	std::wstring get_value(regkey_container &value) {
		return value.info.sValue;
	}
};
struct check_regkey_written : public checkHolders::check_proxy_container<regkey_container, DateTimeContainer> {
	check_regkey_written() { set_alias(_T("written")); }
	unsigned long long get_value(regkey_container &value) {
		return value.ullLastWriteTime;
	}
};
struct check_regkey_type : public checkHolders::check_proxy_container<regkey_container, RegTypeContainer> {
	check_regkey_type() { set_alias(_T("type")); }
	int get_value(regkey_container &value) {
		return value.iType;
	}
};
struct check_regkey_exists : public checkHolders::check_proxy_container<regkey_container, RegExistsContainer> {
	check_regkey_exists() { set_alias(_T("exists")); }
	int get_value(regkey_container &value) {
		return value.uiExists;
	}
};


typedef checkHolders::check_multi_container<regkey_container> check_file_multi;
struct check_regkey_factories {
	static checkHolders::check_proxy_interface<regkey_container>* type() {
		return new check_regkey_type();
	}
	static checkHolders::check_proxy_interface<regkey_container>* exists() {
		return new check_regkey_exists();
	}
	static checkHolders::check_proxy_interface<regkey_container>* child_count() {
		return new check_regkey_child_count();
	}
	static checkHolders::check_proxy_interface<regkey_container>* written() {
		return new check_regkey_written();
	}
	static checkHolders::check_proxy_interface<regkey_container>* value_string() {
		return new check_regkey_string_value();
	}
	static checkHolders::check_proxy_interface<regkey_container>* value_int() {
		return new check_regkey_int_value();
	}
};

#define MAP_FACTORY_PB(value, obj) \
		else if ((p__.first == _T("check")) && (p__.second == ##value)) { checker.add_check(check_regkey_factories::obj()); }


NSCAPI::nagiosReturn CheckSystem::check_registry(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	check_file_multi checker;
	typedef std::pair<int,regkey_filter> filteritem_type;
	typedef std::list<filteritem_type > filterlist_type;
	if (arguments.empty()) {
		msg = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<std::wstring> files;
	unsigned int truncate = 0;
	std::wstring syntax = _T("%filename%");
	std::wstring alias;
	bool bPerfData = true;

	try {
		MAP_OPTIONS_BEGIN(arguments)
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_STR(_T("syntax"), syntax)
			MAP_OPTIONS_STR(_T("alias"), alias)
			MAP_OPTIONS_PUSH(_T("path"), files)
			MAP_OPTIONS_SHOWALL(checker)
			MAP_OPTIONS_EXACT_NUMERIC_ALL_MULTI(checker, _T(""))
			MAP_FACTORY_PB(_T("type"), type)
			MAP_FACTORY_PB(_T("child-count"), child_count)
			MAP_FACTORY_PB(_T("written"), written)
			MAP_FACTORY_PB(_T("int"), value_int)
			MAP_FACTORY_PB(_T("string"), value_string)
			MAP_OPTIONS_MISSING(msg, _T("Unknown argument: "))
			MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		msg = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (filters::filter_exception e) {
		msg = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	}
	FILETIME now;
	GetSystemTimeAsFileTime(&now);
	unsigned __int64 nowi64 = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
	for (std::list<std::wstring>::const_iterator pit = files.begin(); pit != files.end(); ++pit) {
		regkey_container info = regkey_container::get(*pit, nowi64);
		if (info.has_errors()) {
			msg = info.error;
			return NSCAPI::returnUNKNOWN;
		}
		checker.alias = info.render(syntax);
		checker.runCheck(info, returnCode, msg, perf);
	}
	if ((truncate > 0) && (msg.length() > (truncate-4))) {
		msg = msg.substr(0, truncate-4) + _T("...");
		perf = _T("");
	}
	if (msg.empty())
		msg = _T("CheckSingleRegkey ok");
	return returnCode;
}
