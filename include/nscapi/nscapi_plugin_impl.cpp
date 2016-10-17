#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

nscapi::core_wrapper* nscapi::impl::simple_plugin::get_core() const {
	return plugin_singleton->get_core();
}

std::string nscapi::impl::simple_plugin::get_base_path() const {
	return get_core()->expand_path("${base-path}");
}