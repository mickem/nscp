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