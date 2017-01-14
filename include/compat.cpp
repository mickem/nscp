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

#include <compat.hpp>

#include <vector>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

namespace compat {
	namespace po = boost::program_options;

	void log_args(const Plugin::QueryRequestMessage::Request &request) {
		std::stringstream ss;
		for (int i = 0; i < request.arguments_size(); i++) {
			if (i > 0)
				ss << " ";
			if (request.arguments(i).find(" ") != std::string::npos)
				ss << '\"' << request.arguments(i) << '\"';
			else
				ss << request.arguments(i);
		}
		NSC_DEBUG_MSG("Created command: " + ss.str());
	}

	void addShowAll(boost::program_options::options_description &desc) {
		desc.add_options()
			("ShowAll", po::value<std::string>()->implicit_value("short"), "Configures display format (if set shows all items not only failures, if set to long shows all cores).")
			;
	}
	void addAllNumeric(boost::program_options::options_description &desc, const std::string suffix) {
		desc.add_options()
			(std::string("MaxWarn" + suffix).c_str(), po::value<std::vector<std::string> >(), "Maximum value before a warning is returned.")
			(std::string("MaxCrit" + suffix).c_str(), po::value<std::vector<std::string> >(), "Maximum value before a critical is returned.")
			(std::string("MinWarn" + suffix).c_str(), po::value<std::vector<std::string> >(), "Minimum value before a warning is returned.")
			(std::string("MinCrit" + suffix).c_str(), po::value<std::vector<std::string> >(), "Minimum value before a critical is returned.")
			;
	}
	void addOldNumeric(boost::program_options::options_description &desc) {
		desc.add_options()
			(std::string("warn").c_str(), po::value<std::vector<std::string> >(), "Maximum value before a warning is returned.")
			(std::string("crit").c_str(), po::value<std::vector<std::string> >(), "Maximum value before a critical is returned.")
			;
	}

	void do_matchFirstNumeric(const boost::program_options::variables_map &vm, const std::string key, std::string &target, const std::string var, const std::string bound, const std::string op) {
		if (vm.count(key)) {
			std::vector<std::string> bounds = vm[key].as<std::vector<std::string> >();
			if (bounds.size() > 1)
				NSC_DEBUG_MSG("Multiple boundries of the same kind is not supported");
			if (bounds.size() > 0) {
				std::string expr = "";
				std::string value = bounds.front();
				if (value.size() > 3 && value[2] == ':')
					expr = bound + " " + value.substr(0, 2) + " " + value.substr(3);
				else
					expr = bound + op + bounds.front();
				if (target.empty())
					target = var + "=" + expr;
				else
					target = var + "=( " + target.substr(var.length()+1) + " ) or ( " + expr + " )";
			}
		}
	}
	bool hasFirstNumeric(const boost::program_options::variables_map &vm, const std::string suffix) {
		return vm.count("MaxWarn" + suffix) > 0 || vm.count("MaxCrit" + suffix) > 0 || vm.count("MinWarn" + suffix) > 0 || vm.count("MinCrit" + suffix) > 0;
	}
	void matchFirstNumeric(const boost::program_options::variables_map &vm, const std::string upper, const std::string lower, std::string &warn, std::string &crit, const std::string suffix) {
		do_matchFirstNumeric(vm, "MaxWarn" + suffix, warn, "warn", upper, ">=");
		do_matchFirstNumeric(vm, "MaxCrit" + suffix, crit, "crit", upper, ">=");
		do_matchFirstNumeric(vm, "MinWarn" + suffix, warn, "warn", lower, "<=");
		do_matchFirstNumeric(vm, "MinCrit" + suffix, crit, "crit", lower, "<=");
	}
	void matchFirstOldNumeric(const boost::program_options::variables_map &vm, const std::string var, std::string &warn, std::string &crit) {
		do_matchFirstNumeric(vm, "warn", warn, "warn", var, ">=");
		do_matchFirstNumeric(vm, "crit", crit, "crit", var, ">=");
	}

	void matchShowAll(const boost::program_options::variables_map &vm, Plugin::QueryRequestMessage::Request &request, std::string prefix) {
		if (vm.count("ShowAll")) {
			request.add_arguments("show-all");
		}
	}
}