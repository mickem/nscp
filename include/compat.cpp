#include <compat.hpp>

#include <vector>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>

namespace compat {

	namespace po = boost::program_options;

	void log_args(const Plugin::QueryRequestMessage::Request &request) {
		std::stringstream ss;
		for (int i=0;i<request.arguments_size();i++) {
			if (i>0)
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
			(std::string("MaxWarn"+suffix).c_str(), po::value<std::vector<std::string> >(), "Maximum value before a warning is returned.")
			(std::string("MaxCrit"+suffix).c_str(), po::value<std::vector<std::string> >(), "Maximum value before a critical is returned.")
			(std::string("MinWarn"+suffix).c_str(), po::value<std::vector<std::string> >(), "Minimum value before a warning is returned.")
			(std::string("MinCrit"+suffix).c_str(), po::value<std::vector<std::string> >(), "Minimum value before a critical is returned.")
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
			if (bounds.size() > 1 || !target.empty())
				NSC_DEBUG_MSG("Multiple boundries of the same kind is not supported");
			if (bounds.size() > 0) {
				std::string value = bounds.front();
				if (value.size() > 3 && value[2] == ':')
					target = var + "=" + bound + " " + value.substr(0,2) + " " + value.substr(3);
				else
					target = var + "=" + bound + op + bounds.front();
			}
		}
	}
	void matchFirstNumeric(const boost::program_options::variables_map &vm, const std::string upper, const std::string lower, std::string &warn, std::string &crit, const std::string suffix) {
		do_matchFirstNumeric(vm, "MaxWarn" + suffix, warn, "warn", upper, ">");
		do_matchFirstNumeric(vm, "MaxCrit" + suffix, crit, "crit", upper, ">");
		do_matchFirstNumeric(vm, "MinWarn" + suffix, warn, "warn", lower, "<");
		do_matchFirstNumeric(vm, "MinCrit" + suffix, crit, "crit", lower, "<");
	}
	void matchFirstOldNumeric(const boost::program_options::variables_map &vm, const std::string var, std::string &warn, std::string &crit) {
		do_matchFirstNumeric(vm, "warn", warn, "warn", var, ">");
		do_matchFirstNumeric(vm, "crit", crit, "crit", var, ">");
	}

	void matchShowAll(const boost::program_options::variables_map &vm, Plugin::QueryRequestMessage::Request &request, std::string prefix) {
		if (vm.count("ShowAll")) {
			request.add_arguments("top-syntax=" + prefix + "${list}");
		} else {
			request.add_arguments("top-syntax=" + prefix + "${problem_list}");
		}

	}

}
