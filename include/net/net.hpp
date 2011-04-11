#pragma once

#include <strEx.h>

namespace net {
	struct url {
		std::string protocol;
		std::string host;
		std::string path;
		std::string query;
		std::string to_string() {
			return protocol + "://" + host + "/" + path;
		}
	};
	struct wurl {
		std::wstring protocol;
		std::wstring host;
		std::wstring path;
		std::wstring query;
		std::wstring to_string() {
			return protocol + _T("://") + host + _T("/") + path;
		}
		inline url get_url() {
			url r;
			r.protocol = utf8::cvt<std::string>(protocol);
			r.host = utf8::cvt<std::string>(host);
			r.path = utf8::cvt<std::string>(path);
			r.query = utf8::cvt<std::string>(query);
			return r;
		}

	};
	inline wurl parse(const std::wstring& url_s) {
		wurl ret;
		const std::wstring prot_end(_T("://"));
		std::wstring::const_iterator prot_i = std::search(url_s.begin(), url_s.end(), prot_end.begin(), prot_end.end());
		ret.protocol.reserve(std::distance(url_s.begin(), prot_i));
		std::transform(url_s.begin(), prot_i, std::back_inserter(ret.protocol), std::ptr_fun<int,int>(std::tolower)); // protocol is icase
		if( prot_i == url_s.end() )
			return ret;
		std::advance(prot_i, prot_end.length());
		std::wstring::const_iterator path_i = std::find(prot_i, url_s.end(), L'/');
		ret.host.reserve(std::distance(prot_i, path_i));
		std::transform(prot_i, path_i, std::back_inserter(ret.host), std::ptr_fun<int,int>(std::tolower)); // host is icase
		std::wstring::const_iterator query_i = std::find(path_i, url_s.end(), L'?');
		ret.path.assign(path_i, query_i);
		if( query_i != url_s.end() )
			++query_i;
		ret.query.assign(query_i, url_s.end());
		return ret;
	}
}