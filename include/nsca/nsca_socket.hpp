#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

#include <socket/socket_helpers.hpp>

#include <nsca/nsca_packet.hpp>
#include <nsca/nsca_enrypt.hpp>

using boost::asio::ip::tcp;

namespace nsca {

	class socket : public boost::noncopyable {
	private:
		boost::shared_ptr<tcp::socket> socket_;
		nsca_encrypt crypt_inst;
	public:
		typedef boost::asio::basic_socket<tcp,boost::asio::stream_socket_service<tcp> >  basic_socket_type;

	public:
		socket(boost::asio::io_service &io_service) {
			socket_.reset(new tcp::socket(io_service));
		}
		socket() {}

		virtual boost::asio::io_service& get_io_service() {
			return socket_->get_io_service();
		}
		virtual basic_socket_type& get_socket() {
			return *socket_;
		}

		virtual void connect(std::string host, int port) {
			NSC_DEBUG_MSG(_T("Connecting to: ") + to_wstring(host) + _T(" (") + to_wstring(port) + _T(")"));
			tcp::resolver resolver(get_io_service());
			tcp::resolver::query query(host, boost::lexical_cast<std::string>(port));

			tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
			tcp::resolver::iterator end;

			boost::system::error_code error = boost::asio::error::host_not_found;
			while (error && endpoint_iterator != end) {
				tcp::resolver::endpoint_type ep = *endpoint_iterator;
				get_socket().close();
				get_socket().lowest_layer().connect(*endpoint_iterator++, error);
				NSC_DEBUG_MSG(_T("Connected to: ") + to_wstring(ep.address().to_string()));
			}
			if (error)
				throw boost::system::system_error(error);
		}

		~socket() {
			get_socket().close();
		}

		virtual void send_nsca(const nsca::packet &packet, const boost::posix_time::seconds timeout) {
			if (!get_socket().is_open()) {
				NSC_DEBUG_MSG(_T("is closed..."));
				return;
			}
			std::string buffer = crypt_inst.get_rand_buffer(packet.get_packet_length());
			packet.get_buffer(buffer);
			crypt_inst.encrypt_buffer(buffer);
			write_with_timeout(buffer, timeout);
		}
		virtual bool recv_iv(std::string password, int encryption_method, boost::posix_time::seconds timeout) {
			if (!get_socket().is_open()) {
				NSC_DEBUG_MSG(_T("is closed..."));
				return false;
			}
			unsigned int len = nsca::length::iv::get_packet_length();
			std::vector<char> buf(len);
			if (!read_with_timeout(buf, timeout)) {
				NSC_LOG_ERROR_STD(_T("Failed to read IV from server (using ") + strEx::itos(encryption_method) + _T(", ") + strEx::itos(len) + _T(")."));
				return false;
			}
			std::string str_buf(buf.begin(), buf.end());
			NSC_DEBUG_MSG(_T("Encrypting using when sending: ") + utf8::cvt<std::wstring>(nsca::nsca_encrypt::helpers::encryption_to_string(encryption_method)) + _T(" and ") + utf8::cvt<std::wstring>(password));
			crypt_inst.encrypt_init(password, encryption_method, str_buf);
			return true;
		}
		virtual bool read_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			return socket_helpers::io::read_with_timeout(*socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
		virtual void write_with_timeout(std::string &buf, boost::posix_time::seconds timeout) {
			socket_helpers::io::write_with_timeout(*socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
	};
}