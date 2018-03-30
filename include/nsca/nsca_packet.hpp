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
#include <swap_bytes.hpp>
#include <nscpcrypt/nscpcrypt.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <utils.h>

#include <boost/date_time.hpp>

namespace nsca {
	class data {
	public:
		static const short transmitted_iuv_size = 128;
		static const int16_t version3 = 3;

		typedef struct data_packet : public boost::noncopyable {
			int16_t  packet_version;
			uint32_t crc32_value;
			uint32_t timestamp;
			int16_t  return_code;
			char     data[];
			/*
			char     host_name[];
			char     svc_description[];
			char     plugin_output[];
			*/
			//data_packet_struct() : packet_version(NSCA_PACKET_VERSION_3) {}

			char* get_data_offset(unsigned int offset) {
				return &data[offset];
			}
			const char* get_data_offset(unsigned int offset) const {
				return &data[offset];
			}
			const char* get_host_ptr() const {
				return get_data_offset(0);
			}
			const char* get_desc_ptr(unsigned int host_len) const {
				return get_data_offset(host_len);
			}
			const char* get_result_ptr(unsigned int host_len, unsigned int desc_len) const {
				return get_data_offset(host_len + desc_len);
			}
			char* get_host_ptr() {
				return get_data_offset(0);
			}
			char* get_desc_ptr(unsigned int host_len) {
				return get_data_offset(host_len);
			}
			char* get_result_ptr(unsigned int host_len, unsigned int desc_len) {
				return get_data_offset(host_len + desc_len);
			}
		} data_packet;

		/* initialization packet containing IV and timestamp */
		typedef struct iv_packet {
			char     iv[transmitted_iuv_size];
			uint32_t timestamp;
		} init_packet;
	};

	/* data packet containing service check results */
	class nsca_exception : public std::exception {
		std::string msg_;
	public:
		nsca_exception() {}
		~nsca_exception() throw () {}
		nsca_exception(std::string msg) : msg_(msg) {}
		const char* what() const throw () { return msg_.c_str(); }
	};

	class length {
	public:
		typedef unsigned int size_type;
		static const std::size_t host_length = 64;
		static const std::size_t desc_length = 128;
	public:
		static size_type payload_length_;
		static void set_payload_length(size_type length) {
			payload_length_ = length;
		}
		static size_type get_packet_length() {
			return get_packet_length(payload_length_);
		}
		static size_type get_packet_length(size_type output_length) {
			return sizeof(nsca::data::data_packet) + output_length*sizeof(char) + host_length*sizeof(char) + desc_length*sizeof(char);
		}
		static size_type get_payload_length() {
			return payload_length_;
		}
		static size_type get_payload_length(size_type packet_length) {
			return (packet_length - (host_length*sizeof(char) + desc_length*sizeof(char) + sizeof(nsca::data::data_packet))) / sizeof(char);
		}
		class iv {
		public:
			static const unsigned int payload_length_ = nsca::data::transmitted_iuv_size;
			static size_type get_packet_length() {
				return sizeof(nsca::data::iv_packet);
			}
			static size_type get_payload_length() {
				return payload_length_;
			}
		};
	};

