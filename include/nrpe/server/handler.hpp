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

#include <nrpe/packet.hpp>

#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>

#include <list>
namespace nrpe {
namespace server {
class handler : boost::noncopyable {
 public:
  virtual std::list<nrpe::packet> handle(nrpe::packet packet) = 0;
  virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
  virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
  virtual nrpe::packet create_error(std::string msg) = 0;
  virtual unsigned int get_payload_length() = 0;
};
}  // namespace server
}  // namespace nrpe