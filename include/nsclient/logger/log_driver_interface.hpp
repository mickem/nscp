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

#include <nsclient/logger/logger.hpp>

#include <boost/shared_ptr.hpp>

#include <string>

namespace nsclient {
	namespace logging {


		struct log_driver_interface {

			log_driver_interface() {}
			virtual ~log_driver_interface() {}
			virtual void do_log(const std::string data) = 0;
			virtual void synch_configure() = 0;
			virtual void asynch_configure() = 0;

			virtual void set_config(const std::string &key) = 0;
			virtual void set_config(const boost::shared_ptr<log_driver_interface> other) = 0;
			virtual bool shutdown() = 0;
			virtual bool startup() = 0;
			virtual bool is_console() const = 0;
			virtual bool is_oneline() const = 0;
			virtual bool is_no_std_err() const = 0;
			virtual bool is_started() const = 0;

		};

		typedef boost::shared_ptr<log_driver_interface> log_driver_instance;

	}
}