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
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include "stdafx.h"

#include <map>
#include <vector>

#include <boost/optional.hpp>
#include <boost/program_options.hpp>

#include "CheckWMI.h"
#include <strEx.h>
#include <time.h>

#include <arrayBuffer.h>

CheckWMI::CheckWMI() {
}
CheckWMI::~CheckWMI() {
}

namespace sh = nscapi::settings_helper;

void target_helper::add_target(nscapi::settings_helper::settings_impl_interface_ptr core, std::wstring key, std::wstring val) {
	std::wstring alias = key;
	target_info target;
	try {
		sh::settings_registry settings(core);

		target.hostname = val;
		if (val.empty())
			target.hostname = alias;

		settings.add_path_to_settings()
			(target.hostname, _T("TARGET LIST SECTION"), _T("A list of avalible remote target systems"))

			;

		settings.add_key_to_settings(_T("targets/") + target.hostname)
			(_T("hostname"), sh::wstring_key(&target.hostname),
			_T("TARGET HOSTNAME"), _T("Hostname or ip address of target"))

			(_T("username"), sh::wstring_key(&target.username),
			_T("TARGET USERNAME"), _T("Username used to authenticate with"))

			(_T("password"), sh::wstring_key(&target.password),
			_T("TARGET PASSWORD"), _T("Password used to authenticate with"))

			(_T("protocol"), sh::wstring_key(&target.protocol),
			_T("TARGET PROTOCOL"), _T("Protocol identifier used to route requests"))

			;

		settings.register_all();
		settings.notify();

	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}
	targets[alias] = target;
	NSC_LOG_ERROR_STD(_T("Found: ") + alias + _T(" ==> ") + target.to_wstring());
}

bool CheckWMI::loadModule() {
	return false;
}
bool CheckWMI::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		register_command(_T("CheckWMIValue"), _T("Run a WMI query and check the resulting value (the values of each row determin the state)."));
		register_command(_T("CheckWMI"), _T("Run a WMI query and check the resulting rows (the number of hits determine state)."));

		sh::settings_registry settings(get_settings_proxy());
		//settings.set_alias(_T("targets"));

		settings.add_path_to_settings()
			(_T("targets"), sh::fun_values_path(boost::bind(&target_helper::add_target, &targets, get_settings_proxy(), _1, _2)), 
			_T("TARGET LIST SECTION"), _T("A list of avalible remote target systems"))
			;

		settings.register_all();
		settings.notify();

	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
	}
	return true;
}
bool CheckWMI::unloadModule() {
	return true;
}

bool CheckWMI::hasCommandHandler() {
	return true;
}
bool CheckWMI::hasMessageHandler() {
	return false;
}

std::wstring build_namespace(std::wstring ns, std::wstring computer) {
	if (ns.empty())
		ns = _T("root\\cimv2");
	if (!computer.empty())
		ns = _T("\\\\") + computer + _T("\\") + ns;
	return ns;
}
#define MAP_CHAINED_FILTER(value, obj) \
			else if (p__.first.length() > 8 && p__.first.substr(1,6) == _T("filter") && p__.first.substr(7,1) == _T("-") && p__.first.substr(8) == value) { \
			WMIQuery::wmi_filter filter; filter.obj = p__.second; chain.push_filter(p__.first, filter); }

#define MAP_SECONDARY_CHAINED_FILTER(value, obj) \
			else if (p2.first.length() > 8 && p2.first.substr(1,6) == _T("filter") && p2.first.substr(7,1) == _T("-") && p2.first.substr(8) == value) { \
			WMIQuery::wmi_filter filter; filter.obj = p__.second; filter.alias = p2.second; chain.push_filter(p__.first, filter); }

#define MAP_CHAINED_FILTER_STRING(value) \
	MAP_CHAINED_FILTER(value, string)

#define MAP_CHAINED_FILTER_NUMERIC(value) \
	MAP_CHAINED_FILTER(value, numeric)

