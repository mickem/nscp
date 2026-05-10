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
#include <memory>

using namespace boost::assign;
using namespace parsers::where;

//////////////////////////////////////////////////////////////////////////

node_type get_percentage(std::shared_ptr<ping_filter::filter_obj> object, evaluation_context context, node_type subject) {
  helpers::read_arg_type value = helpers::read_arguments(context, subject, "%");
  double number = value.get<1>();
  std::string unit = value.get<2>();

  if (unit != "%") context->error("Invalid unit: " + unit);
  return factory::create_int(static_cast<long long>(number));
}

ping_filter::filter_obj_handler::filter_obj_handler() {
  static constexpr value_type type_custom_pct = type_custom_int_1;

  registry_.add_string_var("host", &filter_obj::get_host, "The host name or ip address (as given on command line)")
      .add_string_var("ip", &filter_obj::get_ip, "The ip address name");
  registry_.add_int_var_w_context("loss", type_custom_pct, &filter_obj::get_loss, "Packet loss")
      .add_int_var("time", type_int, &filter_obj::get_time, "Round trip time in ms")
      .add_int_var("sent", type_int, &filter_obj::get_sent, "Number of packets sent to the host")
      .add_int_var("recv", type_int, &filter_obj::get_recv, "Number of packets received from the host")
      .add_int_var("timeout", type_int, &filter_obj::get_timeout, "Number of packets which timed out from the host");
  registry_.add_converter(type_custom_pct, &get_percentage);
}

void ping_filter::filter_obj::add(std::shared_ptr<ping_filter::filter_obj> other) {
  if (!other) return;
  result.num_send_ += other->result.num_send_;
  result.num_replies_ += other->result.num_replies_;
  result.num_timeouts_ += other->result.num_timeouts_;
  result.time_ += other->result.time_;
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<ping_filter::filter_obj> ping_filter::filter_obj::get_total() { return std::make_shared<ping_filter::filter_obj>(); }
