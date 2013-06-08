#pragma once

#include <parsers/expression/expression.hpp>
#include <parsers/filter/where_filter_impl.hpp>

namespace modern_filter {

	template<class Tobject, class THandler>
	struct filter_text_renderer {
		typedef boost::function<std::string(Tobject*)> function_type;
		struct my_entry {
			parsers::simple_expression::entry origin;
			function_type fun;
			my_entry(const parsers::simple_expression::entry &origin) : origin(origin) {}
			my_entry(const my_entry &other) : origin(other.origin), fun(other.fun) {}
			const my_entry& operator= (const my_entry &other) {
				origin = other.origin;
				fun = other.fun;
				return *this;
			}
		};

		typedef std::vector<my_entry> entry_list;
		entry_list entries;

		bool parse(const std::string str, std::string &error) {
			parsers::simple_expression::result_type keys;
			parsers::simple_expression expr;
			if (!expr.parse(str, keys)) {
				error = "Failed to parse: " + str;
				return false;
			}
			THandler handler;
			BOOST_FOREACH(const parsers::simple_expression::entry &e, keys) {
				my_entry my_e(e);
				if (e.is_variable) {
					std::string tag = e.name;
					if (!handler.has_variable(tag)) {
						error = "Invalid variable: " + e.name;
						return false;
					}
					my_e.fun = handler.bind_simple_string(tag);
					if (!my_e.fun) {
						error = "Invalid variable: " + e.name;
						return false;
					}
				}
				entries.push_back(my_e);
			}
			return true;
		}
		std::string render(boost::shared_ptr<Tobject> obj) {
			std::string ret;
			BOOST_FOREACH(const my_entry &e, entries) {
				if (!e.origin.is_variable)
					ret += e.origin.name;
				else {
					ret += utf8::cvt<std::string>(e.fun(obj.get()));
				}
			}
			return ret;
		}
	};

	template<class Tsummary>
	struct text_renderer {
		typedef boost::function<std::string(Tsummary*)> function_type;
		struct my_entry {
			parsers::simple_expression::entry origin;
			function_type fun;
			my_entry(const parsers::simple_expression::entry &origin) : origin(origin) {}
			my_entry(const my_entry &other) : origin(other.origin), fun(other.fun) {}
			const my_entry& operator= (const my_entry &other) {
				origin = other.origin;
				fun = other.fun;
				return *this;
			}
		};

		typedef std::vector<my_entry> entry_list;
		entry_list entries;

		bool parse(const std::string str, std::string &error) {
			parsers::simple_expression::result_type keys;
			parsers::simple_expression expr;
			if (!expr.parse(str, keys)) {
				error = "Failed to parse: " + str;
				return false;
			}
			BOOST_FOREACH(const parsers::simple_expression::entry &e, keys) {
				my_entry my_e(e);
				if (e.is_variable) {
					my_e.fun = Tsummary::get_function(e.name);
					if (!my_e.fun) {
						error = "Invalid variable: " + e.name;
						return false;
					}
				}
				entries.push_back(my_e);
			}
			return true;
		}
		std::string render(Tsummary &data) {
			std::string ret;
			BOOST_FOREACH(const my_entry &e, entries) {
				if (!e.origin.is_variable)
					ret += e.origin.name;
				else {
					ret += e.fun(&data);
				}
			}
			return ret;
		}
	};


	template<class Tobject, class Thandler, class Tsummary>
	struct modern_filters {
		typedef boost::shared_ptr<where_filter::argument_interface> filter_argument_type;
		typedef where_filter::engine_impl<Tobject, Thandler, filter_argument_type> filter_engine_type;
		typedef boost::shared_ptr<where_filter::error_handler_interface> error_type;
		typedef boost::shared_ptr<filter_engine_type> filter_engine;
		typedef std::map<std::string,std::string> boundries_type;

		text_renderer<Tsummary> renderer_top;
		filter_text_renderer<Tobject, Thandler> renderer_detail;
		filter_engine engine_filter;
		filter_engine engine_warn;
		filter_engine engine_crit;
		filter_engine engine_ok;
		Tsummary summary;
		NSCAPI::nagiosReturn returnCode;
		bool has_matched;
		std::string message;
		bool debug;

		struct perf_entry {
			parsers::where::value_type type;
			std::string label;
			typedef boost::function<long long(Tobject*)> bound_int_type;
			bound_int_type collect_int;

			std::string crit_value;
			std::string warn_value;
		};
		struct perf_instance_data {
			perf_entry parent;
			std::string alias;
			long long number_value;
			std::string string_value;
		};

		typedef std::list<perf_instance_data> performance_instance_data_type;
		performance_instance_data_type performance_instance_data;

