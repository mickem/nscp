#pragma once

#include <protobuf/plugin.pb.h>

#include <nscapi/nscapi_settings_object.hpp>

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>


namespace check_pdh {

	struct counter_config_object {

		nscapi::settings_objects::template_object tpl;
		bool debug;
		std::string collection_strategy;
		std::string counter;
		std::string instances;
		std::string buffer_size;
		std::string type;

		// Runtime items

		std::string to_string() const {
			std::stringstream ss;
			ss << tpl.to_string() << "{counter: " << counter << ", "  << collection_strategy << ", "  << type << "}";
			return ss.str();
		}
	};

	struct command_reader {
		typedef counter_config_object object_type;
		static void post_process_object(object_type&) {}
		static void init_default(object_type& object);
		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample);
		static void apply_parent(object_type &object, object_type &parent);
	};

	typedef nscapi::settings_objects::object_handler<counter_config_object, command_reader> counter_config_handler;


	struct filter_obj {
		std::string alias;
		std::string counter;
		unsigned long long value;

		filter_obj(std::string alias, std::string counter, unsigned long long value) : alias(alias), counter(counter), value(value) {}

		long long get_value() const {
			return value;
		}
		std::string get_counter() const {
			return counter;
		}
		std::string get_alias() const {
			return alias;
		}
	};

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};

	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

	struct check {
		counter_config_handler counters_;
		void check_pdh(pdh_thread &collector, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
		void add_counter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string path, std::string key, std::string query);
	};
}
