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

// This file has been split into multiple smaller files for better organization.
// Include this file to get all protobuf function tests, or include
// individual files for specific functionality:
//
// - nscapi_protobuf_functions_status_test.cpp   : Nagios status conversion tests
// - nscapi_protobuf_functions_response_test.cpp : Response helper tests (set_response_good/bad)
// - nscapi_protobuf_functions_query_test.cpp    : Query request/response tests
// - nscapi_protobuf_functions_submit_test.cpp   : Submit request/response tests
// - nscapi_protobuf_functions_exec_test.cpp     : Execute request/response tests
// - nscapi_protobuf_functions_convert_test.cpp  : Message conversion tests (make_*_from_*)
// - nscapi_protobuf_functions_perfdata_test.cpp : Performance data parsing/building tests
// - nscapi_protobuf_functions_copy_test.cpp     : Response copy tests

#include "nscapi_protobuf_functions_convert_test.cpp"
#include "nscapi_protobuf_functions_copy_test.cpp"
#include "nscapi_protobuf_functions_exec_test.cpp"
#include "nscapi_protobuf_functions_perfdata_test.cpp"
#include "nscapi_protobuf_functions_query_test.cpp"
#include "nscapi_protobuf_functions_response_test.cpp"
#include "nscapi_protobuf_functions_status_test.cpp"
#include "nscapi_protobuf_functions_submit_test.cpp"
