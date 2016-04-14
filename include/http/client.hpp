#pragma once

#include <iostream>
#include <istream>
#include <ostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/tuple/tuple.hpp>

#include <strEx.h>
#include <socket/socket_helpers.hpp>

using boost::asio::ip::tcp;

namespace http {
	class client {
		boost::asio::io_service io_service;
		tcp::socket socket;

		static std::string charToHex(char c) {
			std::string result;
			char first, second;

			first = (c & 0xF0) / 16;
			first += first > 9 ? 'A' - 10 : '0';
			second = c & 0x0F;
			second += second > 9 ? 'A' - 10 : '0';

			result.append(1, first);
			result.append(1, second);

			return result;
		}

		static std::string uri_encode(const std::string& src) {
			std::string result;
			std::string::const_iterator iter;

			for (iter = src.begin(); iter != src.end(); ++iter) {
				switch (*iter) {
				case ' ':
					result.append(1, '+');
					break;
					// alnum
				case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
				case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
				case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
				case 'V': case 'W': case 'X': case 'Y': case 'Z':
				case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
				case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
				case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
				case 'v': case 'w': case 'x': case 'y': case 'z':
				case '0': case '1': case '2': case '3': case '4': case '5': case '6':
				case '7': case '8': case '9':
					// mark
				case '-': case '_': case '.': case '!': case '~': case '*': case '\'':
				case '(': case ')':
					result.append(1, *iter);
					break;
					// escape
				default:
					result.append(1, '%');
					result.append(charToHex(*iter));
					break;
				}
			}

			return result;
		}

	public:
		client()
			: io_service()
			, socket(io_service) {}

		void connect() {
		}
		struct response_type {
			unsigned int code;
			std::list<std::string> headers;
			std::string payload;
			std::string version;
			std::string message;

			void add_header(const std::string &header) {
				headers.push_back(header);
			}
		};
		struct request_type {
			typedef std::map<std::string, std::string> post_map_type;
			std::string verb;
			std::list<std::string> headers;
			std::string payload;
			void add_default_headers() {
				add_header("Accept:", "*/*");
				add_header("Connection:", "close");
			}
			void add_header(std::string key, std::string value) {
				headers.push_back(key + ": " + value);
			}
			void add_header(std::string value) {
				headers.push_back(value);
			}
			void build_request(std::string verb, std::string server, std::string path, std::ostream &os) const {
				const char* crlf = "\r\n";
				os << verb << " " << path << " HTTP/1.0" << crlf;
				os << "Host: " << server << crlf;
				BOOST_FOREACH(const std::string &s, headers) {
					os << s << crlf;
				}
				os << crlf;
				if (!payload.empty())
					os << payload;
				os << crlf;
				os << crlf;
			}
			void add_post_payload(const post_map_type &payload_map) {
				std::string data;
				BOOST_FOREACH(const post_map_type::value_type &v, payload_map) {
					if (!data.empty())
						data += "&";
					data += uri_encode(v.first);
					data += "=";
					data += uri_encode(v.second);
				}
				add_header("Content-Length", strEx::s::xtos(data.size()));
				add_header("Content-Type", "application/x-www-form-urlencoded");
				verb = "POST";
				payload = data;
			}
			void add_post_payload(const std::string &payload_data) {
				add_header("Content-Length", strEx::s::xtos(payload_data.size()));
				add_header("Content-Type", "application/x-www-form-urlencoded");
				verb = "POST";
				payload = payload_data;
			}
		};

		void connect(std::string server, std::string port) {
			tcp::resolver resolver(io_service);
			tcp::resolver::query query(server, port);
			tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
			tcp::resolver::iterator end;

			boost::system::error_code error = boost::asio::error::host_not_found;
			while (error && endpoint_iterator != end) {
				socket.close();
				socket.connect(*endpoint_iterator++, error);
			}
			if (error)
				throw boost::system::system_error(error);
		}
		void send_request(std::string server, std::string path, request_type &request) {
			boost::asio::streambuf requestbuf;
			std::ostream request_stream(&requestbuf);
			request.build_request(request.verb, server, path, request_stream);
			boost::asio::write(socket, requestbuf);
		}
		boost::tuple<std::string, unsigned int, std::string> read_result(boost::asio::streambuf &response) {
			std::string http_version, status_message;
			unsigned int status_code;
			boost::asio::read_until(socket, response, "\r\n");

			std::istream response_stream(&response);
			if (!response_stream)
				throw socket_helpers::socket_exception("Invalid response");
			response_stream >> http_version;
			response_stream >> status_code;
			std::getline(response_stream, status_message);
			return boost::make_tuple(http_version, status_code, status_message);
		}

		response_type execute(std::string server, std::string port, std::string path, request_type &request) {
			response_type response;
			connect(server, port);
			send_request(server, path, request);

			boost::asio::streambuf response_buffer;
			boost::tie(response.version, response.code, response.message) = read_result(response_buffer);

			if (response.version.substr(0, 5) != "HTTP/")
				throw socket_helpers::socket_exception("Invalid response: " + response.version);

			try {
				boost::asio::read_until(socket, response_buffer, "\r\n\r\n");
			} catch (const std::exception &e) {
				throw socket_helpers::socket_exception(std::string("Failed to read header: ") + e.what());
			}

			std::istream response_stream(&response_buffer);
			std::string header;
			while (std::getline(response_stream, header) && header != "\r")
				response.add_header(header);

			std::ostringstream os;
			if (response_buffer.size() > 0)
				os << &response_buffer;

			boost::system::error_code error;
			while (boost::asio::read(socket, response_buffer, boost::asio::transfer_at_least(1), error))
				os << &response_buffer;
			if (error != boost::asio::error::eof)
				throw boost::system::system_error(error);
			response.payload = os.str();

			if (response.code != 200)
				throw socket_helpers::socket_exception(strEx::s::xtos(response.code) + ": " + response.payload);

			return response;
		}

		static bool download(std::string protocol, std::string server, std::string path, std::ostream &os, std::string &error_msg) {
			try {
				request_type rq;
				rq.verb = "GET";
				rq.add_default_headers();
				client c;
				response_type rs = c.execute(server, protocol, path, rq);
				os << rs.payload;
				return true;
			} catch (std::exception& e) {
				error_msg = std::string("Exception: ") + utf8::utf8_from_native(e.what());
				return false;
			}
		}
	};
}