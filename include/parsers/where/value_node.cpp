#include <strEx.h>

#include <parsers/where/value_node.hpp>

namespace parsers {
	namespace where {
		value_container string_value::get_value(evaluation_context errors, value_type type) const {
			if (type == type_float) {
				try {
					return value_container::create_float(strEx::s::stox<double>(value_), is_unsure_);
				} catch (const std::exception &) {
					errors->error("Failed to convert string to number: " + value_);
					return value_container::create_nil();
				}
			}
			if (type == type_int) {
				try {
					return value_container::create_int(strEx::s::stox<long long>(value_), is_unsure_);
				} catch (const std::exception &) {
					errors->error("Failed to convert string to number: " + value_);
					return value_container::create_nil();
				}
			}
			if (type == type_string) {
				return value_container::create_string(value_, is_unsure_);
			}
			errors->error("Failed to convert string to ?: " + value_);
			return value_container::create_nil();
		}
		std::string string_value::to_string() const {
			return "(s){" + value_ + "}";
		}
		bool string_value::find_performance_data(evaluation_context context, performance_collector &collector) {
			collector.set_candidate_value(shared_from_this());
			return false;
		}
		value_container int_value::get_value(evaluation_context errors, value_type type) const {
			if (type == type_float) {
				return value_container::create_float(value_, is_unsure_);
			}
			if (type == type_int) {
				return value_container::create_int(value_, is_unsure_);
			}
			if (type == type_string) {
				return value_container::create_string(strEx::s::xtos(value_), is_unsure_);
			}
			errors->error("Failed to convert int to ?: " + value_);
			return value_container::create_nil();
		}
		std::string int_value::to_string() const {
			return "(i){" + strEx::s::xtos(value_) + "}";
		}
		bool int_value::find_performance_data(evaluation_context context, performance_collector &collector) {
			collector.set_candidate_value(shared_from_this());
			return false;
		}
		value_container float_value::get_value(evaluation_context errors, value_type type) const {
			if (type == type_float) {
				return value_container::create_float(value_, is_unsure_);
			}
			if (type == type_int) {
				return value_container::create_int(value_, is_unsure_);
			}
			errors->error("Failed to convert string to ?: " + strEx::s::xtos(value_));
			return value_container::create_nil();
		}
		std::string float_value::to_string() const {
			return "(f){" + strEx::s::xtos(value_) + "}";
		}
		bool float_value::find_performance_data(evaluation_context context, performance_collector &collector) {
			collector.set_candidate_value(shared_from_this());
			return false;
		}
	}
}