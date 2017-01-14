/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once


#include <str/utils.hpp>

#include <nscapi/nscapi_protobuf.hpp>

#include <boost/asio/buffer.hpp>

#include <sstream>
#include <string>

namespace check_mk {
	class check_mk_exception : public std::exception {
		std::string error_;
	public:
		//		check_mk_exception(std::wstring error) : error_(utf8::cvt<std::string>(error)) {}
		check_mk_exception(std::string error) : error_(error) {}
		virtual ~check_mk_exception() throw() {}
		virtual const char* what() const throw() {
			return error_.c_str();
		}
	};

	struct packet {
		struct section {
			std::string title;
			struct line {
				line() {}
				line(const std::string &data) {
					set_line(data);
				}
				line(const line & other) : items(other.items) {}
				const line& operator=(const line & other) {
					items = other.items;
					return *this;
				}
				std::string to_string() const {
					std::string ret;
					bool first = true;
					BOOST_FOREACH(const std::string &item, items) {
						if (first) {
							ret += item;
							first = false;
						} else {
							ret += " " + item;
						}
					}
					return ret;
				}

				std::string get_item(std::size_t id) {
					if (id >= items.size())
						throw check_mk::check_mk_exception("Invalid line");
					std::list<std::string>::const_iterator cit = items.begin();
					for (std::size_t i = 0; i < id; i++) {
						cit++;
					}
					return *cit;
				}

				std::string get_line() {
					return to_string();
				}

				void set_line(const std::string &data) {
					std::istringstream split(data);
					std::string chunk;
					while (std::getline(split, chunk, ' ')) {
						items.push_back(chunk);
					}
				}

				std::list<std::string> items;
			};
			std::list<line> lines;
			section() {}
			section(std::string title) : title(title) {}
			section(const section & other) : title(other.title), lines(other.lines) {}
			const section& operator=(const section & other) {
				title = other.title;
				lines = other.lines;
				return *this;
			}

			void push(std::string data) {
				lines.push_back(line(data));
			}

			std::string to_string() const {
				std::string ret;
				ret += "<<<" + title + ">>>\n";
				BOOST_FOREACH(const section::line &l, lines) {
					ret += l.to_string() + "\n";
				}
				return ret;
			}

			bool empty() const {
				return title.empty() && lines.empty();
			}

			check_mk::packet::section::line get_line(std::size_t id) {
				if (id >= lines.size())
					throw check_mk::check_mk_exception("Invalid line");
				std::list<line>::const_iterator cit = lines.begin();
				for (std::size_t i = 0; i < id; i++) {
					cit++;
				}
				return *cit;
			}

			void add_line(check_mk::packet::section::line line) {
				lines.push_back(line);
			}
		};
		std::list<section> section_list;

		packet() {}
		packet(const packet & other) : section_list(other.section_list) {}
		const packet& operator=(const packet & other) {
			section_list = other.section_list;
			return *this;
		}

		//////////////////////////////////////////////////////////////////////////
		// Write to string

		std::string write() const {
			std::string ret;
			BOOST_FOREACH(const section &s, section_list) {
				ret += s.to_string();
			}
			return ret;
		}

		//////////////////////////////////////////////////////////////////////////
		// Read from vector (string?)
		void add_section(section s) {
			section_list.push_back(s);
		}

		void read(std::string data) {
			std::istringstream split(data);

			std::string chunk;
			section s;
			while (std::getline(split, chunk)) {
				if (chunk.length() > 6 && chunk.substr(0, 3) == "<<<" && chunk.substr(chunk.length() - 3, 3) == ">>>") {
					if (!s.empty())
						section_list.push_back(s);
					s = section(chunk.substr(3, chunk.length() - 6));
				} else {
					s.push(chunk);
				}
			}
			if (!s.empty())
				section_list.push_back(s);
		}

		std::string to_string() const {
			return write();
		}

		section get_section(std::size_t id) {
			if (id >= section_list.size())
				throw check_mk::check_mk_exception("Invalid section");
			std::list<section>::const_iterator cit = section_list.begin();
			for (std::size_t i = 0; i < id; i++) {
				cit++;
			}
			return *cit;
		}

		std::vector<char> to_vector() {
			std::string s = to_string();
			return std::vector<char>(s.begin(), s.end());
		}
	};
}