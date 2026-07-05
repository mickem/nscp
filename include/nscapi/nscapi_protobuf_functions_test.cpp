// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
