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

#include <boost/program_options.hpp>

#include "CheckWMI.h"
#include <strEx.h>
#include <time.h>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

void target_helper::add_target(nscapi::settings_helper::settings_impl_interface_ptr core, std::string key, std::string val) {
	std::string alias = key;
	target_info target;
	try {
		sh::settings_registry settings(core);

		target.hostname = val;
		if (val.empty())
			target.hostname = alias;

		settings.add_path_to_settings()
			(target.hostname, "TARGET LIST SECTION", "A list of available remote target systems")

			;

		settings.add_key_to_settings("targets/" + target.hostname)
			("hostname", sh::string_key(&target.hostname),
			"TARGET HOSTNAME", "Hostname or ip address of target")

			("username", sh::string_key(&target.username),
			"TARGET USERNAME", "Username used to authenticate with")

			("password", sh::string_key(&target.password),
			"TARGET PASSWORD", "Password used to authenticate with")

			("protocol", sh::string_key(&target.protocol),
			"TARGET PROTOCOL", "Protocol identifier used to route requests")

			;

		settings.register_all();
		settings.notify();

	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("loading: ", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("loading: ");
	}
	targets[alias] = target;
}

bool CheckWMI::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	sh::settings_registry settings(get_settings_proxy());
	//settings.set_alias(_T("targets"));

	settings.add_path_to_settings()
		("targets", sh::fun_values_path(boost::bind(&target_helper::add_target, &targets, get_settings_proxy(), _1, _2)), 
		"TARGET LIST SECTION", "A list of avalible remote target systems")
		;

	settings.register_all();
	settings.notify();
	return true;
}
bool CheckWMI::unloadModule() {
	return true;
}

