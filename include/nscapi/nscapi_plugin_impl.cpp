/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

nscapi::core_wrapper* nscapi::impl::simple_plugin::get_core() const {
	return plugin_singleton->get_core();
}

std::string nscapi::impl::simple_plugin::get_base_path() const {
	return get_core()->expand_path("${base-path}");
}