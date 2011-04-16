#pragma once

#include <boost/shared_ptr.hpp>

#include <socket_helpers.hpp>

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

		virtual void connect(std::wstring host, int port) {
			tcp::resolver resolver(get_io_service());
			tcp::resolver::query query(utf8::cvt<std::string>(host), boost::lexical_cast<std::string>(port));

			tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
			tcp::resolver::iterator end;

			boost::system::error_code error = boost::asio::error::host_not_found;
			while (error && endpoint_iterator != end) {
				tcp::resolver::endpoint_type ep = *endpoint_iterator;
				get_socket().close();
				get_socket().lowest_layer().connect(*endpoint_iterator++, error);
			}
			if (error)
				throw boost::system::system_error(error);
		}

		~socket() {
			get_socket().close();
		}

		virtual void send_nsca(nsca::packet &packet, boost::posix_time::seconds timeout) {
			std::string buffer = crypt_inst.get_rand_buffer(packet.get_packet_length());
			packet.get_buffer(buffer);
			crypt_inst.encrypt_buffer(buffer);
			write_with_timeout(buffer, timeout);
		}
		virtual void recv_iv(std::string password, int encryption_method, boost::posix_time::seconds timeout) {
			unsigned int len = nsca::length::iv::get_packet_length();
			std::vector<char> buf(len);
			read_with_timeout(buf, timeout);
			std::string str_buf(buf.begin(), buf.end());
			crypt_inst.encrypt_init(password, encryption_method, str_buf);
		}
		virtual void read_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			socketHelpers::io::read_with_timeout(*socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
		virtual void write_with_timeout(std::string &buf, boost::posix_time::seconds timeout) {
			socketHelpers::io::write_with_timeout(*socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
		/*
		virtual void read_with_timeout(std::string &buf, boost::posix_time::seconds timeout) {
			socketHelpers::io::read_with_timeout(*socket_, get_socket(), boost::asio::mutable_buffer(buf), timeout);
		}
		virtual void write_with_timeout(std::string &buf, boost::posix_time::seconds timeout) {
			socketHelpers::io::write_with_timeout(*socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
		*/
	};



#ifdef USE_SSL
	class ssl_socket : public socket {
	private:
		boost::shared_ptr<boost::asio::ssl::stream<tcp::socket> > ssl_socket_;

	public:
		ssl_socket(boost::asio::io_service &io_service, boost::asio::ssl::context &ctx, std::wstring host, int port) : socket() {
			ssl_socket_.reset(new boost::asio::ssl::stream<tcp::socket>(io_service, ctx));
		}

		virtual void connect(std::wstring host, int port) {
			socket::connect(host, port);
			ssl_socket_->handshake(boost::asio::ssl::stream_base::client);
		}

		virtual boost::asio::io_service& get_io_service() {
			return ssl_socket_->get_io_service();
		}
		virtual basic_socket_type& get_socket() {
			return ssl_socket_->lowest_layer();
		}

		virtual void write_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			socketHelpers::io::write_with_timeout(*ssl_socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}

		virtual void read_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			socketHelpers::io::read_with_timeout(*ssl_socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
	};
#endif
}