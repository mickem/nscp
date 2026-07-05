// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_thin_plugin_impl.hpp>

nscapi::core_wrapper* nscapi::impl::thin_plugin::get_core() const { return plugin_singleton->get_core(); }
/*
std::string nscapi::impl::thin_plugin::get_base_path() const {
        return get_core()->expand_path("${base-path}");
}*/