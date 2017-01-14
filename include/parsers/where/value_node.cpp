/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
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