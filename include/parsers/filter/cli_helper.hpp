#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

#include <parsers/where/engine_impl.hpp>
#include <parsers/filter/modern_filter.hpp>

#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf.hpp>

namespace modern_filter {
	struct data_container {
		std::string filter_string, warn_string, crit_string, ok_string;
		std::string syntax_empty, syntax_ok, syntax_top, syntax_detail, syntax_perf, perf_config, empty_state, syntax_unique;
		bool debug, escape_html;
		data_container() : debug(false), escape_html(false) {}
	};

	struct perf_writer : public perf_writer_interface {
		Plugin::QueryResponseMessage::Response::Line &line;
		perf_writer(Plugin::QueryResponseMessage::Response::Line &line) : line(line) {}
		virtual void write(const parsers::where::performance_data &data) {
			::Plugin::Common::PerformanceData* perf = line.add_perf();
			perf->set_alias(data.alias);
			if (data.value_int) {
				const parsers::where::performance_data::perf_value<long long> &value = *data.value_int;
				Plugin::Common::PerformanceData::IntValue* perfData = perf->mutable_int_value();
				if (!data.unit.empty())
					perfData->set_unit(data.unit);
				perfData->set_value(value.value);
				if (value.warn)
					perfData->set_warning(*value.warn);
				if (value.crit)
					perfData->set_critical(*value.crit);
				if (value.minimum)
					perfData->set_minimum(*value.minimum);
				if (value.maximum)
					perfData->set_maximum(*value.maximum);
			} else if (data.value_double) {
				const parsers::where::performance_data::perf_value<double> &value = *data.value_double;
				Plugin::Common::PerformanceData::FloatValue* perfData = perf->mutable_float_value();
				if (!data.unit.empty())
					perfData->set_unit(data.unit);
				perfData->set_value(value.value);
				if (value.warn)
					perfData->set_warning(*value.warn);
				if (value.crit)
					perfData->set_critical(*value.crit);
				if (value.minimum)
					perfData->set_minimum(*value.minimum);
				if (value.maximum)
					perfData->set_maximum(*value.maximum);
			}
		}
	};

	template<class T>
	struct cli_helper : public  boost::noncopyable {
		data_container &data;
		boost::program_options::options_description desc;
		const Plugin::QueryRequestMessage::Request &request;
		Plugin::QueryResponseMessage::Response *response;
		bool show_all;

		cli_helper(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, data_container &data)
			: data(data)
			, desc("Allowed options for " + request.command())
			, request(request)
			, response(response)
			, show_all(false) {}

		~cli_helper() {}

		boost::program_options::options_description& get_desc() {
			return desc;
		}
		void add_options(std::string warn, std::string crit, std::string filter, std::string filter_syntax, std::string empty_state = "ignored") {
			nscapi::program_options::add_help(desc);
			boost::program_options::typed_value<std::string> *filter_op = boost::program_options::value<std::string>(&data.filter_string);
			boost::program_options::typed_value<std::string> *warn_op = boost::program_options::value<std::string>(&data.warn_string);
			boost::program_options::typed_value<std::string> *crit_op = boost::program_options::value<std::string>(&data.crit_string);
			boost::program_options::typed_value<std::string> *empty_state_op = boost::program_options::value<std::string>(&data.empty_state);
			boost::program_options::typed_value<std::string> *perf_config_op = boost::program_options::value<std::string>(&data.perf_config);
			if (!filter.empty())
				filter_op->default_value(filter);
			if (!warn.empty())
				warn_op->default_value(warn);
			if (!crit.empty())
				crit_op->default_value(crit);
			if (!empty_state.empty())
				empty_state_op->default_value(empty_state);
			if (!data.perf_config.empty())
				perf_config_op->default_value(data.perf_config);
			desc.add_options()
				("debug", boost::program_options::bool_switch(&data.debug),
					"Show debugging information in the log")
				("show-all", boost::program_options::bool_switch(&show_all),
					"Show debugging information in the log")
				("filter", filter_op,
					(std::string("Filter which marks interesting items.\nInteresting items are items which will be included in the check.\nThey do not denote warning or critical state but they are checked use this to filter out unwanted items.\nAvailable options: \n\nKey\tValue\n") + filter_syntax + "\n\n").c_str())
				("warning", warn_op,
					(std::string("Filter which marks items which generates a warning state.\nIf anything matches this filter the return status will be escalated to warning.\nAvailable options: \n\nKey\tValue\n") + filter_syntax + "\n\n").c_str())
				("warn", boost::program_options::value<std::string>(),
					"Short alias for warning")
				("critical", crit_op,
					(std::string("Filter which marks items which generates a critical state.\nIf anything matches this filter the return status will be escalated to critical.\nAvailable options: \n\nKey\tValue\n") + filter_syntax + "\n\n").c_str())
				("crit", boost::program_options::value<std::string>(),
					"Short alias for critical.")
				("ok", boost::program_options::value<std::string>(&data.ok_string),
					(std::string("Filter which marks items which generates an ok state.\nIf anything matches this any previous state for this item will be reset to ok.\nAvailable options: \n\nKey\tValue\n") + filter_syntax + "\n\n").c_str())
				("empty-state", empty_state_op,
					"Return status to use when nothing matched filter.\nIf no filter is specified this will never happen unless the file is empty.")
				("perf-config", perf_config_op,
					"Performance data generation configuration\nTODO: obj ( key: value; key: value) obj (key:valuer;key:value)")
				("escape-html", boost::program_options::bool_switch(&data.escape_html),
					"Escape any < and > characters to prevent HTML encoding")
				;
		}

