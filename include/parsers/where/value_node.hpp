#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <parsers/where/node.hpp>


namespace parsers {
	namespace where {


		template<class T>
		struct node_value_impl : public any_node {
			T value_;

			node_value_impl(T value, value_type type) : any_node(type), value_(value) {}
			node_value_impl(const node_value_impl<T> &other) : value_(other.value_) {}
			const node_value_impl<T>& operator=(const node_value_impl<T> &other) {
				value_ = other.value_;
				return *this;
			}
			virtual std::list<node_type> get_list_value(evaluation_context errors) const {
				return std::list<node_type>();
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
			string_value(const std::string &value) : node_value_impl<std::string>(value, type_string) {}
			long long get_int_value(evaluation_context context) const;
			std::string get_string_value(evaluation_context context) const;
			std::string to_string() const;
			value_type infer_type(object_converter, value_type) {
				return type_string;
			}
			value_type infer_type(object_converter) {
				return type_string;
			}
			bool find_performance_data(evaluation_context context, performance_collector &collector);
		};
		struct int_value : public node_value_impl<long long>, boost::enable_shared_from_this<int_value> {
			int_value(const long long &value) : node_value_impl<long long>(value, type_int) {}
			long long get_int_value(evaluation_context context) const;
			std::string get_string_value(evaluation_context context) const;
			std::string to_string() const;
			value_type infer_type(object_converter converter, value_type) {
				return type_int;
			}
			value_type infer_type(object_converter converter) {
				return type_int;
			}
			bool find_performance_data(evaluation_context context, performance_collector &collector);
		};
	}
}
