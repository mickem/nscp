#pragma once

#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>
#include <nsca/nsca_packet.hpp>

using boost::asio::ip::tcp;

namespace nscp {
	namespace client {

	class socket : public boost::noncopyable {
	private:
		boost::shared_ptr<tcp::socket> socket_;
	public:
		typedef boost::asio::basic_socket<tcp,boost::asio::stream_socket_service<tcp> >  basic_socket_type;

	public:
		socket(boost::asio::io_service &io_service, std::wstring host, int port) {
			socket_.reset(new tcp::socket(io_service));
			connect(host, port);
		}
		socket() {
		}
	public:

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

		virtual void connect(std::wstring host, int port) {
			tcp::resolver resolver(get_io_service());
			tcp::resolver::query query(to_string(host), to_string(port));
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

		virtual void send(const std::list<nscp::packet> &chunks, boost::posix_time::seconds timeout) {
			boost::shared_ptr<socket_helpers::io::timed_writer> writer(new socket_helpers::io::timed_writer(get_io_service()));
			writer->start_timer(timeout);
			BOOST_FOREACH(const nscp::packet &chunk, chunks) {
				std::wcout << _T(">>>") << chunk.signature.to_wstring() << std::endl;
				if (!writer->write_and_wait(*socket_, get_socket(), boost::asio::buffer(chunk.write_string()))) {
					std::cout << "FaILED TO SEND DATA..." << std::endl;
					return;
				}
			}
			writer->stop_timer();
			writer.reset();
		}
		virtual std::list<nscp::packet> recv(boost::posix_time::seconds timeout) {
			int left = 1;
			std::list<nscp::packet> chunks;
			boost::shared_ptr<socket_helpers::io::timed_reader> reader(new socket_helpers::io::timed_reader(get_io_service()));
			reader->start_timer(timeout);
			while (left > 0) {
				nscp::packet chunk;
				std::vector<char> buf(sizeof(nscp::data::signature_packet));
				if (!reader->read_and_wait(*socket_, get_socket(), boost::asio::buffer(buf))) {
					get_socket().close();
					std::cout << "Timeout (sig)..." << std::endl;
					return chunks;
				}
				chunk.read_signature(buf);
				std::wcout << _T("<<<") << chunk.signature.to_wstring() << std::endl;
				buf.resize(chunk.signature.payload_length);

				if (!reader->read_and_wait(*socket_, get_socket(), boost::asio::buffer(buf))) {
					get_socket().close();
					std::cout << "Timeout (pl)..." << std::endl;
					return chunks;
				}
				chunk.read_payload(buf);
				chunks.push_back(chunk);
				left = chunk.signature.additional_packet_count;
			}
			reader->stop_timer();
			reader.reset();

			get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
			get_socket().close();
			return chunks;
		}
	};



#ifdef USE_SSL
	class ssl_socket : public socket {
	private:
		boost::shared_ptr<boost::asio::ssl::stream<tcp::socket> > ssl_socket_;

	public:
		ssl_socket(boost::asio::io_service &io_service, boost::asio::ssl::context &ctx, std::wstring host, int port) : socket() {
			ssl_socket_.reset(new boost::asio::ssl::stream<tcp::socket>(io_service, ctx));
			connect(host, port);
		}
		virtual ~ssl_socket() {
			ssl_socket_.reset();
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

		virtual void send(const std::list<nscp::packet> &chunks, boost::posix_time::seconds timeout) {
			boost::shared_ptr<socket_helpers::io::timed_writer> writer(new socket_helpers::io::timed_writer(get_io_service()));
			writer->start_timer(timeout);
			BOOST_FOREACH(const nscp::packet &chunk, chunks) {
				std::wcout << _T(">>>") << chunk.signature.to_wstring() << std::endl;
				if (!writer->write_and_wait(*ssl_socket_, get_socket(), boost::asio::buffer(chunk.write_string()))) {
					std::cout << "FaILED TO SEND DATA..." << std::endl;
					return;
				}
			}
			writer->stop_timer();
			writer.reset();
		}

		virtual std::list<nscp::packet> recv(boost::posix_time::seconds timeout) {
			int left = 1;
			std::list<nscp::packet> chunks;
			boost::shared_ptr<socket_helpers::io::timed_reader> reader(new socket_helpers::io::timed_reader(get_io_service()));
			reader->start_timer(timeout);
			while (left > 0) {
				nscp::packet chunk;
				std::vector<char> buf(sizeof(nscp::data::signature_packet));
				if (!reader->read_and_wait(*ssl_socket_, get_socket(), boost::asio::buffer(buf))) {
					get_socket().close();
					std::cout << "Timeout (sig)..." << std::endl;
					return chunks;
				}
				chunk.read_signature(buf);
				std::wcout << _T("<<<") <<  chunk.signature.to_wstring() << std::endl;
				buf.resize(chunk.signature.payload_length);

				if (!reader->read_and_wait(*ssl_socket_, get_socket(), boost::asio::buffer(buf))) {
					get_socket().close();
					std::cout << "Timeout (pl)..." << std::endl;
					return chunks;
				}
				chunk.read_payload(buf);
				chunks.push_back(chunk);
				left = chunk.signature.additional_packet_count;
			}
			reader->stop_timer();
			reader.reset();

			get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
			get_socket().close();
			return chunks;
		}

	};
#endif
}
}