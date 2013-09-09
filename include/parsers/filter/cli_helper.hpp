
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

#include <protobuf/plugin.pb.h>

#include <parsers/expression/expression.hpp>
#include <parsers/where/engine_impl.hpp>
#include <parsers/filter/modern_filter.hpp>

#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>


namespace modern_filter {
	struct data_container {
		std::string filter_string, warn_string, crit_string, ok_string;
		std::string syntax_top, syntax_detail, syntax_perf, empty_detail, empty_state;
		bool debug;
	};
	template<class T>
	struct cli_helper : public  boost::noncopyable {

		data_container &data;
		boost::program_options::options_description desc;
		const Plugin::QueryRequestMessage::Request &request;
		Plugin::QueryResponseMessage::Response *response;


		cli_helper(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, data_container &data)
			: data(data)
			, desc("Allowed options for " + request.command())
			, request(request)
			, response(response)
		{
		}

		~cli_helper() {
		}

		boost::program_options::options_description& get_desc() {
			return desc;
		}

		void add_options(std::string filter_syntax, std::string empty_text = "No matches", std::string empty_state = "unknown") {
			nscapi::program_options::add_help(desc);
			desc.add_options()
				("debug", boost::program_options::bool_switch(&data.debug),
				"Show debugging information in the log")
				("filter", boost::program_options::value<std::string>(&data.filter_string),
				(std::string("Filter which marks interesting items.\nInteresting items are items which will be included in the check.\nThey do not denote warning or critical state but they are checked use this to filter out unwanted items.\n\nAvalible options: \n") + filter_syntax).c_str())
				("warning", boost::program_options::value<std::string>(&data.warn_string),
				(std::string("Filter which marks items which generates a warning state.\nIf anything matches this filter the return status will be escalated to warning.\n\nAvalible options: \n") + filter_syntax).c_str())
				("warn", boost::program_options::value<std::string>(&data.warn_string),
				"Short alias for warning")
				("critical", boost::program_options::value<std::string>(&data.crit_string),
				(std::string("Filter which marks items which generates a critical state.\nIf anything matches this filter the return status will be escalated to critical.\n\nAvalible options: \n") + filter_syntax).c_str())
				("crit", boost::program_options::value<std::string>(&data.crit_string),
				"Short alias for critical.")
				("ok", boost::program_options::value<std::string>(&data.ok_string),
				(std::string("Filter which marks items which generates an ok state.\nIf anything matches this any previous state for this item will be reset to ok.\n\nAvalible options: \n") + filter_syntax).c_str())
				("empty-syntax", boost::program_options::value<std::string>(&data.empty_detail)->default_value(empty_text), 
				"Message to display when nothing matched filter.\nIf no filter is specified this will never happen unless the file is empty.")
				("empty-state", boost::program_options::value<std::string>(&data.empty_state)->default_value(empty_state), 
				"Return status to use when nothing matched filter.\nIf no filter is specified this will never happen unless the file is empty.")
				;
		}

		bool parse_options() {
			boost::program_options::variables_map vm;
			if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
				return false;
			return true;
		}

		bool empty() const {
			return data.warn_string.empty() && data.crit_string.empty();
		}
		void set_default(const std::string warn, const std::string crit) {
			data.warn_string = warn;
			data.crit_string = crit;
		}
		bool build_filter(T &filter) {
			std::string tmp_msg;
			if (data.filter_string == "none")
				data.filter_string = "";

			if (!filter.build_syntax(data.syntax_top, data.syntax_detail, data.syntax_perf, tmp_msg)) {
				nscapi::protobuf::functions::set_response_bad(*response, tmp_msg);
				return false;
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
		void add_syntax(const std::string &default_top_syntax, const std::string &syntax, const std::string &default_detail_syntax, const std::string &default_perf_syntax) {
			std::string tmp_msg;

			std::string tk = "Top level syntax.\n"
				"Used to format the message to return can include strings as well as special keywords such as:\n"
				+ syntax; 
			std::string dk = "Detail level syntax.\n"
				"This is the syntax of each item in the list of top-syntax (see above).\n"
				"Possible values are:\n"
				+ syntax;
			std::string pk = "Performance alias syntax.\n"
				"This is the syntax of each item in the list of top-syntax (see above).\n"
				"Possible values are:\n"
				+ syntax;

			desc.add_options()
				("top-syntax", boost::program_options::value<std::string>(&data.syntax_top)->default_value(default_top_syntax), tk.c_str())
				("detail-syntax", boost::program_options::value<std::string>(&data.syntax_detail)->default_value(default_detail_syntax), dk.c_str())
				("perf-syntax", boost::program_options::value<std::string>(&data.syntax_perf)->default_value(default_perf_syntax), pk.c_str())
				;
		}

		void post_process(T &filter, parsers::where::perf_writer_interface* writer) {
			filter.end_match();
			filter.fetch_perf(writer);
			if (!filter.summary.has_matched()) {
				response->set_message(data.empty_detail);
				response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(nscapi::plugin_helper::translateReturn(data.empty_state)));
				return;
			}
			std::string msg = filter.get_message();
			if (msg.empty()) msg = data.empty_detail;
			response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(filter.returnCode));
			response->set_message(msg);
		}

	};


	struct perf_writer : public parsers::where::perf_writer_interface {
		Plugin::QueryResponseMessage::Response *response;
		std::string unit;
		perf_writer(Plugin::QueryResponseMessage::Response *response) : response(response) {}
		virtual void write(const parsers::where::performance_data &data) {
			::Plugin::Common::PerformanceData* perf = response->add_perf();
			perf->set_alias(data.alias);
			if (data.value_int) {
				const parsers::where::performance_data::perf_value<long long> &value = *data.value_int;
				perf->set_type(::Plugin::Common_DataType_INT);
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
				perf->set_type(::Plugin::Common_DataType_INT);
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
}