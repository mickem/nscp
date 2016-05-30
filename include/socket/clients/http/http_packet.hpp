/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <string>
#include <map>

namespace http {

	bool find_line_end(char c1, char c2) {
		return c1 == '\r' && c2 == '\n';
	}
	bool find_header_break(char c1, char c2) {
		return c1 == '\n' && c2 == '\r';
	}
	struct packet {
		typedef std::map<std::string,std::string> header_type;
		header_type headers_;
		std::string path_;
		std::string verb_;
		std::string payload_;
		int status_code_;

		packet() {}
		packet(std::string verb, std::string path, std::string payload) : verb_(verb), path_(path), payload_(payload), status_code_(0) {
			add_default_headers();
		}
		packet(std::vector<char> &data) {
			std::vector<char>::iterator its = data.begin();
			std::vector<char>::iterator ite = std::adjacent_find(its, data.end(), find_line_end);
			if (ite == data.end())
				return;
			parse_http_response(std::string(its, ite));
			its = ite + 2;
			while (true) {
				std::vector<char>::iterator ite = std::adjacent_find(its, data.end(), find_line_end);
				if (ite == data.end())
					break;
				std::string line(its, ite);
				if (line.empty()) {
					payload_ = std::string(ite+2, data.end());
					break;
				}
				add_header(line);
				its = ite+2;
			}
		}

		void set_path(std::string verb, std::string path) {
			verb_ = verb;
			path_ = path;
		}
		void parse_http_response(std::string line) {
			std::string::size_type pos = line.find(' ');
			if (pos == std::string::npos)
				set_http_response(line, "500");
			else
				set_http_response(line.substr(0, pos), line.substr(pos+1));

		}
		void set_http_response(std::string version, std::string code) {
			status_code_ = strEx::s::stox<int>(code);
		}
		void add_header(std::string key, std::string value) {
			headers_[key] = value;
		}
		void add_header(std::string line) {
			std::string::size_type pos = line.find(':');
			if (pos == std::string::npos)
				add_header(line, "");
			else
				add_header(line.substr(0, pos), line.substr(pos+1));
		}
		void add_default_headers() {
			add_header("Accept", "*/*");
			add_header("Connection", "close");
		}
		void set_payload(std::string data) {
			payload_ = data;
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "verb: " << verb_ << ", path: " << path_;
			BOOST_FOREACH(const header_type::value_type &v, headers_) 
				ss << ", " << v.first << ": " << v.second;
			return ss.str();
		}

		std::string get_header() const {
			std::stringstream ss;
			const char* crlf = "\r\n";
			ss << verb_ << " " << path_ << " HTTP/1.0" << crlf;
			ss << "Host: " << "123.123.123.133" << crlf;
			BOOST_FOREACH(const header_type::value_type &v, headers_)
				ss << v.first << ": " << v.second << crlf;
			ss << crlf;
			return ss.str();
		}
		std::string get_payload() const {
			std::stringstream ss;
			const char* crlf = "\r\n";
			if (!payload_.empty())
				ss << payload_;
			return ss.str();
		}

		std::vector<char> get_packet() const {
			std::vector<char> ret;
			std::string h = get_header();
			ret.insert(ret.end(), h.begin(), h.end());
			std::string p = get_payload();
			ret.insert(ret.end(), p.begin(), p.end());
			return ret;
		}
		
		static packet create_timeout(std::string message) {
			packet p;
			p.status_code_ = 99;
			p.payload_ = message;
			return p;
		}

	};
}