#pragma once

#include <string>
#include <list>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>

namespace parsers {
	namespace where {

		struct filter_exception : public std::exception {
			std::string what_;
			filter_exception(std::string what) : what_(what) {}
			~filter_exception() throw () {}
			const char* what() const throw () {
				return what_.c_str();
			}
			std::string reason() const throw();
		};

		enum operators {
			op_eq, op_le, op_lt, op_gt, op_ge, op_ne, op_in, op_nin, op_or, op_and, op_inv, op_not, op_like, op_not_like, op_binand, op_binor, op_regexp, op_not_regexp
		};

		enum value_type {
			type_invalid = 99, type_tbd = 66, type_multi = 88,
			type_int = 1, type_bool = 2, 
			type_string = 10, 
			type_date = 20, 
			type_size = 30, 
			type_custom_int = 1024,
			type_custom_int_1 = 1024+1,
			type_custom_int_2 = 1024+2,
			type_custom_int_3 = 1024+3,
			type_custom_int_4 = 1024+4,
			type_custom_int_5 = 1024+5,
			type_custom_int_6 = 1024+6,
			type_custom_int_7 = 1024+7,
			type_custom_int_8 = 1024+8,
			type_custom_int_9 = 1024+9,
			type_custom_int_10 = 1024+10,
			type_custom_int_end = 1024+100,
			type_custom_string = 2048,
			type_custom_string_1 = 2048+1,
			type_custom_string_2 = 2048+2,
			type_custom_string_3 = 2048+3,
			type_custom_string_4 = 2048+4,
			type_custom_string_5 = 2048+5,
			type_custom_string_6 = 2048+6,
			type_custom_string_7 = 2048+7,
			type_custom_string_8 = 2048+8,
			type_custom_string_9 = 2048+9,
			type_custom_string_end = 2048+100,
			type_custom = 4096,
			type_custom_1 = 4096+1,
			type_custom_2 = 4096+2,
			type_custom_3 = 4096+3,
			type_custom_4 = 4096+4
		};


		struct evaluation_context_interface {
			virtual bool has_error() const = 0;
			virtual std::string get_error() const = 0;
			virtual void error(const std::string) = 0;
//			virtual void* get_context() = 0;
			virtual void clear_errors() = 0;
		};
		typedef boost::shared_ptr<evaluation_context_interface> evaluation_context;

		struct any_node;
		typedef boost::shared_ptr<any_node> node_type;

		struct performance_data {
			template<class T>
			struct perf_value {
				T value;
				boost::optional<T> crit;
				boost::optional<T> warn;
				boost::optional<T> minimum;
				boost::optional<T> maximum;
			};
			std::string alias;
			std::string unit;
			boost::optional<perf_value<long long> > value_int;
			boost::optional<perf_value<double> > value_double;
			boost::optional<perf_value<std::string> > value_string;
		};
		typedef std::list<performance_data> perf_list_type;

		struct perf_writer_interface {
			virtual void write(const performance_data &value) = 0;
		};


		struct performance_node {
			enum performance_node_type {
				perf_type_upper,
				perf_type_lower,
				perf_type_neutral
			};
			std::string variable;
			node_type value;
			performance_node_type perf_node_type;
			performance_node() {}

		};
		struct performance_collector {
			typedef std::map<std::string,performance_node> boundries_type;
		private:
			boundries_type boundries;
			node_type candidate_value_;
			std::string candidate_variable_;
		public:
			void add_perf(const performance_collector &other);
			bool has_candidates() const;
			void add_candidates(const performance_collector &other);
			void set_candidate_variable(std::string name);
			void set_candidate_value(node_type value);
			void clear_candidates();
			bool add_bounds_candidates(const performance_collector &left, const performance_collector &right);
			bool add_neutral_candidates(const performance_collector &left, const performance_collector &right);
			bool has_candidate_value() const;
			bool has_candidate_variable() const;
			std::string get_variable() const;
			node_type get_value() const;
			boundries_type get_candidates();
		};


