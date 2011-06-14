#pragma once

//#include <parsers/where.hpp>

namespace where_filter {

	template<typename record_type>
	struct result_counter_interface {
		virtual void process(record_type &record, bool result) = 0;
		virtual unsigned int get_total_count() = 0;
		virtual unsigned int get_match_count() = 0;
		virtual std::wstring get_message() = 0;
		virtual std::wstring render(std::wstring syntax, int code) = 0;
	};

	class error_handler_interface {
	public:
		virtual void report_error(std::wstring error) = 0;
		virtual void report_warning(std::wstring error) = 0;
		virtual void report_debug(std::wstring error) = 0;

		virtual bool has_error() = 0;
		virtual std::wstring get_error() = 0;
	};

	template<typename record_type>
	struct argument_interface {
		typedef boost::shared_ptr<error_handler_interface> error_type;
		//typedef boost::shared_ptr<result_counter_interface<record_type> > result_type;
		bool debug;
		std::wstring syntax;
		std::wstring date_syntax;
		std::wstring filter;


		argument_interface(error_type error, std::wstring syntax, std::wstring date_syntax, bool debug = false) 
			: debug(debug)
			, syntax(syntax) 
			, date_syntax(date_syntax)
			, error(error)
		{}

		error_type error;
		//result_type result;

// 		void set_result(result_type result_) {
// 			result = result_;
// 		}
		void set_error(error_type error_) {
			error = error_;
		}

	};

	template<typename record_type>
	struct engine_interface {
		virtual bool boot() = 0;
		virtual bool validate(std::wstring &message) = 0;
		virtual bool match(record_type &record) = 0;
		virtual std::wstring get_name() = 0;
		virtual std::wstring get_subject() = 0;
	};


}