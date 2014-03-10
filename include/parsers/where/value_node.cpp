#include <strEx.h>

#include <parsers/where/value_node.hpp>

namespace parsers {
	namespace where {

		long long string_value::get_int_value(evaluation_context errors) const {
			try {
				return strEx::s::stox<long long>(value_);
			} catch (const std::exception &) {
				errors->error("Failed to convert string to number: " + value_);
				return 0;
			}
		}
		std::string string_value::get_string_value(evaluation_context errors) const {
			return value_;
		}
		std::string string_value::to_string() const {
			return "(s){" + value_ + "}";
		}
		bool string_value::find_performance_data(evaluation_context context, performance_collector &collector) {
			collector.set_candidate_value(shared_from_this());
			return false;
		}
		long long int_value::get_int_value(evaluation_context errors) const {
			return value_;
		}
		std::string int_value::get_string_value(evaluation_context errors) const {
			return strEx::s::xtos(value_);
		}
		std::string int_value::to_string() const {
			return "(i){" + strEx::s::xtos(value_) + "}";
		}
		bool int_value::find_performance_data(evaluation_context context, performance_collector &collector) {
			collector.set_candidate_value(shared_from_this());
			return false;
		}
	}
}

