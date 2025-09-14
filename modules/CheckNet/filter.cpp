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

#include "filter.hpp"

#include <boost/assign.hpp>
#include <boost/make_shared.hpp>

using namespace boost::assign;
using namespace parsers::where;

//////////////////////////////////////////////////////////////////////////

parsers::where::node_type get_percentage(boost::shared_ptr<ping_filter::filter_obj> object, parsers::where::evaluation_context context,
                                         parsers::where::node_type subject) {
  parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
  double number = value.get<1>();
  std::string unit = value.get<2>();

  if (unit != "%") context->error("Invalid unit: " + unit);
  return parsers::where::factory::create_int(number);
}

ping_filter::filter_obj_handler::filter_obj_handler() {
  static constexpr parsers::where::value_type type_custom_pct = parsers::where::type_custom_int_1;

  registry_.add_string("host", &filter_obj::get_host, "The host name or ip address (as given on command line)")
      .add_string("ip", &filter_obj::get_ip, "The ip address name");
  registry_.add_int_context("loss", type_custom_pct, &filter_obj::get_loss, "Packet loss")
      .add_int_x("time", type_int, &filter_obj::get_time, "Round trip time in ms")
      .add_int_x("sent", type_int, &filter_obj::get_sent, "Number of packets sent to the host")
      .add_int_x("recv", type_int, &filter_obj::get_recv, "Number of packets received from the host")
      .add_int_x("timeout", type_int, &filter_obj::get_timeout, "Number of packets which timed out from the host");
  registry_.add_converter()(type_custom_pct, &get_percentage);
}

void ping_filter::filter_obj::add(boost::shared_ptr<ping_filter::filter_obj> other) {
  if (!other) return;
  result.num_send_ += other->result.num_send_;
  result.num_replies_ += other->result.num_replies_;
  result.num_timeouts_ += other->result.num_timeouts_;
  result.time_ += other->result.time_;
}

//////////////////////////////////////////////////////////////////////////

boost::shared_ptr<ping_filter::filter_obj> ping_filter::filter_obj::get_total() { return boost::make_shared<ping_filter::filter_obj>(); }