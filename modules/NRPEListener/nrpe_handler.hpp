#include <nrpe/nrpepacket.hpp>
#include <boost/tuple/tuple.hpp>

namespace nrpe {
	namespace server {
		class handler {
			unsigned int payload_length_;
		public:
			handler(unsigned int payload_length) 
				: payload_length_(payload_length)
			{}
			handler(const handler &other) {
				payload_length_ = other.payload_length_;
			}
			handler& operator= (const handler &other) {
				payload_length_ = other.payload_length_;
				return *this;
			}
			unsigned int get_payload_length() {
				return payload_length_;
			}
			nrpe::packet handle(nrpe::packet packet) {
				return nrpe::packet::create_response(1, _T("HELLO!"), payload_length_);
			}
			nrpe::packet create_error(std::wstring msg) {
				return nrpe::packet::create_response(4, msg, payload_length_);
			}
		};

	}// namespace server
} // namespace nrpe
