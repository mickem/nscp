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

#include <nscapi/nscapi_protobuf.hpp>

namespace nscapi {
	namespace metrics {
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, long long value);
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, unsigned long long value);
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, std::string value);
		void add_metric(Plugin::Common::MetricsBundle *b, const std::string &key, double value);
	}
}
