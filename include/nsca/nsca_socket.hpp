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
		boost::asio::io_service &io_service_;
		nsca_encrypt crypt_inst;
		int time;
	public:
		typedef boost::asio::basic_socket<tcp,boost::asio::stream_socket_service<tcp> >  basic_socket_type;

	public:
		socket(boost::asio::io_service &io_service) : io_service_(io_service), time(0) {
			socket_.reset(new tcp::socket(io_service_));
		}
		~socket() {
			if (socket_)
				socket_->close();
			socket_.reset();
		}

		virtual void connect(std::string host, std::string port) {
			NSC_DEBUG_MSG(_T("Connecting to: ") + utf8::cvt<std::wstring>(host) + _T(" (") + utf8::cvt<std::wstring>(port) + _T(")"));
			tcp::resolver resolver(io_service_);
			tcp::resolver::query query(host, port);

			tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
			tcp::resolver::iterator end;

			boost::system::error_code error = boost::asio::error::host_not_found;
			while (error && endpoint_iterator != end) {
				tcp::resolver::endpoint_type ep = *endpoint_iterator;
				socket_->close();
				socket_->connect(*endpoint_iterator++, error);
				NSC_DEBUG_MSG(_T("Connected to: ") + utf8::cvt<std::wstring>(ep.address().to_string()));
			}
			if (error) {
				NSC_DEBUG_MSG(_T("Failed to connect to:") + utf8::to_unicode(host));
				throw boost::system::system_error(error);
			}
		}


		virtual void shutdown() {
			NSC_DEBUG_MSG(_T("Ending socket (gracefully)"));
			// Initiate graceful connection closure.
			boost::system::error_code ignored_ec;
			if (socket_)
				socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
		};
		virtual void close() {
			if (socket_)
				socket_->close();
			socket_.reset();
		};

		virtual void send_nsca(const nsca::packet &packet, const boost::posix_time::seconds timeout) {
			if (!socket_ || !socket_->is_open()) {
				NSC_LOG_ERROR_STD(_T("Socket was closed when trying to send data..."));
				return;
			}
			std::string buffer = crypt_inst.get_rand_buffer(packet.get_packet_length());
			packet.get_buffer(buffer, time);
			crypt_inst.encrypt_buffer(buffer);
			NSC_DEBUG_MSG(_T("Sending data: ") + strEx::itos(buffer.size()));
			write_with_timeout(buffer, timeout);
		}
		virtual bool recv_iv(std::string password, int encryption_method, boost::posix_time::seconds timeout) {
			if (!socket_ || !socket_->is_open()) {
				NSC_LOG_ERROR_STD(_T("Socket was closed when trying to read data..."));
				return false;
			}
			unsigned int len = nsca::length::iv::get_packet_length();
			std::vector<char> buf(len);
			if (!read_with_timeout(buf, timeout)) {
				NSC_LOG_ERROR_STD(_T("Failed to read IV from server (using ") + strEx::itos(encryption_method) + _T(", ") + strEx::itos(len) + _T(")."));
				return false;
			}
			nsca::iv_packet iv_packet(std::string(buf.begin(), buf.end()));
			std::string iv = iv_packet.get_iv();
			time = iv_packet.get_time();
			NSC_DEBUG_MSG(_T("Encrypting using when sending: ") + utf8::cvt<std::wstring>(nsca::nsca_encrypt::helpers::encryption_to_string(encryption_method)) + _T(" and ") + utf8::cvt<std::wstring>(password));
			crypt_inst.encrypt_init(password, encryption_method, iv);
			return true;
		}
		virtual bool read_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			return socket_helpers::io::read_with_timeout(*socket_, *socket_, boost::asio::buffer(buf), timeout);
		}
		virtual void write_with_timeout(std::string &buf, boost::posix_time::seconds timeout) {
			socket_helpers::io::write_with_timeout(*socket_, *socket_, boost::asio::buffer(buf), timeout);
		}
	};
}