		bool parse_options(boost::program_options::positional_options_description p) {
			boost::program_options::variables_map vm;
			if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response, p))
				return false;

			if (show_all)
				boost::replace_all(data.syntax_top, "${problem_list}", "${detail_list}");
			if (boost::contains(data.syntax_top, "detail_list")
				|| boost::contains(data.syntax_top, "(list)")
				|| boost::contains(data.syntax_top, "{list}")
				|| boost::contains(data.syntax_top, "match_list")
				|| boost::contains(data.syntax_top, "lines")
				)
				data.syntax_ok = "";
			if (vm.count("warn"))
				data.warn_string = vm["warn"].as<std::string>();
			if (vm.count("crit"))
				data.crit_string = vm["crit"].as<std::string>();
			return true;
		}
		bool parse_options() {
			boost::program_options::variables_map vm;
			if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response))
				return false;
			if (show_all)
				boost::replace_all(data.syntax_top, "${problem_list}", "${detail_list}");
			if (boost::contains(data.syntax_top, "detail_list")
				|| boost::contains(data.syntax_top, "(list)")
				|| boost::contains(data.syntax_top, "{list}")
				|| boost::contains(data.syntax_top, "match_list")
				|| boost::contains(data.syntax_top, "lines")
				)
				data.syntax_ok = "";
			if (vm.count("warn"))
				data.warn_string = vm["warn"].as<std::string>();
			if (vm.count("crit"))
				data.crit_string = vm["crit"].as<std::string>();
			return true;
		}
		bool parse_options(std::vector<std::string> &extra) {
			boost::program_options::variables_map vm;
			if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response, true, extra))
				return false;
			if (show_all)
				boost::replace_all(data.syntax_top, "${problem_list}", "${detail_list}");
			if (boost::contains(data.syntax_top, "detail_list")
				|| boost::contains(data.syntax_top, "(list)")
				|| boost::contains(data.syntax_top, "{list}")
				|| boost::contains(data.syntax_top, "match_list")
				|| boost::contains(data.syntax_top, "lines")
				)
				data.syntax_ok = "";
			if (vm.count("warn"))
				data.warn_string = vm["warn"].as<std::string>();
			if (vm.count("crit"))
				data.crit_string = vm["crit"].as<std::string>();
			return true;
		}

		bool empty() const {
			return data.warn_string.empty() && data.crit_string.empty();
		}
		void set_default(const std::string warn, const std::string crit) {
			if (data.warn_string.empty())
				data.warn_string = warn;
			if (data.crit_string.empty())
				data.crit_string = crit;
		}
		void set_default_filter(const std::string filter) {
			if (data.filter_string.empty())
				data.filter_string = filter;
		}
		void set_default_index(const std::string filter) {
			if (data.syntax_unique.empty())
				data.syntax_unique = filter;
		}
		bool build_filter(T &filter) {
			std::string tmp_msg;
			if (data.filter_string == "none")
				data.filter_string = "";
			if (data.ok_string == "none")
				data.ok_string = "";
			if (data.warn_string == "none")
				data.warn_string = "";
			if (data.crit_string == "none")
				data.crit_string = "";

			if (!filter.build_syntax(data.syntax_top, data.syntax_detail, data.syntax_perf, data.perf_config, data.syntax_ok, data.syntax_empty, tmp_msg)) {
				nscapi::protobuf::functions::set_response_bad(*response, tmp_msg);
				return false;
			}
			if (!data.syntax_unique.empty()) {
				if (!filter.build_index(data.syntax_unique, tmp_msg)) {
					nscapi::protobuf::functions::set_response_bad(*response, tmp_msg);
					return false;
				}
			}
			if (!filter.build_engines(data.debug, data.filter_string, data.ok_string, data.warn_string, data.crit_string)) {
				nscapi::protobuf::functions::set_response_bad(*response, "Failed to build engines");
				return false;
			}

			if (!filter.validate()) {
				nscapi::protobuf::functions::set_response_bad(*response, "Failed to validate filter see log for details");
				return false;
			}

			filter.start_match();
			return true;
		}
		void set_default_perf_config(const std::string conf) {
			data.perf_config = conf;
		}
		void add_syntax(const std::string &default_top_syntax, const std::string &syntax, const std::string &default_detail_syntax, const std::string &default_perf_syntax, const std::string &default_empty_syntax, const std::string &default_ok_syntax) {
			std::string tk = "Top level syntax.\n"
				"Used to format the message to return can include strings as well as special keywords such as: \n\nKey\tValue\n" + syntax + "\n";
			std::string dk = "Detail level syntax.\n"
				"This is the syntax of each item in the list of top-syntax (see above).\n"
				"Possible values are: \n\nKey\tValue\n" + syntax + "\n";
			std::string pk = "Performance alias syntax.\n"
				"This is the syntax for the base names of the performance data.\n"
				"Possible values are: \n\nKey\tValue\n" + syntax + "\n";
			std::string ek = "Empty syntax.\n"
				"DEPRECATED! This is the syntax for when nothing matches the filter.\n"
				"Possible values are: \n\nKey\tValue\n" + syntax + "\n";
			std::string ok = "ok syntax.\n"
				"DEPRECATED! This is the syntax for when an ok result is returned.\n"
				"This value will not be used if your syntax contains %(list) or %(count).";

			desc.add_options()
				("top-syntax", boost::program_options::value<std::string>(&data.syntax_top)->default_value(default_top_syntax), tk.c_str())
				("ok-syntax", boost::program_options::value<std::string>(&data.syntax_ok)->default_value(default_ok_syntax), ok.c_str())
				("empty-syntax", boost::program_options::value<std::string>(&data.syntax_empty)->default_value(default_empty_syntax), ek.c_str())
				("detail-syntax", boost::program_options::value<std::string>(&data.syntax_detail)->default_value(default_detail_syntax), dk.c_str())
				("perf-syntax", boost::program_options::value<std::string>(&data.syntax_perf)->default_value(default_perf_syntax), pk.c_str())
				;
		}

		void add_index(const std::string &syntax, const std::string &default_unique_syntax) {
			std::string tk = "Unique syntax.\n"
				"Used to filter unique items (counted will still increase but messages will not repeaters: \n\nKey\tValue\n" + syntax + "\n";

			desc.add_options()
				("unique-index", boost::program_options::value<std::string>(&data.syntax_unique)->default_value(default_unique_syntax), tk.c_str())
				;
		}

		void post_process(T &filter) {
			filter.match_post();
			Plugin::QueryResponseMessage::Response::Line *line = response->add_lines();
			modern_filter::perf_writer writer(*line);
			std::string msg = filter.get_message();
			if (data.escape_html) {
				boost::replace_all(msg, "<", "&lt;");
				boost::replace_all(msg, ">", "&gt;");
			}
			line->set_message(msg);
			filter.fetch_perf(&writer);
			if ((data.empty_state != "ignored") && (!filter.summary.has_matched()))
				response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(nscapi::plugin_helper::translateReturn(data.empty_state)));
			else
				response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(filter.summary.returnCode));
		}
	};
}