#pragma once

#include <utils.h>
#include <strEx.h>

#include <socket/client.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <boost/make_shared.hpp>

namespace graphite_handler {
	namespace sh = nscapi::settings_helper;

	struct graphite_target_object : public nscapi::targets::target_object {
		typedef nscapi::targets::target_object parent;

		graphite_target_object(std::string alias, std::string path) : parent(alias, path) {
			set_property_int("timeout", 30);
			set_property_string("path", "/nsclient++");
		}
		graphite_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);

			nscapi::settings_helper::settings_registry settings(proxy);

			nscapi::settings_helper::path_extension root_path = settings.path(this->path);
			if (is_sample)
				root_path.set_sample();

			root_path.add_key()

				("path", sh::string_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "path", _1), "system.${hostname}.${check_alias}.${perf_alias}"),
					"PATH FOR VALUES", "Path mapping for metrics")

				;
		}
	};

	struct options_reader_impl : public client::options_reader_interface {
		virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
			return boost::make_shared<graphite_target_object>(alias, path);
		}
		virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
			return boost::make_shared<graphite_target_object>(parent, alias, path);
		}

		void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
			desc.add_options()
				("path", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "path", _1)),
					"")

				;
		}
	};
}