#pragma once
#include "where_filter.hpp"
#define DATE_FORMAT "%#c"

namespace where_filter {



	template<typename object_type, typename handler_type, typename argument_type>
	struct engine_impl : public engine_interface<object_type> {
		typedef boost::shared_ptr<handler_type> handler_instance_type;

		argument_type data;
		parsers::where::parser ast_parser;
		handler_instance_type object_handler;
		std::string filter_string;
		bool perf_collection;
		typedef std::map<std::string,std::string> boundries_type;
		boundries_type boundries;

		engine_impl(argument_type data) : data(data), object_handler(handler_instance_type(new handler_type())), perf_collection(false) {
			filter_string = data->filter;
		}
		engine_impl(argument_type data, std::string filter) : data(data), object_handler(handler_instance_type(new handler_type())), filter_string(filter) {}
		bool boot() {
			return true; 
		}

		boundries_type fetch_performance_data() {
			return boundries;
		}

		void enabled_performance_collection() {
			perf_collection = true;
		}

		bool validate(std::string &message) {
			if (data->debug)
				data->error->report_debug("Parsing: " + filter_string);

			if (!ast_parser.parse(filter_string)) {
				data->error->report_error("Parsing failed of '" + filter_string + "' at: " + ast_parser.rest);
				message = "Parsing failed: " + ast_parser.rest;
				return false;
			}
			if (data->debug)
				data->error->report_debug("Parsing succeeded: " + ast_parser.result_as_tree());

			if (!ast_parser.derive_types(object_handler) || object_handler->has_error()) {
				message = "Invalid types: " + object_handler->get_error();
				return false;
			}
			if (data->debug)
				data->error->report_debug("Type resolution succeeded: " + ast_parser.result_as_tree());

			if (!ast_parser.bind(object_handler) || object_handler->has_error()) {
				message = "Variable and function binding failed: " + object_handler->get_error();
				return false;
			}
			if (data->debug)
				data->error->report_debug("Binding succeeded: " + ast_parser.result_as_tree());

			if (!ast_parser.static_eval(object_handler) || object_handler->has_error()) {
				message = "Static evaluation failed: " + object_handler->get_error();
				return false;
			}
			if (data->debug)
				data->error->report_debug("Static evaluation succeeded: " + ast_parser.result_as_tree());

			if (perf_collection) {
				if (!ast_parser.collect_perfkeys(boundries, object_handler) || object_handler->has_error()) {
					message = "Collection of perfkeys failed: " + object_handler->get_error();
					return false;
				}
			}

			return true;
		}

		bool match(boost::shared_ptr<object_type> record) {
			object_handler->set_current_element(record);
			bool ret = ast_parser.evaluate(object_handler);
			if (object_handler->has_error()) {
				data->error->report_error("Error: " + object_handler->get_error());
			}
			return ret;
		}
		std::string get_name() {
			return "where";
		}
		std::string get_subject() { return filter_string; }
	};

	
	class collect_error_handler : public where_filter::error_handler_interface {
		int error_count_;
		int warning_count_;
		std::string first_error_;
		std::string last_error_;
		std::string all_errors_;
		std::string all_warnings_;
	public:
		collect_error_handler() : error_count_(0), warning_count_(0) {}
		void report_error(std::string error) {
			if (error_count_==0)
				first_error_ = error;
			else
				last_error_ = error;
			error_count_++;
			strEx::append_list(all_errors_, error);
		}
		void report_warning(std::string error) {
			warning_count_++;
			strEx::append_list(all_errors_, error);
		}
		void report_debug(std::string error) {
		}
		std::string get_error() {
			return strEx::s::xtos(error_count_) + " errors and " + strEx::s::xtos(warning_count_) + " warnings where returned: " + first_error_;
		}
		bool has_error() {
			return error_count_ > 0;
		}
	};
	class nscapi::core_wrapper;
	class nsc_error_handler : public where_filter::error_handler_interface {
		int error_count_;
		int warning_count_;
		std::string first_error_;
		std::string last_error_;
		nscapi::core_wrapper *core_;
	public:
		nsc_error_handler(nscapi::core_wrapper *core) : error_count_(0), warning_count_(0), core_(core) {}
		void report_error(std::string error) {
			if (error_count_==0)
				first_error_ = error;
			else
				last_error_ = error;
			error_count_++;
			if (core_->should_log(NSCAPI::log_level::error)) { 
				core_->log(NSCAPI::log_level::error, __FILE__, __LINE__, utf8::cvt<std::string>(error));
			}
		}
		void report_warning(std::string error) {
			if (core_->should_log(NSCAPI::log_level::warning)) { 
				core_->log(NSCAPI::log_level::warning, __FILE__, __LINE__, utf8::cvt<std::string>(error));
			}
		}
		void report_debug(std::string error) {
			if (core_->should_log(NSCAPI::log_level::debug)) { 
				core_->log(NSCAPI::log_level::debug, __FILE__, __LINE__, utf8::cvt<std::string>(error));
			}
		}
		std::string get_error() {
			return strEx::s::xtos(error_count_) + " errors and " + strEx::s::xtos(warning_count_) + " warnings where returned: " + first_error_;
		}
		bool has_error() {
			return error_count_ > 0;
		}
	};

	template<typename record_type>
	struct simple_count_result : public result_counter_interface<record_type> {
		unsigned int count_;
		unsigned int match_;
		std::string message_;
		std::string syntax_;
		std::string date_syntax_;
		bool debug_;
		boost::shared_ptr<where_filter::error_handler_interface> error_;

		simple_count_result(boost::shared_ptr<argument_interface > argument) : count_(0), match_(0), syntax_(argument->syntax), date_syntax_(argument->date_syntax), debug_(argument->debug), error_(argument->error) {}
		void process(boost::shared_ptr<record_type> record, bool result) {
			count_++;
			if (result) {
				strEx::append_list(message_, record->render(syntax_, date_syntax_));
				match_++;
			} else {
// 				if (debug_)
// 					error_->report_debug(_T("==> NO Matched: ") + record->render(syntax_, DATE_FORMAT));
			}
		}
		unsigned int get_total_count() { return count_; }
		unsigned int get_match_count() { return match_; }
		std::string get_message() { return message_; }
		std::string render(std::string syntax, int code) {
			strEx::replace(syntax, "%list%", message_);
			strEx::replace(syntax, "%matches%", strEx::s::xtos(count_));
			strEx::replace(syntax, "%total%", strEx::s::xtos(match_));
			strEx::replace(syntax, "%status%", strEx::s::xtos(code));
			return syntax;
		}
	};

	struct error_gatherer {
		typedef std::list<std::string> error_type;
		void error(std::string err) {
			errors.push_back(err);
		}
		bool has_error() {
			return !errors.empty();
		}
		std::string get_error() {
			std::string ret;
			BOOST_FOREACH(std::string s, errors) {
				if (!ret.empty()) ret += ", ";
				ret += s;
			}
			return ret;
		}
	private:
		error_type errors;
	};

}
