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
#pragma once

#include <types.hpp>
#include <string>
#include <unicode_char.hpp>
#include <boost/asio/buffer.hpp>
#include <strEx.h>

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