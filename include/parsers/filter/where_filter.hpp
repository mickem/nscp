#pragma once

namespace where_filter {

	template<typename record_type>
	struct result_counter_interface {
		virtual void process(boost::shared_ptr<record_type> record, bool result) = 0;
		virtual unsigned int get_total_count() = 0;
		virtual unsigned int get_match_count() = 0;
		virtual std::string get_message() = 0;
		virtual std::string render(std::string syntax, int code) = 0;
	};

	class error_handler_interface {
	public:
		virtual void report_error(std::string error) = 0;
		virtual void report_warning(std::string error) = 0;
		virtual void report_debug(std::string error) = 0;

		virtual bool has_error() = 0;
		virtual std::string get_error() = 0;
	};

	struct argument_interface {
		typedef boost::shared_ptr<error_handler_interface> error_type;
		//typedef boost::shared_ptr<result_counter_interface<record_type> > result_type;
		bool debug;
		std::string syntax;
		std::string date_syntax;
		std::string filter;


		argument_interface(error_type error, std::string syntax, std::string date_syntax, bool debug = false) 
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
		virtual bool validate(std::string &message) = 0;
		virtual bool match(boost::shared_ptr<record_type> record) = 0;
		virtual std::string get_name() = 0;
		virtual std::string get_subject() = 0;
	};
}