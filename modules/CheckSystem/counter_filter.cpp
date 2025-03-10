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

#include <boost/bind/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>

#include "counter_filter.hpp"

using namespace boost::assign;
using namespace parsers::where;

counter_filter::filter_obj_handler::filter_obj_handler() {
  registry_.add_string()("counter", boost::bind(&filter_obj::get_counter, boost::placeholders::_1), "The name of the file");
}