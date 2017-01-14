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

#pragma once

#include <parsers/where/node.hpp>

namespace parsers {
	namespace where {
		struct parser {
			node_type resulting_tree;
			std::string rest;
			bool parse(object_factory factory, std::string expr);
			bool derive_types(object_converter converter);
			bool static_eval(evaluation_context context);
			bool bind(object_converter context);
			value_container evaluate(evaluation_context context);
			bool collect_perfkeys(evaluation_context context, performance_collector &boundries);
			std::string result_as_tree() const;
			std::string result_as_tree(evaluation_context context) const;
			bool require_object(evaluation_context context) const;
		};
	}
}