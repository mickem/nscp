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

#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <parsers/where/node.hpp>
#include <parsers/where/helpers.hpp>

namespace parsers {
	namespace where {
		template<class T>
		struct node_value_impl : public any_node {
			T value_;
			bool is_unsure_;

			node_value_impl(T value, value_type type, bool is_unsure) : any_node(type), value_(value), is_unsure_(is_unsure) {}
			node_value_impl(const node_value_impl<T> &other) : value_(other.value_), is_unsure_(other.is_unsure_) {}
			const node_value_impl<T>& operator=(const node_value_impl<T> &other) {
				value_ = other.value_;
				is_unsure_ = other.is_unsure_;
				return *this;
			}
			virtual std::list<node_type> get_list_value(evaluation_context errors) const {
				std::list<node_type> ret;
				ret.push_back(factory::create_ios(value_));
				return ret;
			}
			virtual bool can_evaluate() const {
				return false;
			}
			virtual boost::shared_ptr<any_node> evaluate(evaluation_context context) const {
				return factory::create_false();
			}
			virtual bool bind(object_converter context) {
				return true;
			}
			virtual bool static_evaluate(evaluation_context context) const {
				return true;
			}
			virtual bool require_object(evaluation_context context) const {
				return false;
			}
		};

		struct string_value : public node_value_impl<std::string>, boost::enable_shared_from_this<string_value> {
			string_value(const std::string &value, bool is_unsure = false) : node_value_impl<std::string>(value, type_string, is_unsure) {}
			value_container get_value(evaluation_context context, value_type type) const;
			virtual std::string to_string() const;
			virtual std::string to_string(evaluation_context errors) const;
			value_type infer_type(object_converter, value_type) {
				return type_string;
			}
			value_type infer_type(object_converter) {
				return type_string;
			}
			bool find_performance_data(evaluation_context context, performance_collector &collector);
		};
		struct int_value : public node_value_impl<long long>, boost::enable_shared_from_this<int_value> {
			int_value(const long long &value, bool is_unsure = false) : node_value_impl<long long>(value, type_int, is_unsure) {}
			value_container get_value(evaluation_context context, value_type type) const;
			virtual std::string to_string() const;
			virtual std::string to_string(evaluation_context errors) const;
			value_type infer_type(object_converter converter, value_type vt) {
				if (helpers::type_is_int(vt))
					return type_int;
				if (helpers::type_is_float(vt)) {
					set_type(vt);
					return vt;
				}
				return type_int;
			}
			value_type infer_type(object_converter converter) {
				return type_int;
			}
			bool find_performance_data(evaluation_context context, performance_collector &collector);
		};
		struct float_value : public node_value_impl<double>, boost::enable_shared_from_this<float_value> {
			float_value(const double &value, bool is_unsure = false) : node_value_impl<double>(value, type_float, is_unsure) {}
			value_container get_value(evaluation_context context, value_type type) const;
			virtual std::string to_string() const;
			virtual std::string to_string(evaluation_context errors) const;
			value_type infer_type(object_converter converter, value_type vt) {
				if (helpers::type_is_float(vt)) {
					return type_float;
				}
				if (helpers::type_is_int(vt)) {
					set_type(vt);
					return vt;
				}
				return type_float;
			}
			value_type infer_type(object_converter converter) {
				return type_float;
			}
			bool find_performance_data(evaluation_context context, performance_collector &collector);
		};
	}
}