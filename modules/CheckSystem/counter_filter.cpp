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

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>

#include "counter_filter.hpp"

using namespace boost::assign;
using namespace parsers::where;

counter_filter::filter_obj_handler::filter_obj_handler() {
	registry_.add_string()
		("counter", boost::bind(&filter_obj::get_counter, _1), "The name of the file")
		;
}