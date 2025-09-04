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

#include <list>

using namespace parsers::where;

node_type get_column_fun(const value_type, evaluation_context context, const node_type subject) {
  std::list<node_type> l = subject->get_list_value(context);
  if (l.size() != 1) {
    context->error("Invalid number of arguments for function");
    return factory::create_false();
  }
  node_type f = l.front();
  long long idx = f->get_int_value(context);
  logfile_filter::native_context* n_context = reinterpret_cast<logfile_filter::native_context*>(context.get());
  std::string value = n_context->get_object()->get_column(idx);
  return factory::create_string(value);
}

//////////////////////////////////////////////////////////////////////////

logfile_filter::filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string()
    ("line", [] (auto obj, auto context) { return obj->get_line(); }, "Match the content of an entire line")
    ("column1", [] (auto obj, auto context) { return obj->get_column(1); }, [] (auto obj, auto context) { return obj->get_column_number(1); }, "The value in the first column")
    ("column2", [] (auto obj, auto context) { return obj->get_column(2); }, [] (auto obj, auto context) { return obj->get_column_number(2); }, "The value in the second column")
    ("column3", [] (auto obj, auto context) { return obj->get_column(3); }, [] (auto obj, auto context) { return obj->get_column_number(3); }, "The value in the third column")
    ("column4", [] (auto obj, auto context) { return obj->get_column(4); }, [] (auto obj, auto context) { return obj->get_column_number(4); }, "The value in the 4:th column")
    ("column5", [] (auto obj, auto context) { return obj->get_column(5); }, [] (auto obj, auto context) { return obj->get_column_number(5); }, "The value in the 5:th column")
    ("column6", [] (auto obj, auto context) { return obj->get_column(6); }, [] (auto obj, auto context) { return obj->get_column_number(6); }, "The value in the 6:th column")
    ("column7", [] (auto obj, auto context) { return obj->get_column(7); }, [] (auto obj, auto context) { return obj->get_column_number(7); }, "The value in the 7:th column")
    ("column8", [] (auto obj, auto context) { return obj->get_column(8); }, [] (auto obj, auto context) { return obj->get_column_number(8); }, "The value in the 8:th column")
    ("column9", [] (auto obj, auto context) { return obj->get_column(9); }, [] (auto obj, auto context) { return obj->get_column_number(9); }, "The value in the 9:th column")
      ("filename", [](auto obj, auto context) { return obj->filename; }, "The name of the file")
      ("file", [](auto obj, auto context) { return obj->filename; }, "The name of the file")
    ;

  registry_.add_string_fun()
    ("column", &get_column_fun, "Fetch the value from the given column number.\nSyntax: column(<coulmn number>)")
    ;
  // clang-format on
}