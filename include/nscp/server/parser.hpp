#pragma once

#include <nscp/packet.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>

#include <protobuf/ipc.pb.h>
#include "handler.hpp"

namespace nscp {
	namespace server {

		class parser : public boost::noncopyable {
			std::vector<char> buffer_;
		public:

			template <typename InputIterator>
			boost::tuple<bool, InputIterator> digest_anything(InputIterator begin, InputIterator end, std::size_t wanted) {
				std::size_t count = wanted - buffer_.size();
				for (; count > 0&& begin != end; ++begin, --count)
					buffer_.push_back(*begin);
				return boost::make_tuple(buffer_.size() >= wanted, begin);
			}

			template <typename InputIterator>
			boost::tuple<bool, InputIterator> digest_signature(InputIterator begin, InputIterator end) {
				return digest_anything(begin, end, nscp::length::get_signature_size());
			}

			template <typename InputIterator>
			boost::tuple<bool, InputIterator> digest_header(InputIterator begin, InputIterator end, const nscp::data::tcp_signature_data &signature) {
				return digest_anything(begin, end, nscp::length::get_header_size(signature));
			}

			template <typename InputIterator>
			boost::tuple<bool, InputIterator> digest_payload(InputIterator begin, InputIterator end, const nscp::data::tcp_signature_data &signature) {
				return digest_anything(begin, end, nscp::length::get_payload_size(signature));
			}

			void parse_signature(nscp::packet &packet) {
				assert(buffer_.size() >= nscp::length::get_signature_size());
				nscp::data::tcp_signature_data *tmp = reinterpret_cast<nscp::data::tcp_signature_data*>(&(*buffer_.begin()));
				packet.read_signature(tmp);
				buffer_.clear();
			}
			void parse_header(nscp::packet &packet) {
				unsigned long wanted = nscp::length::get_header_size(packet.signature);
				if (wanted == 0)
					return;
				// @todo parse header and such here
				buffer_.clear();
			}
			void parse_payload(nscp::packet &packet) {
				unsigned long wanted = nscp::length::get_payload_size(packet.signature);
				assert(buffer_.size() >= wanted);
				packet.payload.insert(packet.payload.begin(), buffer_.begin(), buffer_.begin()+wanted);
				buffer_.clear();
			}
			unsigned int get_size() {
				return buffer_.size();
			}
		};


		struct digester : public boost::noncopyable {
			enum state {
				need_signature,
				need_header,
				need_payload
			};

			parser parser_;
			state current_state_;
			nscp::packet packet_;


			void reset() {
				current_state_ = need_signature;
			}

			unsigned long long get_next_size() {
				if (current_state_ == need_signature) {
					return nscp::length::get_signature_size();
				} else if (current_state_ == need_header) {
					return nscp::length::get_header_size(packet_.signature);
				} else if (current_state_ == need_payload) {
					return nscp::length::get_payload_size(packet_.signature);
				}
				return 0;
			}

			template<typename iterator_type>
			boost::tuple<bool, iterator_type> digest(iterator_type begin, iterator_type end) {
				bool result = false;
				if (current_state_ == need_signature) {
					boost::tie(result, begin) = parser_.digest_signature(begin, end);
					if (result) {
						parser_.parse_signature(packet_);
						current_state_ = need_header;
					} else 
						return boost::make_tuple(false, begin);
				}
				if (current_state_ == need_header) {
					boost::tie(result, begin) = parser_.digest_header(begin, end, packet_.signature);
					if (result) {
						parser_.parse_header(packet_);
						current_state_ = need_payload;
					} else
						return boost::make_tuple(false, begin);
				}
				if (current_state_ == need_payload) {
					boost::tie(result, begin) = parser_.digest_payload(begin, end, packet_.signature);
					if (result) {
						parser_.parse_payload(packet_);
						current_state_ = need_signature;
					}
					return boost::make_tuple(result, begin);
				}
				return boost::make_tuple(result, begin);
			}
			nscp::packet get_packet() const { return packet_; }
		};
	}// namespace server
} // namespace nscp