		typedef std::map<std::string,perf_entry> leaf_performance_entry_type;
		leaf_performance_entry_type leaf_performance_data;


		filter_engine create_engine(filter_argument_type arg, std::string filter) {
			arg->filter = filter;
			return filter_engine(new filter_engine_type(arg));
		}

		bool build_syntax(const std::string &top, const std::string &detail, std::string &error) {
			if (!renderer_top.parse(top, error)) {
				error = "Invalid top-syntax: " + error;
				return false;
			}
			if (!renderer_detail.parse(detail, error)) {
				error = "Invalid syntax: " + error;
				return false;
			}
			return true;
		}
		bool build_engines(const std::string &filter, const std::string &ok, const std::string &warn, const std::string &crit, std::string &errors) {
			error_type error(new where_filter::collect_error_handler());
			filter_argument_type fargs(new where_filter::argument_interface(error, "", ""));

			if (!filter.empty()) engine_filter = create_engine(fargs, filter);
			if (!ok.empty()) engine_ok = create_engine(fargs, ok);
			if (!warn.empty()) engine_warn = create_engine(fargs, warn);
			if (!crit.empty()) engine_crit = create_engine(fargs, crit);

			if (engine_filter) engine_filter->boot();
			if (engine_ok) engine_ok->boot();
			if (engine_warn) engine_warn->boot();
			if (engine_crit) engine_crit->boot();

			if (engine_warn) engine_warn->enabled_performance_collection();
			if (engine_crit) engine_crit->enabled_performance_collection();

			if (error->has_error()) {
				errors = utf8::cvt<std::string>(error->get_error());
				return false;
			}
			return true;
		}

		bool validate(std::string &error) {
			std::string msg;
			if (engine_filter && !engine_filter->validate(msg)) {
				error = msg;
				return false;
			}
			if (engine_warn && !engine_warn->validate(msg)) {
				error = msg;
				return false;
			}
			if (engine_warn) {
				BOOST_FOREACH(const boundries_type::value_type &v, engine_warn->fetch_performance_data()) {
					if (!summary.add_performance_data_metric(utf8::cvt<std::string>(v.first))) {
						register_leaf_performance_data(utf8::cvt<std::string>(v.first), utf8::cvt<std::string>(v.second), engine_warn);
					}
				}
			}
			if (engine_crit && !engine_crit->validate(msg)) {
				error = msg;
				return false;
			}
			if (engine_crit) {
				BOOST_FOREACH(const boundries_type::value_type &v, engine_crit->fetch_performance_data()) {
					if (!summary.add_performance_data_metric(utf8::cvt<std::string>(v.first))) {
						register_leaf_performance_data(utf8::cvt<std::string>(v.first), utf8::cvt<std::string>(v.second), engine_crit);
					}
				}
			}
			if (engine_ok && !engine_ok->validate(msg)) {
				error = msg;
				return false;
			}
			return true;
		}

		void register_leaf_performance_data(const std::string &tag, const std::string &value, filter_engine engine) {
			if (engine->object_handler->has_variable(tag)) {
				perf_entry entry;
				entry.type = engine->object_handler->get_type(tag);
				if (entry.type == parsers::where::type_int) {
					entry.collect_int = engine->object_handler->bind_simple_int(tag);
					if (!entry.collect_int)
						return;
					typename leaf_performance_entry_type::iterator it = leaf_performance_data.find(tag);
					if (it != leaf_performance_data.end()) {
						if (engine == engine_crit)
							it->second.crit_value = value;
						else
							it->second.warn_value = value;
					} else {
						entry.label = tag;
						if (engine == engine_crit)
							entry.crit_value = value;
						else
							entry.warn_value = value;
						leaf_performance_data[tag] = entry;
					}
				}
			}
		}

