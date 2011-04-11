#pragma once

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>

#include <strEx.h>

using boost::asio::ip::tcp;

namespace http {
	class client {
	public:
		static bool download(std::string protocol, std::string server, std::string path, std::ostream &os, std::string &error_msg) {
			try {

				boost::asio::io_service io_service;

				// Get a list of endpoints corresponding to the server name.
				tcp::resolver resolver(io_service);
				tcp::resolver::query query(server, protocol);
				tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
				tcp::resolver::iterator end;

				// Try each endpoint until we successfully establish a connection.
				tcp::socket socket(io_service);
				boost::system::error_code error = boost::asio::error::host_not_found;
				while (error && endpoint_iterator != end) {
					socket.close();
					socket.connect(*endpoint_iterator++, error);
				}
				if (error)
					throw boost::system::system_error(error);

				// Form the request. We specify the "Connection: close" header so that the
				// server will close the socket after transmitting the response. This will
				// allow us to treat all data up until the EOF as the content.
				boost::asio::streambuf request;
				std::ostream request_stream(&request);
				request_stream << "GET " << path << " HTTP/1.0\r\n";
				request_stream << "Host: " << server << "\r\n";
				request_stream << "Accept: */*\r\n";
				request_stream << "Connection: close\r\n\r\n";

				// Send the request.
				boost::asio::write(socket, request);

				// Read the response status line.
				boost::asio::streambuf response;
				boost::asio::read_until(socket, response, "\r\n");

				// Check that response is OK.
				std::istream response_stream(&response);
				std::string http_version;
				response_stream >> http_version;
				unsigned int status_code;
				response_stream >> status_code;
				std::string status_message;
				std::getline(response_stream, status_message);
				if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
					error_msg = "Invalid response";
					return false;
				}
				if (status_code != 200) {
					error_msg = "Response returned with status code " + strEx::wstring_to_string(strEx::itos(status_code));
					return false;
				}

				// Read the response headers, which are terminated by a blank line.
				boost::asio::read_until(socket, response, "\r\n\r\n");

				// Process the response headers.
				std::string header;
				while (std::getline(response_stream, header) && header != "\r")
					;
					//std::cout << "HDR: " << header << "\n";

				// Write whatever content we already have to output.
				if (response.size() > 0)
					os << &response;

				// Read until EOF, writing data to output as we go.
				while (boost::asio::read(socket, response, boost::asio::transfer_at_least(1), error))
					os << &response;
				if (error != boost::asio::error::eof)
					throw boost::system::system_error(error);
			} catch (std::exception& e) {
				error_msg = std::string("Exception: ") + e.what();
				return false;
			}
			return true;
		}
	};

}
