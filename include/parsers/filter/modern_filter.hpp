#pragma once

#include <parsers/expression/expression.hpp>

namespace modern_filter {

	template<class Tobject, class THandler>
	struct filter_text_renderer {
		typedef boost::function<std::wstring(Tobject*)> function_type;
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
					std::wstring tag = utf8::cvt<std::wstring>(e.name);
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

		text_renderer<Tsummary> renderer_top;
		filter_text_renderer<Tobject, Thandler> renderer_detail;
		filter_engine engine_filter;
		filter_engine engine_warn;
		filter_engine engine_crit;
		filter_engine engine_ok;
		Tsummary summary;
		NSCAPI::nagiosReturn returnCode;
		std::string message;
		bool debug;

		filter_engine create_engine(filter_argument_type arg, std::string filter) {
			arg->filter = utf8::cvt<std::wstring>(filter);
			return filter_engine(new filter_engine_type(arg));
		}

		void reset() {
			summary.reset();
		}

		bool build_syntax(const std::string &top, const std::string &detail, std::string &error) {
			if (!renderer_top.parse(top, error)) {
				return false;
			}
			if (!renderer_detail.parse(detail, error)) {
				return false;
			}
			return true;
		}
		void build_engines(const std::string &filter, const std::string &ok, const std::string &warn, const std::string &crit) {
			error_type error(new where_filter::nsc_error_handler());
			filter_argument_type fargs(new where_filter::argument_interface(error, _T(""), _T("")));

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

		}

		bool validate(std::string &error) {
			std::wstring msg;
			if (engine_filter && !engine_filter->validate(msg)) {
				error = utf8::cvt<std::string>(msg);
				return false;
			}
			if (engine_warn && !engine_warn->validate(msg)) {
				error = utf8::cvt<std::string>(msg);
				return false;
			}
			if (engine_crit && !engine_crit->validate(msg)) {
				error = utf8::cvt<std::string>(msg);
				return false;
			}
			if (engine_ok && !engine_ok->validate(msg)) {
				error = utf8::cvt<std::string>(msg);
				return false;
			}
			return true;
		}

		void start_match() {
			returnCode = NSCAPI::returnOK;
		}
		boost::tuple<bool,bool> match(boost::shared_ptr<Tobject> record) {
			bool matched = false;
			bool done = false;
			if (engine_filter && engine_filter->match(record)) {
				record->matched();
				std::string current = renderer_detail.render(record);
				summary.matched(current);
				if (engine_crit && engine_crit->match(record)) {
					summary.matched_crit(current);
					nscapi::plugin_helper::escalteReturnCodeToCRIT(returnCode);
					matched = done = true;
				} else if (engine_warn && engine_warn->match(record)) {
					summary.matched_warn(current);
					nscapi::plugin_helper::escalteReturnCodeToWARN(returnCode);
					matched = done = true;
				} else if (engine_ok && engine_ok->match(record) || !engine_ok) {
					summary.matched_ok(current);
					message = renderer_top.render(summary);
					matched = true;
				} else if (debug) {
					NSC_DEBUG_MSG_STD(_T("Crit/warn/ok did not match: ") + utf8::cvt<std::wstring>(current));
				}
				if (matched) {
					summary.message += current;
					message = renderer_top.render(summary);
				}
			} else if (debug) {
				NSC_DEBUG_MSG_STD(_T("Did not match: ") + utf8::cvt<std::wstring>(renderer_detail.render(record)));
			}
			return boost::make_tuple(matched, done);
		}
	};
}