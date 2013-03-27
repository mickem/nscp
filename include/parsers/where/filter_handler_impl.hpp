#pragma once

namespace parsers {
	namespace where {

		template<class T>
		struct filter_handler_impl : public filter_handler_interface {
			typedef boost::function<std::string(T*)> bound_string_type;
			typedef boost::function<long long(T*)> bound_int_type;
			typedef boost::function<expression_ast(T*,value_type,filter_handler handler, const expression_ast *)> bound_function_type;
			typedef boost::shared_ptr<T> object_type;

			virtual bool has_variable(std::string) = 0;
			virtual value_type get_type(std::string) = 0;
			virtual bool can_convert(value_type from, value_type to) = 0;

			std::size_t bind_string(std::string key) {
				string_functions.push_back(bind_simple_string(key));
				return string_functions.size();
			}
			virtual std::size_t bind_int(std::string key) {
				int_functions.push_back(bind_simple_int(key));
				return int_functions.size();
			}
			virtual std::size_t bind_function(value_type to, std::string name, expression_ast *subject) {
				functions.push_back(bind_simple_function(to, name, subject));
				return functions.size();
			}
			long long execute_int(filter_handler_interface::index_type id) {
				return int_functions[id-1](current_element.get());
			}
			std::string execute_string(filter_handler_interface::index_type id) {
				return string_functions[id-1](current_element.get());
			}
			expression_ast execute_function(filter_handler_interface::index_type id, value_type type, filter_handler handler, const expression_ast *arguments) {
				return functions[id-1](current_element.get(), type, handler, arguments);
			}

			virtual bound_string_type bind_simple_string(std::string key) = 0;
			virtual bound_int_type bind_simple_int(std::string key) = 0;
			virtual bound_function_type bind_simple_function(value_type to, std::string name, expression_ast *subject) = 0;

			void error(std::string err) { errors.push_back(err); }
			bool has_error() { return !errors.empty(); }
			std::string get_error() { return format::join(errors, std::string(", ")); }
			void set_current_element(boost::shared_ptr<T> current_element_) {
				current_element = current_element_;
			}
		private:
			std::list<std::string> errors;
			std::vector<bound_string_type> string_functions;
			std::vector<bound_int_type> int_functions;
			std::vector<bound_function_type> functions;
			boost::shared_ptr<T> current_element;
		};

	}
}