#pragma once

#include <utf8.hpp>
#include <algorithm>

namespace net {

	struct string_traits {
		static std::string protocol_suffix() {
			return "://";
		}
		static std::string port_prefix() {
			return ":";
		}
	};
	template<class string_type, class stream = std::stringstream, class traits = string_traits>
	struct base_url {
		typedef base_url<string_type, stream, traits> my_type;
		string_type protocol;
		string_type host;
		string_type path;
		string_type query;
		unsigned int port;
		base_url() : port(0) {}

		string_type to_string() const {
			stream ss;
			ss << protocol << traits::protocol_suffix() << host;
			if (port != 0)
				ss << traits::port_prefix() << port;
			ss << path;
			return ss.str();
		}

		inline int get_port() const {
			return port;
		}
		inline int get_port(int default_port) const {
			if (port == 0)
				return default_port;
			return port;
		}
		inline std::string get_host(std::string default_host = "127.0.0.1") const {
			if (!host.empty())
				return utf8::cvt<std::string>(host);
			return default_host;
		}
		inline std::string get_port_string(std::string default_port) const {
			if (port != 0)
				return boost::lexical_cast<std::string>(port);
			return default_port;
		}
		inline std::string get_port_string() const {
			return boost::lexical_cast<std::string>(port);
		}

		void import(const base_url &n) {

			if (protocol.empty() && !n.protocol.empty())
				protocol = n.protocol;
			if (host.empty() && !n.host.empty())
				host = n.host;
			if (port == 0 && n.port != 0)
				port = n.port;
			if (path.empty() && !n.path.empty())
				path = n.path;
			if (query.empty() && !n.query.empty())
				query = n.query;
		}
		void apply(const base_url &n) {
			if (!n.protocol.empty())
				protocol = n.protocol;
			if (!n.host.empty())
				host = n.host;
			if (n.port != 0)
				port = n.port;
			if (!n.path.empty())
				path = n.path;
			if (!n.query.empty())
				query = n.query;
		}
	};

	typedef base_url<std::string, std::stringstream, string_traits> url;

	inline url parse(const std::string& url_s, unsigned int default_port = 0) {
		url ret;
		const std::string prot_end("://");
		std::string::const_iterator prot_i = std::search(url_s.begin(), url_s.end(), prot_end.begin(), prot_end.end());
		if (prot_i != url_s.end()) {
			ret.protocol.reserve(std::distance(url_s.begin(), prot_i));
			std::transform(url_s.begin(), prot_i, std::back_inserter(ret.protocol), std::ptr_fun<int,int>(std::tolower)); // protocol is icase
			std::advance(prot_i, prot_end.length());
		} else {
			ret.protocol = "";
			prot_i = url_s.begin();
		}
		std::string k("/:");
		std::string::const_iterator path_i = std::find_first_of(prot_i, url_s.end(), k.begin(), k.end());
		ret.host = std::string(prot_i, path_i);
		//ret.host.reserve(std::distance(prot_i, path_i));
		//std::transform(prot_i, path_i, std::back_inserter(ret.host), std::ptr_fun<int,int>(std::tolower)); // host is icase
		if (ret.protocol != "ini" && ret.protocol != "registry") {
			if ((path_i != url_s.end()) && (*path_i == ':')) {
				std::string::const_iterator port_b = path_i; ++port_b;
				std::string::const_iterator tmp = std::find(path_i, url_s.end(), '/');
				std::string chunk = std::string(port_b, tmp);
				if (!chunk.empty() && chunk.find_first_not_of("0123456789") == std::string::npos) {
					ret.port = boost::lexical_cast<unsigned int>(chunk);
					path_i = tmp;
				}
			} else {
				ret.port = default_port;
			}
		}
		std::string::const_iterator query_i = std::find(path_i, url_s.end(), '?');
		ret.path.assign(path_i, query_i);
		if( query_i != url_s.end() )
			++query_i;
		ret.query.assign(query_i, url_s.end());
		return ret;
	}
}
