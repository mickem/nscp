/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include <nscapi/nscapi_protobuf_types.hpp>
/*
namespace nscapi {
	namespace protobuf {
		namespace types {
			std::string destination_container::to_string() const {
				std::stringstream ss;
				ss << "id: " << id;
				ss << ", address: " << address.to_string();
				ss << ", comment: " << comment;
				int i=0;
				BOOST_FOREACH(std::string a, tags) {
					ss << ", tags[" << i++ << "]: " << a;
				}
				BOOST_FOREACH(const data_map::value_type &kvp, data) {
					ss << ", data[" << kvp.first << "]: " << kvp.second;
				}
				return ss.str();
			}

			void destination_container::import(const destination_container &other) {
				if (id.empty() && !other.id.empty())
					id = other.id;
				address.import(other.address);
				if (comment.empty() && !other.comment.empty())
					comment = other.comment;
				BOOST_FOREACH(const std::string &t, other.tags) {
					tags.insert(t);
				}
				BOOST_FOREACH(const data_map::value_type &kvp, other.data) {
					if (data.find(kvp.first) == data.end())
						data[kvp.first] = kvp.second;
				}
			}
			void destination_container::apply(const destination_container &other) {
				if (!other.id.empty())
					id = other.id;
				address.apply(other.address);
				if (!other.comment.empty())
					comment = other.comment;
				BOOST_FOREACH(const std::string &t, other.tags) {
					tags.insert(t);
				}
				BOOST_FOREACH(const data_map::value_type &kvp, other.data) {
					data[kvp.first] = kvp.second;
				}
			}
		}
	}
}
*/