#pragma once

#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>

#include <nsca/nsca_packet.hpp>
#include <cryptopp/cryptopp.hpp>

#include "handler.hpp"

namespace nsca {
	namespace server {
		class parser : public boost::noncopyable{
			unsigned int payload_length_;
			unsigned int packet_length_;

			std::string buffer_;
			boost::shared_ptr<nsca::server::handler> handler_;
		public:
			parser(unsigned int payload_length) 
				: payload_length_(payload_length)
				, packet_length_(nsca::length::get_packet_length(payload_length)) 
			{}

			template <typename InputIterator>
			boost::tuple<bool, InputIterator> digest(InputIterator begin, InputIterator end) {
				int count = packet_length_ - buffer_.size();
				for (; count > 0&& begin != end; ++begin, --count)
					buffer_.push_back(*begin);
				return boost::make_tuple(buffer_.size() >= packet_length_, begin);
			}

			void decrypt(nscp::encryption::engine &encryption) {
				encryption.decrypt_buffer(buffer_);
			}
			nsca::packet parse() {
				nsca::packet packet(payload_length_);
				packet.parse_data(buffer_.c_str(), buffer_.size());
				buffer_.clear();
				return packet;
			}
			std::string get_buffer() const {
				return buffer_;
			}
			std::string::size_type size() {
				return buffer_.size();
			}
		};

	}// namespace server
} // namespace nsca
