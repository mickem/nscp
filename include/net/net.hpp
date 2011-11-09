#pragma once

#include <strEx.h>

namespace net {
	struct url {
		std::string protocol;
		std::string host;
		std::string path;
		std::string query;
		unsigned int port;
		std::string get_port() {
			std::stringstream ss;
			ss << port;
			return ss.str();
		}
		std::string to_string() {
			std::stringstream ss;
			ss << protocol << "://" << host << ":" << port << path;
			return ss.str();
		}
	};
	struct wurl {
		std::wstring protocol;
		std::wstring host;
		std::wstring path;
		std::wstring query;
		unsigned int port;
		std::wstring to_string() {
			std::wstringstream ss;
			ss << protocol << _T("://") << host << _T(":") << port << path;
			return ss.str();
		}
		inline url get_url() {
			url r;
			r.protocol = utf8::cvt<std::string>(protocol);
			r.host = utf8::cvt<std::string>(host);
			r.path = utf8::cvt<std::string>(path);
			r.query = utf8::cvt<std::string>(query);
			r.port = port;
			return r;
		}

	};
	inline wurl parse(const std::wstring& url_s, unsigned int default_port = 80) {
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
	inline url parse(const std::string& url_s, unsigned int default_port = 80) {
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