		void reset() {
			returnCode = NSCAPI::returnOK;
			has_matched = false;
			summary.reset();
		}
		boost::tuple<bool,bool> match(boost::shared_ptr<Tobject> record) {
			bool matched = false;
			bool done = false;
			// done should be set if we want to bail out after the first hit!
			// I.e. mode==first (mode==all)
			if (engine_filter && engine_filter->match(record)) {
				record->matched();
				std::string current = renderer_detail.render(record);
				store_perf(record, current);
				summary.matched(current);
				if (engine_crit && engine_crit->match(record)) {
					summary.matched_crit(current);
					nscapi::plugin_helper::escalteReturnCodeToCRIT(returnCode);
					matched = true;
				} else if (engine_warn && engine_warn->match(record)) {
					summary.matched_warn(current);
					nscapi::plugin_helper::escalteReturnCodeToWARN(returnCode);
					matched = true;
				} else if (engine_ok && engine_ok->match(record) || !engine_ok) {
					summary.matched_ok(current);
					message = renderer_top.render(summary);
					matched = true;
				} else if (debug) {
					NSC_DEBUG_MSG_STD("Crit/warn/ok did not match: " + current);
				}
				if (matched) {
					message = renderer_top.render(summary);
				}
			} else if (debug) {
				NSC_DEBUG_MSG_STD("Did not match: " + renderer_detail.render(record));
			}
			return boost::make_tuple(matched, done);
		}
		void store_perf(boost::shared_ptr<Tobject> record, const std::string &alias) {
			BOOST_FOREACH(const typename leaf_performance_entry_type::value_type &entry, leaf_performance_data) {
				if (entry.second.type == parsers::where::type_int) {
					long long value = entry.second.collect_int(record.get());
					append_record(alias, entry.second, value);
				}
			}
		}
		void append_record(const std::string &alias, const perf_entry &parent, long long value) {
			perf_instance_data data;
			data.alias = alias;
			data.parent = parent;
			data.number_value = value;
			performance_instance_data.push_back(data);
		}
		void append_record(const std::string &alias, const perf_entry &parent, std::string value) {
			perf_instance_data data;
			data.alias = alias;
			data.parent = parent;
			data.string_value = value;
			performance_instance_data.push_back(data);
		}

		void fetch_perf() {
			summary.fetch_perf();
			BOOST_FOREACH(const typename performance_instance_data_type::value_type &entry, performance_instance_data) {
				std::cout << " * " << entry.parent.label << "." << entry.alias << " = " << entry.number_value << "; " << entry.parent.warn_value << ";" << entry.parent.crit_value << std::endl;
			}
		}

	};


	template <class Timpl>
	struct generic_summary {
		long long count_match;
		long long count_ok;
		long long count_warn;
		long long count_crit;
		long long count_problem;
		std::string list_match;
		std::string list_ok;
		std::string list_crit;
		std::string list_warn;
		std::string list_problem;

		typedef std::map<std::string,boost::function<std::string()> > metrics_type;
		metrics_type metrics;

		generic_summary() : count_match(0), count_ok(0), count_warn(0), count_crit(0), count_problem(0) {}

		void reset() {
			count_match = count_ok = count_warn = count_crit = count_problem = 0;
			list_match = list_ok = list_warn = list_crit = "";
		}
		void matched(std::string &line) {
			format::append_list(list_match, line);
			count_match++;
		}
		void matched_ok(std::string &line) {
			format::append_list(list_ok, line);
			count_ok++;
		}
		void matched_warn(std::string &line) {
			format::append_list(list_warn, line);
			format::append_list(list_problem, line);
			count_warn++;
		}
		void matched_crit(std::string &line) {
			format::append_list(list_crit, line);
			format::append_list(list_problem, line);
			count_crit++;
		}
		std::string get_list_match() {
			return list_match;
		}
		std::string get_list_ok() {
			return list_ok;
		}
		std::string get_list_warn() {
			return list_warn;
		}
		std::string get_list_crit() {
			return list_crit;
		}
		std::string get_list_problem() {
			return list_problem;
		}
		std::string get_count_match() {
			return strEx::s::xtos(count_match);
		}
		std::string get_count_ok() {
			return strEx::s::xtos(count_ok);
		}
		std::string get_count_warn() {
			return strEx::s::xtos(count_warn);
		}
		std::string get_count_crit() {
			return strEx::s::xtos(count_crit);
		}
		std::string get_count_problem() {
			return strEx::s::xtos(count_problem);
		}
		static boost::function<std::string(Timpl*)> get_function(std::string key) {
			if (key == "count" || key == "match_count")
				return &Timpl::get_count_match;
			if (key == "ok_count")
				return &Timpl::get_count_ok;
			if (key == "warn_count" || key == "warning_count")
				return &Timpl::get_count_warn;
			if (key == "crit_count" || key == "critical_count")
				return &Timpl::get_count_crit;
			if (key == "problem_count")
				return &Timpl::get_count_problem;
			if (key == "list" || key == "match_list")
				return &Timpl::get_list_match;
			if (key == "ok_list")
				return &Timpl::get_list_ok;
			if (key == "warn_list" || key == "warning_list")
				return &Timpl::get_list_warn;
			if (key == "crit_list" || key == "critical_list")
				return &Timpl::get_list_crit;
			if (key == "problem_list")
				return &Timpl::get_list_problem;
			return boost::function<std::string(Timpl*)>();
		}
		void fetch_perf() {
			BOOST_FOREACH(const metrics_type::value_type &m, metrics) {
				std::cout << " * " << m.first << " = " << m.second() << std::endl;
			}
		}

	};

}