NSCAPI::nagiosReturn CheckWMI::CheckSimpleWMI(const std::wstring &target, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, checkHolders::int_handler> > > WMIContainer;

	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	typedef filters::chained_filter<WMIQuery::wmi_filter,WMIQuery::wmi_row> filter_chain;
	filter_chain chain;
	if (arguments.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}
	unsigned int truncate = 0;
	std::wstring query, alias;
	std::wstring ns = _T("root\\cimv2");
	bool bPerfData = true;
	std::wstring colSyntax;
	std::wstring colSep;
	target_helper::target_info target_info;
	boost::optional<target_helper::target_info> t = targets.find(target);
	if (t)
		target_info = *t;
	std::wstring given_target;

	WMIContainer result_query;
	try {
		MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_STR(_T("Query"), query)
		MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
		MAP_OPTIONS_STR(_T("namespace"), ns)
		MAP_OPTIONS_STR(_T("Alias"), result_query.alias)
		MAP_OPTIONS_STR(_T("target"), given_target)
		MAP_OPTIONS_STR(_T("user"), target_info.username)
		MAP_OPTIONS_STR(_T("password"), target_info.password)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_NUMERIC_ALL(result_query, _T(""))
		MAP_OPTIONS_SHOWALL(result_query)
		MAP_CHAINED_FILTER(_T("string"),string)
		MAP_OPTIONS_STR(_T("columnSyntax"),colSyntax)
		MAP_OPTIONS_STR(_T("columnSeparator"),colSep)
		MAP_CHAINED_FILTER(_T("numeric"),numeric)
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
		MAP_SECONDARY_CHAINED_FILTER(_T("string"),string)
		MAP_SECONDARY_CHAINED_FILTER(_T("numeric"),numeric)
			else if (p2.first == _T("Query")) {
					query = p__.second;
					result_query.alias = p2.second;
				}
		MAP_OPTIONS_MISSING_EX(p2, message, _T("Unknown argument: "))
			MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = _T("WMIQuery failed: ") + e.getMessage();
		return NSCAPI::returnCRIT;
	}

	if (!given_target.empty()) {
		t = targets.find(given_target);
		if (t)
			target_info.update_from(*t);
		else
			target_info.hostname = given_target;
	}

	WMIQuery::result_type rows;
	try {
		WMIQuery wmiQuery;
		ns = build_namespace(ns, target_info.hostname);
		NSC_DEBUG_MSG_STD(_T("Running query: '") + query + _T("' on: ") + ns + _T(" with ") + target_info.to_wstring());
		rows = wmiQuery.execute(ns, query, target_info.username, target_info.password);
	} catch (WMIException e) {
		message = _T("WMIQuery failed: ") + e.getMessage();
		return NSCAPI::returnCRIT;
	}
	int hit_count = 0;

	if (chain.empty()) {
		NSC_DEBUG_MSG_STD(_T("No filters specified so we will match all rows"));
		hit_count = rows.size();
		for (WMIQuery::result_type::iterator citRow = rows.begin(); citRow != rows.end(); ++citRow) {
			WMIQuery::wmi_row vals = *citRow;
			strEx::append_list(message, vals.render(colSyntax, colSep), colSep);
		}
	} else {
		bool match = chain.get_inital_state();
		for (WMIQuery::result_type::iterator citRow = rows.begin(); citRow != rows.end(); ++citRow) {
			WMIQuery::wmi_row vals = *citRow;
			match = chain.match(match, vals);
			if (match) {
				strEx::append_list(message, vals.render(colSyntax, colSep), colSep);
				hit_count++;
			}
		}
	}

	if (!bPerfData)
		result_query.perfData = false;
	if (result_query.alias.empty())
		result_query.alias = _T("wmi query");

	NSC_DEBUG_MSG_STD(_T("Message is: ") + message);
	result_query.runCheck(hit_count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("OK: WMI Query returned no results.");
	return returnCode;
}

NSCAPI::nagiosReturn CheckWMI::CheckSimpleWMIValue(const std::wstring &target, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericBounds<long long, checkHolders::int64_handler> > > WMIContainer;
	if (arguments.empty()) {
		message = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<WMIContainer> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	WMIContainer tmpObject;
	bool bPerfData = true;
	unsigned int truncate = 0;
	std::wstring query;
	std::wstring ns = _T("root\\cimv2");
	std::wstring aliasCol;
	std::wstring password;
	std::wstring user;

	target_helper::target_info target_info;
	boost::optional<target_helper::target_info> t = targets.find(target);
	if (t)
		target_info = *t;
	std::wstring given_target;

	// Query=Select ... MaxWarn=5 MaxCrit=12 Check=Col1 --(later)-- Match==test Check=Col2
	// MaxWarnNumeric:ID=>5
	try {
		MAP_OPTIONS_BEGIN(arguments)
			MAP_OPTIONS_SHOWALL(tmpObject)
			MAP_OPTIONS_NUMERIC_ALL(tmpObject, _T(""))
			MAP_OPTIONS_STR(_T("namespace"), ns)
			MAP_OPTIONS_STR(_T("Alias"), tmpObject.data)
			MAP_OPTIONS_STR(_T("AliasCol"), aliasCol)
			MAP_OPTIONS_STR(_T("target"), given_target)
			MAP_OPTIONS_STR(_T("user"), target_info.username)
			MAP_OPTIONS_STR(_T("password"), target_info.password)
			MAP_OPTIONS_STR(_T("Query"), query)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_STR_AND(_T("Check"), tmpObject.data, list.push_back(tmpObject))
			MAP_OPTIONS_STR(_T("Alias"), tmpObject.data)
			MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
			MAP_OPTIONS_SECONDARY_STR_AND(p2,_T("Check"), tmpObject.data, tmpObject.alias, list.push_back(tmpObject))
				MAP_OPTIONS_MISSING_EX(p2, message, _T("Unknown argument: "))
				MAP_OPTIONS_SECONDARY_END()
			MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
			MAP_OPTIONS_END()

	} catch (filters::parse_exception e) {
		message = _T("WMIQuery failed: ") + e.getMessage();
		return NSCAPI::returnCRIT;
	}
	if (!given_target.empty()) {
		t = targets.find(given_target);
		if (t)
			target_info.update_from(*t);
		else
			target_info.hostname = given_target;
	}

	WMIQuery::result_type rows;
	try {
		ns = build_namespace(ns, target_info.hostname);
		NSC_DEBUG_MSG_STD(_T("Running query: '") + query + _T("' on: ") + ns + _T(" with ") + target_info.to_wstring());
		WMIQuery wmiQuery;
		rows = wmiQuery.execute(ns, query, target_info.username, target_info.password);
	} catch (WMIException e) {
		message = _T("WMIQuery failed: ") + e.getMessage();
		return NSCAPI::returnCRIT;
	}
	int hit_count = 0;

	for (std::list<WMIContainer>::const_iterator it = list.begin(); it != list.end(); ++it) {
		WMIContainer itm = (*it);
		itm.setDefault(tmpObject);
		itm.perfData = bPerfData;
		if (itm.data == _T("*")) {
			for (WMIQuery::result_type::const_iterator citRow = rows.begin(); citRow != rows.end(); ++citRow) {
				for (WMIQuery::wmi_row::list_type::const_iterator citCol = (*citRow).results.begin(); citCol != (*citRow).results.end(); ++citCol) {
					long long value = (*citCol).second.numeric;
					itm.runCheck(value, returnCode, message, perf);
				}
			}
		}
	}
	for (WMIQuery::result_type::const_iterator citRow = rows.begin(); citRow != rows.end(); ++citRow) {
		bool found = false;
		std::wstring alias;
		if (!aliasCol.empty()) {
			alias = (*citRow).get(aliasCol).string;
		}
		for (WMIQuery::wmi_row::list_type::const_iterator citCol = (*citRow).results.begin(); citCol != (*citRow).results.end(); ++citCol) {
			for (std::list<WMIContainer>::const_iterator it = list.begin(); it != list.end(); ++it) {
				WMIContainer itm = (*it);
				if (itm.data == _T("*")) {
					found = true;
				} else if ((*citCol).first == itm.data) {
					std::wstring oldAlias = itm.alias;
					if (!alias.empty())
						itm.alias = alias + _T(" ") + itm.getAlias();
					found = true;
					long long value = (*citCol).second.numeric;
					itm.runCheck(value, returnCode, message, perf);
					itm.alias = oldAlias;
				}
			}
		}
		if (!found) {
			NSC_LOG_ERROR_STD(_T("At least one of the queried columns was not found!"));
		}
	}

	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("OK: Everything seems fine.");
	return returnCode;
}

struct pad_handler {
	std::vector<std::wstring::size_type> widths;
	int index;
	pad_handler() : index(0) {}
	void reset_index() {
		index = 0;
	}
	void set_next(std::wstring::size_type size) {
		set(index++, size);
	}
	void set(int col, std::wstring::size_type size) {
		if (col >= widths.size())
			widths.resize(col+20, 0);
		widths[col] = max(widths[col], size);
	}
	std::wstring pad_next(std::wstring str, wchar_t c = L' ') {
		return pad(index++, str, c);
	}
	std::wstring pad_current(std::wstring str, wchar_t c = L' ') {
		return pad(index, str, c);
	}
	std::wstring pad(int col, std::wstring str, wchar_t c = L' ') const {
		std::wstring::size_type w = 0;
		if (widths.size() > col)
			w = widths[col];
		if (w > str.size())
			w -= str.size();
		else
			w = 0;
		return std::wstring(1, c) + str + std::wstring(w+1, c);
	}

};
void print_pretty_results(WMIQuery::result_type &rows, int limit, std::wstring & result) 
{
	pad_handler padder;
	NSC_DEBUG_MSG_STD(_T("Query returned: ") + strEx::itos(rows.size()) + _T(" rows."));
	int rownum=0;
	BOOST_FOREACH(const WMIQuery::wmi_row &row, rows) {
		if (rownum++ == 0) {
			padder.reset_index();
			BOOST_FOREACH(const WMIQuery::wmi_row::list_type::value_type &val, row.results) {
				padder.set_next(val.first.length());
			}
		}
		if (limit != -1 && rownum > limit)
			break;
		padder.reset_index();
		BOOST_FOREACH(const WMIQuery::wmi_row::list_type::value_type &val, row.results) {
			padder.set_next(val.second.string.length());
		}
	}
	rownum=0;
	BOOST_FOREACH(const WMIQuery::wmi_row &row, rows) {
		if (rownum++ == 0) {
			std::wstring row1 = _T("|");
			std::wstring row2 = _T("|");
			padder.reset_index();
			BOOST_FOREACH(const WMIQuery::wmi_row::list_type::value_type &val, row.results) {
				row1 += padder.pad_current(val.first) + _T("|");
				row2 += padder.pad_next(_T(""), L'-') + _T("|");
			}
			result += row1 + _T("\n");
			result += row2 + _T("\n");
		}

		if (limit != -1 && rownum > limit)
			break;
		std::wstring row1 = _T("|");
		padder.reset_index();
		BOOST_FOREACH(const WMIQuery::wmi_row::list_type::value_type &val, row.results) {
			row1 += padder.pad_next(val.second.string) + _T("|");
		}
		result += row1 + _T("\n");
	}
}

void print_simple_results(WMIQuery::result_type &rows, int limit, std::wstring & result) 
{
	int rownum=0;
	BOOST_FOREACH(const WMIQuery::wmi_row &row, rows) {
		if (limit != -1 && rownum > limit)
			break;
		bool first = true;
		BOOST_FOREACH(const WMIQuery::wmi_row::list_type::value_type &val, row.results) {
			if (first) 
				first = false;
			else
				result += _T(",");
			result += val.second.string;
		}
		result += _T("\n");
	}
}
void print_results(WMIQuery::result_type &rows, int limit, std::wstring & result, bool simple)  {
	if (simple)
		print_simple_results(rows, limit, result);
	else
		print_pretty_results(rows, limit, result);
}


NSCAPI::nagiosReturn CheckWMI::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("checkwmi")) {
		return CheckSimpleWMI(target, arguments, message, perf);
	} else if (command == _T("checkwmivalue")) {
		return CheckSimpleWMIValue(target, arguments, message, perf);
	}	
	return NSCAPI::returnIgnored;
}

