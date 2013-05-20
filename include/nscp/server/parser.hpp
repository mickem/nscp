#pragma once

#include <nscp/packet.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>

#include <protobuf/ipc.pb.h>
#include "handler.hpp"

namespace nscp {
	namespace server {
		struct digester : public boost::noncopyable {
			std::vector<char> buffer_;
			nscp::packet packet_;

			void reset() {
				buffer_.clear();
			}

			template<typename iterator_type>
			boost::tuple<bool, iterator_type> digest(iterator_type begin, iterator_type end) {
				bool result = false;
				for (; begin != end; ++begin)
					buffer_.push_back(*begin);
				if (buffer_.size() >= nscp::length::get_header_size()) {
					nscp::data::frame *tmp = reinterpret_cast<nscp::data::frame*>(&(*buffer_.begin()));
					unsigned int frame_length = nscp::length::get_frame_size(tmp->header);
					if (buffer_.size() >= frame_length) {
						bool last_frame = tmp->header.flags&nscp::data::flag_last_frame==1;
						// TODO: push frames!
						buffer_.erase(buffer_.begin(), buffer_.begin()+frame_length);
						return boost::make_tuple(last_frame, end);
					}
				}
			}
			nscp::packet get_packet() const { return packet_; }

			unsigned int get_next_size()  {
				if (buffer_.size() < nscp::length::get_header_size())
					return nscp::length::get_header_size();

				nscp::data::frame *tmp = reinterpret_cast<nscp::data::frame*>(&(*buffer_.begin()));
				unsigned int frame_length = nscp::length::get_frame_size(tmp->header);
				if (buffer_.size() < frame_length) 
					return frame_length;

				if (tmp->header.flags&nscp::data::flag_last_frame==1)
					return 0;

				return nscp::length::get_header_size();
			}


		};
	}// namespace server
} // namespace nscp
