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

#include <map>
#include <vector>
#include <algorithm>

#include <boost/program_options.hpp>

#include "CheckWMI.h"
#include <str/xtos.hpp>
#include <time.h>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_settings_helper.hpp>

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
			(target.hostname, "Targets", "A list of available remote target systems")

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

bool CheckWMI::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	sh::settings_registry settings(get_settings_proxy());
	//settings.set_alias(_T("targets"));

	settings.add_path_to_settings()
		("targets", sh::fun_values_path(boost::bind(&target_helper::add_target, &targets, get_settings_proxy(), _1, _2)),
			"TARGET LIST SECTION", "A list of available remote target systems",
			"TARGET DEFENTION", "For more configuration options add a dedicated section")
		;

	settings.register_all();
	settings.notify();

	return true;
}
bool CheckWMI::unloadModule() {
	return true;
}

std::string build_namespace(std::string ns, std::string computer) {
	if (ns.empty())
		ns = "root\\cimv2";
	if (!computer.empty())
		ns = "\\\\" + computer + "\\" + ns;
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
		std::string get_row() const {
			return row.to_string();
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
	filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
	filter_helper.add_syntax("${list}", "%(line)", "", "", "");
	filter_helper.get_desc().add_options()
		("target", po::value<std::string>(&given_target), "The target to check (for checking remote machines).")
		("user", po::value<std::string>(&target_info.username), "Remote username when checking remote machines.")
		("password", po::value<std::string>(&target_info.password), "Remote password when checking remote machines.")
		("namespace", po::value<std::string>(&ns)->default_value("root\\cimv2"), "The WMI root namespace to bind to.")
		("query", po::value<std::string>(&query), "The WMI query to execute.")
		;

	if (!filter_helper.parse_options())
		return;

	if (query.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No query specified");

	if (!given_target.empty()) {
		t = targets.find(given_target);
		if (t)
			target_info.update_from(*t);
		else
			target_info.hostname = given_target;
	}

	try {
		ns = build_namespace(ns, target_info.hostname);
		wmi_impl::query wmiQuery(query, ns, target_info.username, target_info.password);
		filter.context->registry_.add_string()
			("line", boost::bind(&wmi_filter::filter_obj::get_row, _1), "Get a list of all columns");
		BOOST_FOREACH(const std::string &col, wmiQuery.get_columns()) {
			filter.context->registry_.add_int()
				(col, boost::bind(&wmi_filter::filter_obj::get_int, _1, col), boost::bind(&wmi_filter::filter_obj::get_string, _1, col), "Column: " + col).add_perf("", col, "");
		}

		if (!filter_helper.build_filter(filter))
			return;

		wmi_impl::row_enumerator e = wmiQuery.execute();
		while (e.has_next()) {
			boost::shared_ptr<wmi_filter::filter_obj> record(new wmi_filter::filter_obj(e.get_next()));
			filter.match(record);
		}
	} catch (const wmi_impl::wmi_exception &e) {
		return nscapi::protobuf::functions::set_response_bad(*response, "WMIQuery failed: " + e.reason());
	}
	filter_helper.post_process(filter);
}

inline std::string pad(const std::string &s, const std::size_t &c) {
	return s + std::string(c - s.size(), ' ');
}

typedef  std::vector<std::string> row_type;
std::string render_table(const std::vector<std::size_t> &widths, const row_type &headers, const std::list<row_type> &rows) {
	std::size_t count = widths.size();
	std::stringstream ss;
	std::string line;
	for (int i = 0; i < count; ++i) {
		line += std::string(widths[i] + 3, '-');
	}
	ss << line << "\n";
	if (headers.size() != widths.size())
		throw wmi_impl::wmi_exception(E_INVALIDARG, "Invalid header size");
	for (int i = 0; i < count; ++i)
		ss << " " << pad(headers[i], widths[i]) << " ";
	ss << "\n" << line << "\n";
	BOOST_FOREACH(const row_type &row, rows) {
		if (row.size() != widths.size())
			throw wmi_impl::wmi_exception(E_INVALIDARG, "Invalid row size");
		for (int i = 0; i < count; ++i)
			ss << " " << pad(row[i], widths[i]) << " ";
		ss << "\n";
	}
	ss << line;
	return ss.str();
}

std::string render(const row_type &headers, std::vector<std::size_t> &widths, wmi_impl::row_enumerator e) {
	std::list<row_type> rows;
	std::size_t count = widths.size();
	while (e.has_next()) {
		wmi_impl::row wmi_row = e.get_next();
		row_type row;
		for (std::size_t i = 0; i < count; i++) {
			std::string c = wmi_row.get_string(headers[i]);
			widths[i] = (std::max)(c.size(), widths[i]);
			row.push_back(c);
		}
		rows.push_back(row);
	}
	return render_table(widths, headers, rows);
}

std::string list_ns_rec(std::string ns, std::string user, std::string password) {
	std::stringstream ss;
	wmi_impl::instances impl("__Namespace", ns, user, password);
	wmi_impl::row_enumerator e = impl.get();
	while (e.has_next()) {
		wmi_impl::row wmi_row = e.get_next();
		std::string str = wmi_row.get_string("Name");
		ss << ns << "\\" << str << "\n";
		ss << list_ns_rec(ns + "\\" + str, user, password);
	}
	return ss.str();
}

NSCAPI::nagiosReturn CheckWMI::commandLineExec(const int target_mode, const std::string &command, const std::list<std::string> &arguments, std::string &result) {
	try {
		if (command == "wmi" || command == "help" || command.empty()) {
			namespace po = boost::program_options;

			std::string query, ns, user, password, list_cls, list_inst;
			std::string computer;
			bool simple;
			int limit = -1;
			po::options_description desc("Allowed options");
			desc.add_options()
				("help,h", "Show help screen")
				("select,s", po::value<std::string>(&query), "Execute a query")
				("simple", "Use simple format")
				("list-classes", po::value<std::string>(&list_cls)->implicit_value(""), "list all classes of a given type")
				("list-instances", po::value<std::string>(&list_inst), "list all instances of a given type")
				("list-ns", "list all name spaces")
				("list-all-ns", "list all name spaces recursively")
				("limit,l", po::value<int>(&limit), "Limit number of rows")
				("namespace,n", po::value<std::string>(&ns)->default_value("root\\cimv2"), "Namespace")
				("computer,c", po::value<std::string>(&computer), "A remote computer to connect to ")
				("user,u", po::value<std::string>(&user), "The user for the remote computer")
				("password,p", po::value<std::string>(&password), "The password for the remote computer")
				;

			boost::program_options::variables_map vm;

			if (command == "help") {
				std::stringstream ss;
				ss << "wmi Command line syntax:" << std::endl;
				ss << desc;
				result = ss.str();
				return NSCAPI::exec_return_codes::returnOK;
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
				return NSCAPI::exec_return_codes::returnOK;
			}
			simple = vm.count("simple") > 0;

			ns = build_namespace(ns, computer);

			if (vm.count("select")) {
				row_type headers;
				std::vector<std::size_t> widths;
				std::size_t count = 0;
				try {
					wmi_impl::query wmiQuery(query, ns, user, password);
					std::list<std::string> cols = wmiQuery.get_columns();
					count = cols.size();
					BOOST_FOREACH(const std::string &col, cols) {
						headers.push_back(col);
						widths.push_back(col.size());
					}
					result = render(headers, widths, wmiQuery.execute());
					return NSCAPI::exec_return_codes::returnOK;
				} catch (const wmi_impl::wmi_exception &e) {
					result += "ERROR: " + e.reason();
					return NSCAPI::exec_return_codes::returnERROR;
				}
			} else if (vm.count("list-classes")) {
				try {
					std::stringstream ss;
					wmi_impl::classes query(list_cls, ns, user, password);
					wmi_impl::row_enumerator e = query.get();
					while (e.has_next()) {
						wmi_impl::row wmi_row = e.get_next();
						ss << wmi_row.get_string("__CLASS") << "\n";
					}
					result = ss.str();
					return NSCAPI::exec_return_codes::returnOK;
				} catch (const wmi_impl::wmi_exception &e) {
					result += "ERROR: " + e.reason();
					return NSCAPI::exec_return_codes::returnERROR;
				}
			} else if (vm.count("list-instances")) {
				try {
					std::stringstream ss;
					wmi_impl::instances query(list_inst, ns, user, password);
					wmi_impl::row_enumerator e = query.get();
					while (e.has_next()) {
						wmi_impl::row wmi_row = e.get_next();
						ss << wmi_row.get_string("Name") << "\n";
					}
					result = ss.str();
					return NSCAPI::exec_return_codes::returnOK;
				} catch (const wmi_impl::wmi_exception &e) {
					result += "ERROR: " + e.reason();
					return NSCAPI::exec_return_codes::returnERROR;
				}
			} else if (vm.count("list-ns")) {
				try {
					std::stringstream ss;
					wmi_impl::instances query("__Namespace", ns, user, password);
					wmi_impl::row_enumerator e = query.get();
					while (e.has_next()) {
						wmi_impl::row wmi_row = e.get_next();
						ss << wmi_row.get_string("Name") << "\n";
					}
					result = ss.str();
					return NSCAPI::exec_return_codes::returnOK;
				} catch (wmi_impl::wmi_exception e) {
					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
					result += "ERROR: " + e.reason();
					return NSCAPI::exec_return_codes::returnERROR;
				}
			} else if (vm.count("list-all-ns")) {
				try {
					result = list_ns_rec(ns, user, password);
				} catch (wmi_impl::wmi_exception e) {
					NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
					result += "ERROR: " + e.reason();
					return NSCAPI::exec_return_codes::returnERROR;
				}
			}
			return NSCAPI::exec_return_codes::returnOK;
		}
		return NSCAPI::cmd_return_codes::returnIgnored;
	} catch (std::exception e) {
		result += "ERROR: " + utf8::utf8_from_native(e.what());
		return NSCAPI::exec_return_codes::returnERROR;
	}
}