void list_ns_rec(std::wstring ns, std::wstring user, std::wstring password, std::wstring &result) {
	try {
		WMIQuery wmiQuery;
		WMIQuery::result_type rows = wmiQuery.get_instances(ns, _T("__Namespace"), user, password);
		BOOST_FOREACH(WMIQuery::wmi_row &row, rows) {
			const WMIQuery::WMIResult &res = row.results[std::wstring(_T("Name"))];
			//const WMIQuery::wmi_row::list_type::value_type v = row.results[_T("Name")];
			std::wstring name = res.string;
			result += ns + _T("\\") + name + _T("\n");
			list_ns_rec(ns + _T("\\") + name, user, password, result);
		}
	} catch (WMIException e) {
		NSC_LOG_ERROR_STD(_T("WMIQuery failed: ") + e.getMessage());
		result += _T("ERROR: ") + e.getMessage();
	}
}


NSCAPI::nagiosReturn CheckWMI::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	try {
		if (command == _T("wmi") || command == _T("help") || command.empty()) {

			namespace po = boost::program_options;

			std::wstring query, ns, computer, user, password, list_cls, list_inst;
			bool simple;
			int limit = -1;
			po::options_description desc("Allowed options");
			desc.add_options()
				("help,h", "Show help screen")
				("select,s", po::wvalue<std::wstring>(&query), "Execute a query")
				("simple", "Use simple format")
				("list-classes", po::wvalue<std::wstring>(&list_cls)->implicit_value(_T("")), "list all classes of a given type")
				("list-instances", po::wvalue<std::wstring>(&list_inst)->implicit_value(_T("")), "list all instances of a given type")
				("list-ns", "list all name spaces")
				("list-all-ns", "list all name spaces recursivly")
				("limit,l", po::value<int>(&limit), "Limit number of rows")
				("namespace,n", po::wvalue<std::wstring>(&ns)->default_value(_T("root\\cimv2")), "Namespace")
				("computer,c", po::wvalue<std::wstring>(&computer), "A remote computer to connect to ")
				("user,u", po::wvalue<std::wstring>(&user), "The user for the remote computer")
				("password,p", po::wvalue<std::wstring>(&password), "The password for the remote computer")
				;

			boost::program_options::variables_map vm;

			if (command == _T("help")) {
				std::stringstream ss;
				ss << "wmi Command line syntax:" << std::endl;
				ss << desc;
				result = utf8::cvt<std::wstring>(ss.str());
				return NSCAPI::isSuccess;
			}

			std::vector<std::wstring> args(arguments.begin(), arguments.end());
			po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(args).options(desc).run();
			po::store(parsed, vm);
			po::notify(vm);

			if (vm.count("help") || (vm.count("select") == 0 && vm.count("list-classes") == 0 && vm.count("list-instances") == 0 && vm.count("list-ns") == 0 && vm.count("list-all-ns") == 0)) {
				std::stringstream ss;
				ss << "CheckWMI Command line syntax:" << std::endl;
				ss << desc;
				result = utf8::cvt<std::wstring>(ss.str());
				return NSCAPI::isSuccess;
			}
			simple = vm.count("simple") > 0;

			ns = build_namespace(ns, computer);

			WMIQuery::result_type rows;
			if (vm.count("select")) {
				try {
					WMIQuery wmiQuery;
					NSC_DEBUG_MSG_STD(_T("Running query: '") + query + _T("' on: ") + ns);
					rows = wmiQuery.execute(ns, query, user, password);
				} catch (WMIException e) {
					NSC_LOG_ERROR_STD(_T("WMIQuery failed: ") + e.getMessage());
					result += _T("ERROR: ") + e.getMessage();
					return NSCAPI::hasFailed;
				}
				if (rows.empty()) {
					result += _T("No result");
					return NSCAPI::isSuccess;
				} else {
					print_results(rows, limit, result, simple);
				}
			} else if (vm.count("list-classes")) {
				try {
					WMIQuery wmiQuery;
					rows = wmiQuery.get_classes(ns, list_cls, user, password);
					print_results(rows, limit, result, simple);
				} catch (WMIException e) {
					NSC_LOG_ERROR_STD(_T("WMIQuery failed: ") + e.getMessage());
					result += _T("ERROR: ") + e.getMessage();
					return NSCAPI::hasFailed;
				}
			} else if (vm.count("list-instances")) {
				try {
					WMIQuery wmiQuery;
					rows = wmiQuery.get_instances(ns, list_inst, user, password);
					print_results(rows, limit, result, simple);
				} catch (WMIException e) {
					NSC_LOG_ERROR_STD(_T("WMIQuery failed: ") + e.getMessage());
					result += _T("ERROR: ") + e.getMessage();
					return NSCAPI::hasFailed;
				}
			} else if (vm.count("list-ns")) {
				try {
					WMIQuery wmiQuery;
					rows = wmiQuery.get_instances(ns, _T("__Namespace"), user, password);
					print_results(rows, limit, result, simple);
				} catch (WMIException e) {
					NSC_LOG_ERROR_STD(_T("WMIQuery failed: ") + e.getMessage());
					result += _T("ERROR: ") + e.getMessage();
					return NSCAPI::hasFailed;
				}
			} else if (vm.count("list-all-ns")) {
				try {
					list_ns_rec(ns, user, password, result);
				} catch (WMIException e) {
					NSC_LOG_ERROR_STD(_T("WMIQuery failed: ") + e.getMessage());
					result += _T("ERROR: ") + e.getMessage();
					return NSCAPI::hasFailed;
				}
			}
		}
		return NSCAPI::isSuccess;
	} catch (const std::exception &e) {
		result += _T("ERROR: Failed to parse command line: ") + utf8::cvt<std::wstring>(e.what());
		NSC_LOG_ERROR_STD(_T("Failed to parse command line: ") + utf8::cvt<std::wstring>(e.what()));
		return NSCAPI::hasFailed;
	}
	return NSCAPI::returnIgnored;
}


NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(CheckWMI, _T("wmi"));
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