std::wstring build_namespace(std::wstring ns, std::string computer) {
	if (ns.empty())
		ns = _T("root\\cimv2");
	if (!computer.empty())
		ns = _T("\\\\") + utf8::cvt<std::wstring>(computer) + _T("\\") + ns;
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

void CheckWMI::check_wmi(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, checkHolders::int_handler> > > WMIContainer;

	typedef filters::chained_filter<WMIQuery::wmi_filter,WMIQuery::wmi_row> filter_chain;
	filter_chain chain;
	unsigned int truncate = 0;
	std::wstring query, alias;
	std::wstring ns = _T("root\\cimv2");
	bool bPerfData = true;
	std::string colSyntax;
	std::string colSep;
	target_helper::target_info target_info;
	boost::optional<target_helper::target_info> t;
	/*
	boost::optional<target_helper::target_info> t = targets.find(target);
	if (t)
		target_info = *t;
		*/
	std::string given_target;
	WMIContainer result_query;


	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
		("truncate", po::value<unsigned int>(&truncate), "Truncate the resulting message (mainly useful in older version of nsclient++)")
		("alias", po::value<std::string>(&result_query.alias),			"Alias: TODO.")
		("columnSyntax", po::value<std::string>(&colSyntax), "Syntax for columns.")
		("columnSeparator", po::value<std::string>(&colSep), "TODO: What is this.")
		("target", po::value<std::string>(&given_target), "The target to check (for checking remote machines).")
		("user", po::value<std::string>(&target_info.username), "Remote username when checking remote machines.")
		("password", po::value<std::string>(&target_info.password), "Remote password when checking remote machines.")
		("namespace", po::wvalue<std::wstring>(&ns), "The WMI root namespace to bind to.")
		("query", po::wvalue<std::wstring>(&query), "The WMI query to execute.")
		;

	nscapi::program_options::legacy::add_numerical_all(desc);
	//nscapi::program_options::legacy::add_exact_numerical_all(desc);
	nscapi::program_options::legacy::add_ignore_perf_data(desc, bPerfData);
	nscapi::program_options::legacy::add_show_all(desc);

	boost::program_options::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
		return;
	nscapi::program_options::legacy::collect_numerical_all(vm, result_query);
	//nscapi::program_options::legacy::collect_exact_numerical_all(vm, query2);
	nscapi::program_options::legacy::collect_show_all(vm, result_query);
	//nscapi::program_options::legacy::collect_show_all(vm, query2);

	/*
		MAP_CHAINED_FILTER(_T("string"),string)
		MAP_CHAINED_FILTER(_T("numeric"),numeric)
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
		MAP_SECONDARY_CHAINED_FILTER(_T("string"),string)
		MAP_SECONDARY_CHAINED_FILTER(_T("numeric"),numeric)
			else if (p2.first == _T("Query")) {
					query = p__.second;
					result_query.alias = p2.second;
				}
				*/

	if (!given_target.empty()) {
		t = targets.find(given_target);
		if (t)
			target_info.update_from(*t);
		else
			target_info.hostname = given_target;
	}
	if (query.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No query specified.");

	WMIQuery::result_type rows;
	try {
		WMIQuery wmiQuery;
		ns = build_namespace(ns, target_info.hostname);
		rows = wmiQuery.execute(ns, query, utf8::cvt<std::wstring>(target_info.username), utf8::cvt<std::wstring>(target_info.password));
	} catch (WMIException e) {
		return nscapi::protobuf::functions::set_response_bad(*response, "WMIQuery failed: " + e.reason());
	}
	std::size_t hit_count = 0;

	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::string message;
	if (chain.empty()) {
		NSC_DEBUG_MSG_STD("No filters specified so we will match all rows");
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
		result_query.alias = "wmi query";

	std::string perf;
	result_query.runCheck(hit_count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + "...";
	if (message.empty())
		message = "OK: WMI Query returned no results.";
	response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(returnCode));
	response->set_message(message);
}

void CheckWMI::check_wmi_value(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericBounds<long long, checkHolders::int64_handler> > > WMIContainer;

	std::list<WMIContainer> list;
	WMIContainer tmpObject;
	bool bPerfData = true;
	unsigned int truncate = 0;
	std::wstring query;
	std::wstring ns = _T("root\\cimv2");
	std::wstring aliasCol;
	std::wstring password;
	std::wstring user;

	target_helper::target_info target_info;
	boost::optional<target_helper::target_info> t;
	/*
	boost::optional<target_helper::target_info> t = targets.find(target);
	if (t)
		target_info = *t;
	*/
	std::string given_target;


	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
		("truncate", po::value<unsigned int>(&truncate), "Truncate the resulting message (mainly useful in older version of nsclient++)")
//		("alias", po::wvalue<std::wstring>(&result_query.alias),			"Alias: TODO.")
//		("columnSyntax", po::wvalue<std::wstring>(&colSyntax), "Syntax for columns.")
//		("columnSeparator", po::wvalue<std::wstring>(&colSep), "TODO: What is this.")
		("target", po::value<std::string>(&given_target), "The target to check (for checking remote machines).")
		("user", po::value<std::string>(&target_info.username), "Remote username when checking remote machines.")
		("password", po::value<std::string>(&target_info.password), "Remote password when checking remote machines.")
		("namespace", po::wvalue<std::wstring>(&ns), "The WMI root namespace to bind to.")
		("query", po::wvalue<std::wstring>(&query), "The WMI query to execute.")
		;

	nscapi::program_options::legacy::add_numerical_all(desc);
	//nscapi::program_options::legacy::add_exact_numerical_all(desc);
	nscapi::program_options::legacy::add_ignore_perf_data(desc, bPerfData);
	nscapi::program_options::legacy::add_show_all(desc);

	boost::program_options::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
		return;
	//nscapi::program_options::legacy::collect_numerical_all(vm, result_query);
	//nscapi::program_options::legacy::collect_exact_numerical_all(vm, query2);
	//nscapi::program_options::legacy::collect_show_all(vm, result_query);
	//nscapi::program_options::legacy::collect_show_all(vm, query2);
	// Query=Select ... MaxWarn=5 MaxCrit=12 Check=Col1 --(later)-- Match==test Check=Col2
	// MaxWarnNumeric:ID=>5
	/*
	MAP_OPTIONS_STR(_T("AliasCol"), aliasCol)
	MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
	MAP_OPTIONS_STR_AND(_T("Check"), tmpObject.data, list.push_back(tmpObject))
		
	MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
	MAP_OPTIONS_SECONDARY_STR_AND(p2,_T("Check"), tmpObject.data, tmpObject.alias, list.push_back(tmpObject))
	MAP_OPTIONS_MISSING_EX(p2, message, _T("Unknown argument: "))
	MAP_OPTIONS_SECONDARY_END()
	MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
	*/
	if (!given_target.empty()) {
		t = targets.find(given_target);
		if (t)
			target_info.update_from(*t);
		else
			target_info.hostname = given_target;
	}
	if (query.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No query specified.");

	WMIQuery::result_type rows;
	try {
		ns = build_namespace(ns, target_info.hostname);
		WMIQuery wmiQuery;
		rows = wmiQuery.execute(ns, query, utf8::cvt<std::wstring>(target_info.username), utf8::cvt<std::wstring>(target_info.password));
	} catch (WMIException e) {
		return nscapi::protobuf::functions::set_response_bad(*response, "WMIQuery failed: " + e.reason());
	}

	std::string message, perf;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	for (std::list<WMIContainer>::const_iterator it = list.begin(); it != list.end(); ++it) {
		WMIContainer itm = (*it);
		itm.setDefault(tmpObject);
		itm.perfData = bPerfData;
		if (itm.data == "*") {
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
		std::string alias;
		if (!aliasCol.empty()) {
			alias = utf8::cvt<std::string>((*citRow).get(aliasCol).string);
		}
		for (WMIQuery::wmi_row::list_type::const_iterator citCol = (*citRow).results.begin(); citCol != (*citRow).results.end(); ++citCol) {
			for (std::list<WMIContainer>::const_iterator it = list.begin(); it != list.end(); ++it) {
				WMIContainer itm = (*it);
				if (itm.data == "*") {
					found = true;
				} else if (utf8::cvt<std::string>((*citCol).first) == itm.data) {
					std::string oldAlias = itm.alias;
					if (!alias.empty())
						itm.alias = alias + " " + itm.getAlias();
					found = true;
					long long value = (*citCol).second.numeric;
					itm.runCheck(value, returnCode, message, perf);
					itm.alias = oldAlias;
				}
			}
		}
		if (!found) {
			NSC_LOG_ERROR_STD("At least one of the queried columns was not found!");
		}
	}

	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + "...";
	if (message.empty())
		message = "OK: Everything seems fine.";
	response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(returnCode));
	response->set_message(message);
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
	//NSC_DEBUG_MSG_STD("Query returned: " + strEx::s:::xtos(rows.size()) + " rows.");
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
	} catch (const WMIException &e) {
		NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
		result += _T("ERROR: ") + utf8::cvt<std::wstring>(e.reason());
	}
}


NSCAPI::nagiosReturn CheckWMI::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	try {
		if (command == _T("wmi") || command == _T("help") || command.empty()) {

			namespace po = boost::program_options;

			std::wstring query, ns, user, password, list_cls, list_inst;
			std::string computer;
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
				("list-all-ns", "list all name spaces recursively")
				("limit,l", po::value<int>(&limit), "Limit number of rows")
				("namespace,n", po::wvalue<std::wstring>(&ns)->default_value(_T("root\\cimv2")), "Namespace")
				("computer,c", po::value<std::string>(&computer), "A remote computer to connect to ")
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
					NSC_DEBUG_MSG_STD("Running query: '" + utf8::cvt<std::string>(query) + "' on: " + utf8::cvt<std::string>(ns));
					rows = wmiQuery.execute(ns, query, user, password);
				} catch (WMIException e) {
					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
					result += _T("ERROR: ") + utf8::cvt<std::wstring>(e.reason());
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
					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
					result += _T("ERROR: ") + utf8::cvt<std::wstring>(e.reason());
					return NSCAPI::hasFailed;
				}
			} else if (vm.count("list-instances")) {
				try {
					WMIQuery wmiQuery;
					rows = wmiQuery.get_instances(ns, list_inst, user, password);
					print_results(rows, limit, result, simple);
				} catch (WMIException e) {
					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
					result += _T("ERROR: ") + utf8::cvt<std::wstring>(e.reason());
					return NSCAPI::hasFailed;
				}
			} else if (vm.count("list-ns")) {
				try {
					WMIQuery wmiQuery;
					rows = wmiQuery.get_instances(ns, _T("__Namespace"), user, password);
					print_results(rows, limit, result, simple);
				} catch (WMIException e) {
					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
					result += _T("ERROR: ") + utf8::cvt<std::wstring>(e.reason());
					return NSCAPI::hasFailed;
				}
			} else if (vm.count("list-all-ns")) {
				try {
					list_ns_rec(ns, user, password, result);
				} catch (WMIException e) {
					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
					result += _T("ERROR: ") + utf8::cvt<std::wstring>(e.reason());
					return NSCAPI::hasFailed;
				}
			}
		}
		return NSCAPI::isSuccess;
	} catch (const std::exception &e) {
		result += _T("ERROR: Failed to parse command line: ") + utf8::to_unicode(e.what());
		NSC_LOG_ERROR_EXR("Failed to parse command line: ", e);
		return NSCAPI::hasFailed;
	}
}
