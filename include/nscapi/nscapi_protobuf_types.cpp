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