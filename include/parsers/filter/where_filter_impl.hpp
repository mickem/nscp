#pragma once
#include "where_filter.hpp"
#define DATE_FORMAT _T("%#c")

namespace where_filter {



	template<typename object_type, typename handler_type, typename argument_type>
	struct engine_impl : public engine_interface<object_type> {
		typedef boost::shared_ptr<handler_type> handler_instance_type;

		argument_type data;
		parsers::where::parser ast_parser;
		handler_instance_type object_handler;
		std::wstring filter_string;
		bool perf_collection;
		typedef std::map<std::wstring,std::wstring> boundries_type;
		boundries_type boundries;

		engine_impl(argument_type data) : data(data), object_handler(handler_instance_type(new handler_type())), perf_collection(false) {
			filter_string = data->filter;
		}
		engine_impl(argument_type data, std::wstring filter) : data(data), object_handler(handler_instance_type(new handler_type())), filter_string(filter) {}
		bool boot() {
			return true; 
		}

		boundries_type fetch_performance_data() {
			return boundries;
		}

		void enabled_performance_collection() {
			perf_collection = true;
		}

		bool validate(std::wstring &message) {
			if (data->debug)
				data->error->report_debug(_T("Parsing: ") + filter_string);

			if (!ast_parser.parse(filter_string)) {
				data->error->report_error(_T("Parsing failed of '") + filter_string + _T("' at: ") + ast_parser.rest);
				message = _T("Parsing failed: ") + ast_parser.rest;
				return false;
			}
			if (data->debug)
				data->error->report_debug(_T("Parsing succeeded: ") + ast_parser.result_as_tree());

			if (!ast_parser.derive_types(object_handler) || object_handler->has_error()) {
				message = _T("Invalid types: ") + object_handler->get_error();
				return false;
			}
			if (data->debug)
				data->error->report_debug(_T("Type resolution succeeded: ") + ast_parser.result_as_tree());

			if (!ast_parser.bind(object_handler) || object_handler->has_error()) {
				message = _T("Variable and function binding failed: ") + object_handler->get_error();
				return false;
			}
			if (data->debug)
				data->error->report_debug(_T("Binding succeeded: ") + ast_parser.result_as_tree());

			if (!ast_parser.static_eval(object_handler) || object_handler->has_error()) {
				message = _T("Static evaluation failed: ") + object_handler->get_error();
				return false;
			}
			if (data->debug)
				data->error->report_debug(_T("Static evaluation succeeded: ") + ast_parser.result_as_tree());

			if (perf_collection) {
				if (!ast_parser.collect_perfkeys(boundries, object_handler) || object_handler->has_error()) {
					message = _T("Collection of perfkeys failed: ") + object_handler->get_error();
					return false;
				}
			}

			return true;
		}

		bool match(boost::shared_ptr<object_type> record) {
			object_handler->set_current_element(record);
			bool ret = ast_parser.evaluate(object_handler);
			if (object_handler->has_error()) {
				data->error->report_error(_T("Error: ") + object_handler->get_error());
			}
			return ret;
		}
		std::wstring get_name() {
			return _T("where");
		}
		std::wstring get_subject() { return filter_string; }
	};


	class nsc_error_handler : public where_filter::error_handler_interface {
		int error_count_;
		int warning_count_;
		std::wstring first_error_;
		std::wstring last_error_;
	public:
		nsc_error_handler() : error_count_(0), warning_count_(0) {}
		void report_error(std::wstring error) {
			if (error_count_==0)
				first_error_ = error;
			else
				last_error_ = error;
			error_count_++;
			NSC_LOG_ERROR(error);
		}
		void report_warning(std::wstring error) {
			NSC_LOG_MESSAGE(error);
		}
		void report_debug(std::wstring error) {
			NSC_DEBUG_MSG(error);
		}
		std::wstring get_error() {
			return strEx::itos(error_count_) + _T(" errors and ") + strEx::itos(warning_count_) + _T(" warnings where returned: ") + first_error_;
		}
		bool has_error() {
			return error_count_ > 0;
		}
	};

	template<typename record_type>
	struct simple_count_result : public result_counter_interface<record_type> {
		unsigned int count_;
		unsigned int match_;
		std::wstring message_;
		std::wstring syntax_;
		std::wstring date_syntax_;
		bool debug_;
		boost::shared_ptr<where_filter::error_handler_interface> error_;

		simple_count_result(boost::shared_ptr<argument_interface > argument) : count_(0), match_(0), syntax_(argument->syntax), date_syntax_(argument->date_syntax), debug_(argument->debug), error_(argument->error) {}
		void process(boost::shared_ptr<record_type> record, bool result) {
			count_++;
			if (result) {
// 				if (debug_)
// 					error_->report_debug(_T("==> Matched: ") + record->render(syntax_, DATE_FORMAT));
						
				strEx::append_list(message_, record->render(syntax_, date_syntax_));
				/*
				if (alias.length() < 16)
					strEx::append_list(alias, info.filename);
				else
					strEx::append_list(alias, std::wstring(_T("...")));
					*/
				match_++;
			} else {
// 				if (debug_)
// 					error_->report_debug(_T("==> NO Matched: ") + record->render(syntax_, DATE_FORMAT));
			}
		}
		unsigned int get_total_count() { return count_; }
		unsigned int get_match_count() { return match_; }
		std::wstring get_message() { return message_; }
		std::wstring render(std::wstring syntax, int code) {
			strEx::replace(syntax, _T("%list%"), message_);
			strEx::replace(syntax, _T("%matches%"), strEx::itos(count_));
			strEx::replace(syntax, _T("%total%"), strEx::itos(match_));
			strEx::replace(syntax, _T("%status%"), strEx::itos(code));
			return syntax;
		}
	};

	struct error_gatherer {
		typedef std::list<std::wstring> error_type;
		void error(std::wstring err) {
			errors.push_back(err);
		}
		bool has_error() {
			return !errors.empty();
		}
		std::wstring get_error() {
			std::wstring ret;
			BOOST_FOREACH(std::wstring s, errors) {
				if (!ret.empty()) ret += _T(", ");
				ret += s;
			}
			return ret;
		}
	private:
		error_type errors;
	};

}
