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

#include <types.hpp>
#include <string>
#include <boost/asio/buffer.hpp>
#include <str/xtos.hpp>

namespace check_nt {
	class check_nt_exception {
		std::wstring error_;
	public:
		check_nt_exception(std::wstring error) : error_(error) {}
		std::wstring getMessage() {
			return error_;
		}
	};
	class check_nt_packet_exception : public check_nt_exception {
	public:
		check_nt_packet_exception(std::wstring error) : check_nt_exception(error) {}
	};

	class packet /*: public boost::noncopyable*/ {
	private:
		std::string data_;
	public:
		packet() {};
		packet(std::vector<char> buffer) {
			data_ = std::string(buffer.begin(), buffer.end());
		};
		packet(std::string data)
			: data_(data) {}
		packet(const packet &other) : data_(other.data_) {}
		packet& operator=(packet const& other) {
			data_ = other.data_;
			return *this;
		}

		~packet() {
			//delete [] tmpBuffer;
		}

		std::vector<char> get_buffer() const {
			return std::vector<char>(data_.begin(), data_.end());
		}
		std::string get_payload() const {
			return data_;
		}

		unsigned int get_packet_length() const { return data_.length(); }
		boost::asio::const_buffer to_buffers() const {
			return boost::asio::buffer(get_buffer(), get_packet_length());
		}
		std::string to_string() {
			std::stringstream ss;
			ss << "data: " << data_;
			return ss.str();
		}
	};
}