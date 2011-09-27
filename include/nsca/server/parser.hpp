#pragma once

#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>

#include <nsca/nsca_packet.hpp>
#include <nsca/nsca_enrypt.hpp>

#include "handler.hpp"

namespace nsca {
	namespace server {
		class parser : public boost::noncopyable{
			std::string buffer_;
			unsigned int packet_length_;
			unsigned int payload_length_;
			boost::shared_ptr<nsca::server::handler> handler_;
		public:
			parser(boost::shared_ptr<nsca::server::handler> handler) : handler_(handler) {
				set_payload_length(handler->get_payload_length());
			}

			template <typename InputIterator>
			boost::tuple<bool, InputIterator> digest(InputIterator begin, InputIterator end) {
				int count = packet_length_ - buffer_.size();
				for (; count > 0&& begin != end; ++begin, --count)
					buffer_.push_back(*begin);
				return boost::make_tuple(buffer_.size() >= packet_length_, begin);
			}

			nsca::packet parse(nsca::nsca_encrypt &encryption) {
				nsca::packet packet(payload_length_);
				//std::string buffer = encryption.get_rand_buffer(packet.get_packet_length());
				//packet.get_buffer(buffer);
				encryption.decrypt_buffer(buffer_);
				packet.parse_data(buffer_.c_str(), buffer_.size());
				buffer_.clear();
				return packet;
			}
			void set_payload_length(unsigned int length) {
				payload_length_ = length;
				packet_length_ = nsca::length::get_packet_length(length);
			}
		};

	}// namespace server
} // namespace nsca
