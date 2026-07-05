// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "counter_filter.hpp"

#include <boost/assign.hpp>

using namespace boost::assign;
using namespace parsers::where;

counter_filter::filter_obj_handler::filter_obj_handler() { registry_.add_string_var("counter", &filter_obj::get_counter, "The name of the file"); }