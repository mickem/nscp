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
std::string build_namespace(std::string ns, std::string computer) {
	if (ns.empty())
		ns = "root\\cimv2";
	if (!computer.empty())
		ns ="\\\\" + computer + "\\" + ns;
	return ns;
}

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace wmi_filter {

	struct filter_obj {
		wmi_impl::row &row;
		filter_obj(wmi_impl::row &row) : row(row) {}

		std::string get_string(const std::string col) const {
			return row.get_string(col);
		}
		long long get_int(const std::string col) const {
			return row.get_int(col);
		}
	};

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler() {

		}
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}
void CheckWMI::check_wmi(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {

	typedef wmi_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::string given_target;
	target_helper::target_info target_info;
	boost::optional<target_helper::target_info> t;
	std::string query, ns = "root\\cimv2";

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax(), "CPU Load ok");
	filter_helper.add_syntax("${list}", filter.get_format_syntax(), "CHANGE ME", "");
	filter_helper.get_desc().add_options()
		("target", po::value<std::string>(&given_target), "The target to check (for checking remote machines).")
		("user", po::value<std::string>(&target_info.username), "Remote username when checking remote machines.")
		("password", po::value<std::string>(&target_info.password), "Remote password when checking remote machines.")
		("namespace", po::value<std::string>(&ns), "The WMI root namespace to bind to.")
		("query", po::value<std::string>(&query), "The WMI query to execute.")
		;

	if (!filter_helper.parse_options())
		return;

	if (query.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No query specified");
	if (filter_helper.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No checks specified add warn/crit boundries");

	if (!given_target.empty()) {
		t = targets.find(given_target);
		if (t)
			target_info.update_from(*t);
		else
			target_info.hostname = given_target;
	}

	wmi_impl::WMIQuery::result_type rows;
	try {
		ns = build_namespace(ns, target_info.hostname);
		wmi_impl::query wmiQuery(query, ns, target_info.username, target_info.password);
		BOOST_FOREACH(const std::string &col, wmiQuery.get_columns()) {
			filter.context->registry_.add_int()
				(col, boost::bind(&wmi_filter::filter_obj::get_int, _1, col), boost::bind(&wmi_filter::filter_obj::get_string, _1, col), "Column: " + col).add_perf("", col, "");
		}

		if (!filter_helper.build_filter(filter))
			return;

		wmi_impl::row_enumerator e = wmiQuery.execute();
		while (e.has_next()) {
			boost::shared_ptr<wmi_filter::filter_obj> record(new wmi_filter::filter_obj(e.get_next()));
			boost::tuple<bool,bool> ret = filter.match(record);
		}
	} catch (const wmi_impl::wmi_exception &e) {
		return nscapi::protobuf::functions::set_response_bad(*response, "WMIQuery failed: " + e.reason());
	}
	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
}

struct pad_handler {
	std::vector<std::string::size_type> widths;
	int index;
	pad_handler() : index(0) {}
	void reset_index() {
		index = 0;
	}
	void set_next(std::string::size_type size) {
		set(index++, size);
	}
	void set(int col, std::string::size_type size) {
		if (col >= widths.size())
			widths.resize(col+20, 0);
		widths[col] = max(widths[col], size);
	}
	std::string pad_next(std::string str, char c = ' ') {
		return pad(index++, str, c);
	}
	std::string pad_current(std::string str, char c = ' ') {
		return pad(index, str, c);
	}
	std::string pad(int col, std::string str, char c = ' ') const {
		std::string::size_type w = 0;
		if (widths.size() > col)
			w = widths[col];
		if (w > str.size())
			w -= str.size();
		else
			w = 0;
		return std::string(1, c) + str + std::string(w+1, c);
	}

};
void print_pretty_results(wmi_impl::WMIQuery::result_type &rows, int limit, std::string & result) 
{
	pad_handler padder;
	//NSC_DEBUG_MSG_STD("Query returned: " + strEx::s:::xtos(rows.size()) + " rows.");
	int rownum=0;
	BOOST_FOREACH(const wmi_impl::WMIQuery::wmi_row &row, rows) {
		if (rownum++ == 0) {
			padder.reset_index();
			BOOST_FOREACH(const wmi_impl::WMIQuery::wmi_row::list_type::value_type &val, row.results) {
				padder.set_next(val.first.length());
			}
		}
		if (limit != -1 && rownum > limit)
			break;
		padder.reset_index();
		BOOST_FOREACH(const wmi_impl::WMIQuery::wmi_row::list_type::value_type &val, row.results) {
			padder.set_next(val.second.string.length());
		}
	}
	rownum=0;
	BOOST_FOREACH(const wmi_impl::WMIQuery::wmi_row &row, rows) {
		if (rownum++ == 0) {
			std::string row1 = "|";
			std::string row2 = "|";
			padder.reset_index();
			BOOST_FOREACH(const wmi_impl::WMIQuery::wmi_row::list_type::value_type &val, row.results) {
				row1 += padder.pad_current(utf8::cvt<std::string>(val.first)) + "|";
				row2 += padder.pad_next("", '-') + "|";
			}
			result += row1 + "\n";
			result += row2 + "\n";
		}

		if (limit != -1 && rownum > limit)
			break;
		std::string row1 = "|";
		padder.reset_index();
		BOOST_FOREACH(const wmi_impl::WMIQuery::wmi_row::list_type::value_type &val, row.results) {
			row1 += padder.pad_next(val.second.get_string()) + "|";
		}
		result += row1 + "\n";
	}
}

void print_simple_results(wmi_impl::WMIQuery::result_type &rows, int limit, std::string & result) 
{
	int rownum=0;
	BOOST_FOREACH(const wmi_impl::WMIQuery::wmi_row &row, rows) {
		if (limit != -1 && rownum > limit)
			break;
		bool first = true;
		BOOST_FOREACH(const wmi_impl::WMIQuery::wmi_row::list_type::value_type &val, row.results) {
			if (first) 
				first = false;
			else
				result += ",";
			result += val.second.get_string();
		}
		result += "\n";
	}
}
void print_results(wmi_impl::WMIQuery::result_type &rows, int limit, std::string & result, bool simple)  {
	if (simple)
		print_simple_results(rows, limit, result);
	else
		print_pretty_results(rows, limit, result);
}


void list_ns_rec(std::wstring ns, std::wstring user, std::wstring password, std::string &result) {
	try {
		wmi_impl::WMIQuery wmiQuery;
		wmi_impl::WMIQuery::result_type rows = wmiQuery.get_instances(ns, _T("__Namespace"), user, password);
		BOOST_FOREACH(wmi_impl::WMIQuery::wmi_row &row, rows) {
			const wmi_impl::WMIQuery::WMIResult &res = row.results[std::wstring(_T("Name"))];
			//const WMIQuery::wmi_row::list_type::value_type v = row.results[_T("Name")];
			std::wstring name = res.string;
			result += utf8::cvt<std::string>(ns) + "\\" + utf8::cvt<std::string>(name) + "\n";
			list_ns_rec(ns + _T("\\") + name, user, password, result);
		}
	} catch (const wmi_impl::wmi_exception &e) {
		NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
		result += "ERROR: " + e.reason();
	}
}


NSCAPI::nagiosReturn CheckWMI::commandLineExec(const std::string &command, const std::list<std::string> &arguments, std::string &result) {
	try {
		if (command == "wmi" || command == "help" || command.empty()) {

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

			if (command == "help") {
				std::stringstream ss;
				ss << "wmi Command line syntax:" << std::endl;
				ss << desc;
				result = ss.str();
				return NSCAPI::isSuccess;
			}

			std::vector<std::string> args(arguments.begin(), arguments.end());
			po::parsed_options parsed = po::basic_command_line_parser<char>(args).options(desc).run();
			po::store(parsed, vm);
			po::notify(vm);

			if (vm.count("help") || (vm.count("select") == 0 && vm.count("list-classes") == 0 && vm.count("list-instances") == 0 && vm.count("list-ns") == 0 && vm.count("list-all-ns") == 0)) {
				std::stringstream ss;
				ss << "CheckWMI Command line syntax:" << std::endl;
				ss << desc;
				result = ss.str();
				return NSCAPI::isSuccess;
			}
			simple = vm.count("simple") > 0;

			ns = build_namespace(ns, computer);

			wmi_impl::WMIQuery::result_type rows;
			if (vm.count("select")) {
// 				try {
// 					wmi_impl::WMIQuery wmiQuery;
// 					NSC_DEBUG_MSG_STD("Running query: '" + utf8::cvt<std::string>(query) + "' on: " + utf8::cvt<std::string>(ns));
// 					rows = wmiQuery.execute(ns, query, user, password);
// 				} catch (wmi_impl::wmi_exception e) {
// 					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
// 					result += "ERROR: " + e.reason();
// 					return NSCAPI::hasFailed;
// 				}
// 				if (rows.empty()) {
// 					result += "No result";
// 					return NSCAPI::isSuccess;
// 				} else {
// 					print_results(rows, limit, result, simple);
// 				}
			} else if (vm.count("list-classes")) {
				try {
					wmi_impl::WMIQuery wmiQuery;
					rows = wmiQuery.get_classes(ns, list_cls, user, password);
					print_results(rows, limit, result, simple);
				} catch (wmi_impl::wmi_exception e) {
					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
					result += "ERROR: " + e.reason();
					return NSCAPI::hasFailed;
				}
			} else if (vm.count("list-instances")) {
				try {
					wmi_impl::WMIQuery wmiQuery;
					rows = wmiQuery.get_instances(ns, list_inst, user, password);
					print_results(rows, limit, result, simple);
				} catch (wmi_impl::wmi_exception e) {
					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
					result += "ERROR: " + e.reason();
					return NSCAPI::hasFailed;
				}
			} else if (vm.count("list-ns")) {
				try {
					wmi_impl::WMIQuery wmiQuery;
					rows = wmiQuery.get_instances(ns, _T("__Namespace"), user, password);
					print_results(rows, limit, result, simple);
				} catch (wmi_impl::wmi_exception e) {
					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
					result += "ERROR: " + e.reason();
					return NSCAPI::hasFailed;
				}
			} else if (vm.count("list-all-ns")) {
				try {
					list_ns_rec(ns, user, password, result);
				} catch (wmi_impl::wmi_exception e) {
					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
					result += "ERROR: " + e.reason();
					return NSCAPI::hasFailed;
				}
			}
		}
		return NSCAPI::isSuccess;
	} catch (const std::exception &e) {
		result += "ERROR: Failed to parse command line: " + utf8::utf8_from_native(e.what());
		NSC_LOG_ERROR_EXR("Failed to parse command line: ", e);
		return NSCAPI::hasFailed;
	}
}
