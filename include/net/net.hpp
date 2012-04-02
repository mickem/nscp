#pragma once

#include <strEx.h>

namespace net {

	struct string_traits {
		static std::string protocol_suffix() {
			return "://";
		}
		static std::string port_prefix() {
			return ":";
		}
	};
	struct wstring_traits {
		static std::wstring protocol_suffix() {
			return _T("://");
		}
		static std::wstring port_prefix() {
			return _T(":");
		}
	};


	template<class string_type, class stream = std::wstringstream, class traits = wstring_traits>
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
		inline std::wstring get_whost(std::wstring default_host = _T("127.0.0.1")) const {
			if (!host.empty())
				return utf8::cvt<std::wstring>(host);
			return default_host;
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
	typedef base_url<std::wstring, std::wstringstream, wstring_traits> wurl;

	inline wurl url_to_wide(const url &u) {
		wurl r;
		r.protocol = utf8::cvt<std::wstring>(u.protocol);
		r.host = utf8::cvt<std::wstring>(u.host);
		r.path = utf8::cvt<std::wstring>(u.path);
		r.query = utf8::cvt<std::wstring>(u.query);
		r.port = u.port;
		return r;
	}
	inline url wide_to_url(const wurl &u) {
		url r;
		r.protocol = utf8::cvt<std::string>(u.protocol);
		r.host = utf8::cvt<std::string>(u.host);
		r.path = utf8::cvt<std::string>(u.path);
		r.query = utf8::cvt<std::string>(u.query);
		r.port = u.port;
		return r;
	}


	inline wurl parse(const std::wstring& url_s, unsigned int default_port = 0) {
		wurl ret;
		const std::wstring prot_end(_T("://"));
		std::wstring::const_iterator prot_i = std::search(url_s.begin(), url_s.end(), prot_end.begin(), prot_end.end());
		if (prot_i != url_s.end()) {
			ret.protocol.reserve(std::distance(url_s.begin(), prot_i));
			std::transform(url_s.begin(), prot_i, std::back_inserter(ret.protocol), std::ptr_fun<int,int>(std::tolower)); // protocol is icase
			std::advance(prot_i, prot_end.length());
		} else {
			ret.protocol = _T("");
			prot_i = url_s.begin();
		}
		std::wstring k(_T("/:"));
		std::wstring::const_iterator path_i = std::find_first_of(prot_i, url_s.end(), k.begin(), k.end());
		ret.host.reserve(std::distance(prot_i, path_i));
		std::transform(prot_i, path_i, std::back_inserter(ret.host), std::ptr_fun<int,int>(std::tolower)); // host is icase
		if ((path_i != url_s.end()) && (*path_i == L':')) {
			std::wstring::const_iterator port_b = path_i; ++port_b;
			path_i = std::find(path_i, url_s.end(), L'/');
			ret.port = boost::lexical_cast<unsigned int>(std::wstring(port_b, path_i));
		} else {
			ret.port = default_port;
		}
		std::wstring::const_iterator query_i = std::find(path_i, url_s.end(), L'?');
		ret.path.assign(path_i, query_i);
		if( query_i != url_s.end() )
			++query_i;
		ret.query.assign(query_i, url_s.end());
		return ret;
	}
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
		ret.host.reserve(std::distance(prot_i, path_i));
		std::transform(prot_i, path_i, std::back_inserter(ret.host), std::ptr_fun<int,int>(std::tolower)); // host is icase
		if ((path_i != url_s.end()) && (*path_i == ':')) {
			std::string::const_iterator port_b = path_i; ++port_b;
			path_i = std::find(path_i, url_s.end(), '/');
			ret.port = boost::lexical_cast<unsigned int>(std::string(port_b, path_i));
		} else {
			ret.port = default_port;
		}
		std::string::const_iterator query_i = std::find(path_i, url_s.end(), '?');
		ret.path.assign(path_i, query_i);
		if( query_i != url_s.end() )
			++query_i;
		ret.query.assign(query_i, url_s.end());
		return ret;
	}
}