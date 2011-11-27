#pragma once

#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>
#include <nsca/nsca_packet.hpp>

using boost::asio::ip::tcp;

namespace nrpe {
	namespace client {

	class socket : public boost::noncopyable {
	private:
		boost::shared_ptr<tcp::socket> socket_;
	public:
		typedef boost::asio::basic_socket<tcp,boost::asio::stream_socket_service<tcp> >  basic_socket_type;

	public:
		socket(boost::asio::io_service &io_service, std::string host, std::string port) {
			socket_.reset(new tcp::socket(io_service));
			connect(host, port);
		}
		socket() {
		}

		virtual ~socket() {
			socket_.reset();
			//get_socket().close();
		}


		virtual boost::asio::io_service& get_io_service() {
			return socket_->get_io_service();
		}
		virtual basic_socket_type& get_socket() {
			return *socket_;
		}

		virtual void connect(std::string host, std::string port) {
			tcp::resolver resolver(get_io_service());
			tcp::resolver::query query(host, port);
			//tcp::resolver::query query("www.medin.name", "80");
			//tcp::resolver::query query("test_server", "80");

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

		virtual void send(nrpe::packet &packet, boost::posix_time::seconds timeout) {
			std::vector<char> buf = packet.get_buffer();
			write_with_timeout(buf, timeout);
		}
		virtual nrpe::packet recv(const nrpe::packet &packet, boost::posix_time::seconds timeout) {
			std::vector<char> buf(packet.get_packet_length());
			if (!read_with_timeout(buf, timeout))
				return nrpe::packet(nrpe::data::responsePacket, nrpe::data::version2, NSCAPI::returnUNKNOWN, _T("Failed to read data"), packet.get_payload_length());
			get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
			get_socket().close();
			return nrpe::packet(&buf[0], buf.size(), packet.get_payload_length());
		}
		virtual bool read_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			return socket_helpers::io::read_with_timeout(*socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
		virtual bool write_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			return socket_helpers::io::write_with_timeout(*socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
	};



#ifdef USE_SSL
	class ssl_socket : public socket {
	private:
		boost::shared_ptr<boost::asio::ssl::stream<tcp::socket> > ssl_socket_;

	public:
		ssl_socket(boost::asio::io_service &io_service, boost::asio::ssl::context &ctx, std::string host, std::string port) : socket() {
			ssl_socket_.reset(new boost::asio::ssl::stream<tcp::socket>(io_service, ctx));
			connect(host, port);
		}
		virtual ~ssl_socket() {
			ssl_socket_.reset();
		}


		virtual void connect(std::string host, std::string port) {
			socket::connect(host, port);
			ssl_socket_->handshake(boost::asio::ssl::stream_base::client);
		}

		virtual boost::asio::io_service& get_io_service() {
			return ssl_socket_->get_io_service();
		}
		virtual basic_socket_type& get_socket() {
			return ssl_socket_->lowest_layer();
		}

		virtual bool write_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			return socket_helpers::io::write_with_timeout(*ssl_socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}

		virtual bool read_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			return socket_helpers::io::read_with_timeout(*ssl_socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
	};
#endif
}
}