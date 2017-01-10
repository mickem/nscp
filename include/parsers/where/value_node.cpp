/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <str/xtos.hpp>

#include <parsers/where/value_node.hpp>

namespace parsers {
	namespace where {
		value_container string_value::get_value(evaluation_context errors, value_type type) const {
			if (type == type_float) {
				try {
					return value_container::create_float(str::stox<double>(value_), is_unsure_);
				} catch (const std::exception &) {
					errors->error("Failed to convert string to number: " + value_);
					return value_container::create_nil();
				}
			}
			if (type == type_int) {
				try {
					return value_container::create_int(str::stox<long long>(value_), is_unsure_);
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
		std::string string_value::to_string(evaluation_context context) const {
			return value_;
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
				return value_container::create_string(str::xtos(value_), is_unsure_);
			}
			errors->error("Failed to convert int to ?: " + value_);
			return value_container::create_nil();
		}
		std::string int_value::to_string() const {
			return "(i){" + str::xtos(value_) + "}";
		}
		std::string int_value::to_string(evaluation_context context) const {
			return str::xtos(value_);
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
			errors->error("Failed to convert string to ?: " + str::xtos(value_));
			return value_container::create_nil();
		}
		std::string float_value::to_string() const {
			return "(f){" + str::xtos(value_) + "}";
		}
		std::string float_value::to_string(evaluation_context context) const {
			return str::xtos(value_);
		}
		bool float_value::find_performance_data(evaluation_context context, performance_collector &collector) {
			collector.set_candidate_value(shared_from_this());
			return false;
		}
	}
}