	class packet {
	public:
		std::string service;
		std::string result;
		std::string host;
		unsigned int code;
		unsigned int time;
		unsigned int payload_length_;
	public:
		packet(std::string _host, unsigned int payload_length = 512, int time_delta = 0) : host(_host), payload_length_(payload_length) {
			boost::posix_time::ptime now = boost::posix_time::second_clock::local_time()
				+ boost::posix_time::seconds(time_delta);
			boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970, 1, 1));
			boost::posix_time::time_duration diff = now - time_t_epoch;
			time = diff.total_seconds();
		}
		packet(unsigned int payload_length) : payload_length_(payload_length) {}
		packet() : payload_length_(nsca::length::get_payload_length()) {}
		packet operator =(const packet &other) {
			service = other.service;
			result = other.result;
			host = other.host;
			code = other.code;
			time = other.time;
			payload_length_ = other.payload_length_;
			return *this;
		}

		std::string to_string() const {
			return "host: " + host + ", " +
				"service: " + service + ", " +
				"code: " + str::xtos(code) + ", " +
				"time: " + str::xtos(time) + ", " +
				"result: " + result;
		}

		void parse_data(const char* buffer, unsigned int buffer_len) {
			char *tmp = new char[buffer_len];
			memcpy(tmp, buffer, buffer_len);
			nsca::data::data_packet *data = reinterpret_cast<nsca::data::data_packet*>(tmp);
			//packet_version=swap_bytes::ntoh<int16_t>(data->packet_version);
			time = swap_bytes::ntoh<uint32_t>(data->timestamp);
			code = swap_bytes::ntoh<int16_t>(data->return_code);
			//data->crc32_value= swap_bytes::hton<uint32_t>(0);

			host = data->get_host_ptr();
			service = data->get_desc_ptr(nsca::length::host_length);
			result = data->get_result_ptr(nsca::length::host_length, nsca::length::desc_length);

			unsigned int crc32 = swap_bytes::ntoh<uint32_t>(data->crc32_value);
			data->crc32_value = 0;
			unsigned int calculated_crc32 = calculate_crc32(tmp, buffer_len);
			delete[] tmp;
			if (crc32 != calculated_crc32)
				throw nsca::nsca_exception("Invalid crc: " + str::xtos(crc32) + " != " + str::xtos(calculated_crc32));
		}
		void validate_lengths() const {
			if (service.length() >= nsca::length::desc_length)
				throw nsca::nsca_exception("Description field to long: " + str::xtos(service.length()) + " > " + str::xtos(nsca::length::desc_length));
			if (host.length() >= nsca::length::host_length)
				throw nsca::nsca_exception("Host field to long: " + str::xtos(host.length()) + " > " + str::xtos(nsca::length::host_length));
			if (result.length() >= get_payload_length())
				throw nsca::nsca_exception("Result field to long: " + str::xtos(result.length()) + " > " + str::xtos(get_payload_length()));
		}

		static void copy_string(char* data, const std::string &value, std::string::size_type max_length) {
			memset(data, 0, max_length);
			value.copy(data, value.size() > max_length ? max_length : value.size());
		}

		void get_buffer(std::string &buffer, int servertime = 0) const {
			nsca::data::data_packet *data = reinterpret_cast<nsca::data::data_packet*>(&*buffer.begin());
			if (buffer.size() < get_packet_length())
				throw nsca::nsca_exception("Buffer is to short: " + str::xtos(buffer.length()) + " > " + str::xtos(get_packet_length()));

			data->packet_version = swap_bytes::hton<int16_t>(nsca::data::version3);
			if (servertime != 0)
				data->timestamp = swap_bytes::hton<uint32_t>(static_cast<uint32_t>(servertime));
			else
				data->timestamp = swap_bytes::hton<uint32_t>(static_cast<uint32_t>(time));
			data->return_code = swap_bytes::hton<int16_t>(static_cast<int16_t>(code));
			data->crc32_value = swap_bytes::hton<uint32_t>(0);

			copy_string(data->get_host_ptr(), host, nsca::length::host_length);
			copy_string(data->get_desc_ptr(nsca::length::host_length), service, nsca::length::desc_length);
			copy_string(data->get_result_ptr(nsca::length::host_length, nsca::length::desc_length), result, get_payload_length());

			unsigned int calculated_crc32 = calculate_crc32(buffer.c_str(), static_cast<int>(buffer.size()));
			data->crc32_value = swap_bytes::hton<uint32_t>(calculated_crc32);
		}
		std::string get_buffer() const {
			std::string buffer;
			get_buffer(buffer);
			return buffer;
		}
		unsigned int get_packet_length() const { return nsca::length::get_packet_length(payload_length_); }
		unsigned int get_payload_length() const { return payload_length_; }
	};

	class iv_packet {
		std::string iv;
		uint32_t time;
	public:
		iv_packet(std::string iv, uint32_t time) : iv(iv), time(time) {}
		iv_packet(std::string iv, boost::posix_time::ptime now) : iv(iv), time(ptime_to_unixtime(now)) {}
		iv_packet(std::string buffer) {
			parse(buffer);
		}
		uint32_t ptime_to_unixtime(boost::posix_time::ptime now) {
			//boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
			boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970, 1, 1));
			boost::posix_time::time_duration diff = now - time_t_epoch;
			return diff.total_seconds();
		}
		uint32_t get_time() const {
			return time;
		}
		std::string get_iv() const {
			return iv;
		}

		std::string get_buffer() const {
			if (iv.size() != nsca::length::iv::get_payload_length())
				throw nsca::nsca_exception("Invalid IV size: " + str::xtos(iv.size()) + " != " + str::xtos(nsca::length::iv::get_payload_length()));
			nsca::data::iv_packet data;
			memcpy(data.iv, iv.c_str(), iv.size());
			data.timestamp = swap_bytes::hton<uint32_t>(time);
			char *src = reinterpret_cast<char*>(&data);
			std::string buffer(src, nsca::length::iv::get_packet_length());
			return buffer;
		}
		void parse(const std::string &buffer) {
			if (buffer.size() < nsca::length::iv::get_packet_length())
				throw nsca::nsca_exception("Buffer is to short: " + str::xtos(buffer.length()) + " > " + str::xtos(nsca::length::iv::get_packet_length()));
			const nsca::data::iv_packet *data = reinterpret_cast<const nsca::data::iv_packet*>(buffer.c_str());
			iv = std::string(data->iv, nsca::data::transmitted_iuv_size);
			time = swap_bytes::ntoh<uint32_t>(data->timestamp);
		}
	};
}
