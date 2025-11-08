/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <NSCAPI.h>
#include <nscapi/nscapi_plugin_wrapper.hpp>

#include <win/windows.hpp>

#include <managed/core_impl.hpp>
#include <managed/convert.hpp>

using namespace System;
using namespace System::IO;
using namespace System::Reflection;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

//using namespace PB::Commands;

nscapi::core_wrapper* CoreImpl::get_core() {
	return core;
}

CoreImpl::CoreImpl(nscapi::core_wrapper* core) 
	: core(core) 
{
}

NSCP::Core::Result^ CoreImpl::query(protobuf_data^ request) {
	std::string response;
	return gcnew NSCP::Core::Result(
		get_core()->query(to_nstring(request), response), 
		to_pbd(response)
	);
}
NSCP::Core::Result^ CoreImpl::exec(String^ target, protobuf_data^ request) {
	std::string response;
	return gcnew NSCP::Core::Result(
		get_core()->exec_command(to_nstring(target), to_nstring(request), response),
		to_pbd(response)
	);
}
NSCP::Core::Result^ CoreImpl::submit(String^ channel, protobuf_data^ request) {
	std::string response;
	return gcnew NSCP::Core::Result(
		get_core()->submit_message(to_nstring(channel), to_nstring(request), response),
		to_pbd(response)
	);
}
bool CoreImpl::reload(String^ module) {
	return get_core()->reload(to_nstring(module)) == NSCAPI::api_return_codes::isSuccess;
}
NSCP::Core::Result^ CoreImpl::settings(protobuf_data^ request) {
	std::string response;
	return gcnew NSCP::Core::Result(
		get_core()->settings_query(to_nstring(request), response),
		to_pbd(response)
	);
}
NSCP::Core::Result^ CoreImpl::registry(protobuf_data^ request) {
	std::string response;
	return gcnew NSCP::Core::Result(
		get_core()->registry_query(to_nstring(request), response),
		to_pbd(response)
	);
}
void CoreImpl::log(protobuf_data^ request) {
	get_core()->log(to_nstring(request));
}