		struct binary_operator_impl {
			virtual node_type evaluate(evaluation_context context, const node_type left, const node_type right) const = 0;
		};
		struct binary_function_impl {
			virtual node_type evaluate(value_type type, evaluation_context context, const node_type subject) const = 0;
		};
		struct unary_operator_impl {
			virtual node_type evaluate(evaluation_context context, const node_type subject) const = 0;
		};


		struct object_converter_interface : public evaluation_context_interface {
			virtual bool can_convert(value_type from, value_type to) = 0;
			virtual bool can_convert(std::string name, boost::shared_ptr<any_node> subject, value_type to) = 0;
			virtual boost::shared_ptr<binary_function_impl> create_converter(std::string name, boost::shared_ptr<any_node> subject, value_type to) = 0;
		};
		typedef boost::shared_ptr<object_converter_interface> object_converter;
		struct object_factory_interface : public object_converter_interface {
			typedef std::size_t index_type;

			virtual bool has_variable(const std::string &name) = 0;
			virtual node_type create_variable(const std::string &name, bool human_readable) = 0;

			virtual bool has_function(const std::string &name) = 0;
			virtual node_type create_function(const std::string &name, node_type subject) = 0;

			virtual std::string get_performance_config_key(const std::string prefix, const std::string object, const std::string suffix, const std::string key, const std::string unit) const = 0;
		};
		typedef boost::shared_ptr<object_factory_interface> object_factory;

		struct any_node {
		private:
			value_type type;
		public:
			any_node() : type(type_tbd) {}
			any_node(const value_type type) : type(type) {}
			any_node(const any_node& other) : type(other.type) {}
			const any_node& operator=(const any_node& other) {
				type = other.type;
				return *this;
			}

			virtual value_type get_type() const { return type; }
			virtual void set_type(value_type newtype) { type = newtype; };
			bool is_int() const;
			bool is_string() const;
			virtual value_type infer_type(object_converter converter) = 0;
			virtual value_type infer_type(object_converter converter, value_type suggestion) = 0;

			virtual std::string to_string() const = 0;

			virtual long long get_int_value(evaluation_context errors) const = 0;
			virtual std::string get_string_value(evaluation_context errors) const = 0;
			virtual std::list<boost::shared_ptr<any_node> > get_list_value(evaluation_context errors) const = 0;

			virtual bool can_evaluate() const = 0;
			virtual boost::shared_ptr<any_node> evaluate(evaluation_context errors) const = 0;
			virtual bool static_evaluate(evaluation_context errors) const = 0;
			virtual bool require_object(evaluation_context errors) const = 0;
			virtual bool bind(object_converter errors) = 0;

			// Performance data functions
			virtual bool find_performance_data(evaluation_context context, performance_collector &collector) = 0;
			virtual perf_list_type get_performance_data(object_factory context, std::string alias, node_type warn, node_type crit, node_type minimum, node_type maximum)  {
				perf_list_type ret;
				return ret;
			}
		};

		struct list_node_interface : public any_node {
			virtual void push_back(node_type value) = 0;
		};
		typedef boost::shared_ptr<list_node_interface> list_node_type;



		struct factory {
			static node_type create_list(const std::list<std::string> &other);
			static list_node_type create_list();
			static node_type create_list(const std::list<long long> &other);
			static node_type create_bin_op(const operators &op, node_type lhs, node_type rhs);
			static node_type create_un_op(const operators op, node_type node);
			static node_type create_conversion(node_type node);
			static node_type create_fun(object_factory factory, const std::string op, node_type node);
			static node_type create_string(const std::string &value);
			static node_type create_int(const long long &value);
			static node_type create_neg_int(const long long &value);
			static node_type create_variable(object_factory factory, const std::string &name);
			static node_type create_false();
			static node_type create_true();
		};
	}
}
