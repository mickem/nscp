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
			boost::shared_ptr<nscp::server::server_handler> handler_;
		public:
			parser(boost::shared_ptr<nscp::server::server_handler> handler) : handler_(handler) {}

			typedef boost::function<boost::tuple<bool, char*>(parser*, char*, char*)> digest_function;

			template <typename InputIterator>
			boost::tuple<bool, InputIterator> digest_anything(InputIterator begin, InputIterator end, unsigned long wanted) {
				int count = wanted - buffer_.size();
				for (; count > 0&& begin != end; ++begin, --count)
					buffer_.push_back(*begin);
				return boost::make_tuple(buffer_.size() >= wanted, begin);
			}

			boost::tuple<bool, char*> digest_signature(char* begin, char* end) {
				return digest_anything(begin, end, nscp::length::get_signature_size());
			}

			template <typename InputIterator>
			InputIterator digest_header(InputIterator begin, InputIterator end, const nscp::data::tcp_signature_data &signature) {
				return digest_anything(begin, end, nscp::length::get_header_size(signature));
			}

			boost::tuple<bool, char*> digest_payload(char* begin, char* end, const nscp::data::tcp_signature_data &signature) {
				return digest_anything(begin, end, nscp::length::get_payload_size(signature));
			}

			nscp::data::tcp_signature_data parse_signature() {
				assert(buffer_.size() >= nscp::length::get_signature_size());
				nscp::data::tcp_signature_data *tmp = reinterpret_cast<nscp::data::tcp_signature_data*>(&(*buffer_.begin()));
				nscp::data::tcp_signature_data signature = *tmp;
				buffer_.clear();
				return signature;
			}
			void parse_header(const nscp::data::tcp_signature_data &signature) {
				unsigned long wanted = nscp::length::get_header_size(signature);
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
	}// namespace server
} // namespace nscp
