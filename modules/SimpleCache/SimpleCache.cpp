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

#include "stdafx.h"

#include <map>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/optional.hpp>

#include <time.h>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <parsers/expression/expression.hpp>

#include <settings/client/settings_client.hpp>

#include "SimpleCache.h"

namespace sh = nscapi::settings_helper;

Cache::Cache() {
}
Cache::~Cache() {
}

bool Cache::loadModule() {
	return false;
}
struct simple_string_functor {
	std::string value;
	simple_string_functor(std::string value) : value(value) {}
	simple_string_functor(const simple_string_functor &other) : value(other.value) {}
	const simple_string_functor& operator=(const simple_string_functor &other) {
		value = other.value;
		return *this;
	}
	std::string operator() (const std::string channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage::Response &payload) {
		return value;
	}
	std::string operator() (const Cache::cache_query &query) {
		return value;
	}
};
struct header_host_functor {
	std::string operator() (const std::string channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage::Response &payload) {
		std::string sender = hdr.sender_id();
		for (int i=0;i<hdr.hosts_size();i++) {
			if (hdr.hosts(i).id() == sender)
				return hdr.hosts(i).host();
		}
		return "";
	}
	std::string operator() (const Cache::cache_query &query) {
		return query.host;
	}
};
struct payload_command_functor {
	std::string operator() (const std::string channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage::Response &payload) {
		return payload.command();
	}
	std::string operator() (const Cache::cache_query &query) {
		return query.command;
	}
};
struct channel_functor {
	std::string operator() (const std::string channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage::Response &payload) {
		return channel;
	}
	std::string operator() (const Cache::cache_query &query) {
		return query.channel;
	}
};
struct payload_alias_functor {
	std::string operator() (const std::string channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage::Response &payload) {
		return payload.alias();
	}
	std::string operator() (const Cache::cache_query &query) {
		return query.alias;
	}
};
struct payload_alias_or_command_functor {
	std::string operator() (const std::string channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage::Response &payload) {
		if (payload.has_alias())
			return payload.alias();
		return payload.command();
	}
	std::string operator() (const Cache::cache_query &query) {
		if (!query.alias.empty())
			return query.alias;
		return query.command;
	}
};

std::string simple_string_fun(std::string key) {
	return key;
}
bool Cache::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	std::string primary_key;
	std::wstring channel;
	try {
		register_command(_T("cache"), _T("Check for cached state"));
		register_command(_T("check_cache"), _T("Check for cached state"));

		sh::settings_registry settings(get_settings_proxy());
		
		settings.set_alias(alias, _T("cache"));
		
		settings.alias().add_path_to_settings()
			(_T("CACHE"), _T("Section for simple cache module (SimpleCache.dll)."))

			;

		settings.alias().add_key_to_settings()
			(_T("primary index"), sh::string_key(&primary_key, "${alias-or-command}"),
			_T("PRIMARY CACHE INDEX"), _T("Set this to the value you want to use as unique key for the cache (host, command, result,...)."))

			(_T("channel"), sh::wstring_key(&channel, _T("CACHE")),
			_T("CHANNEL"), _T("The channel to listen to."))

			;

		settings.register_all();
		settings.notify();

		get_core()->registerSubmissionListener(get_id(), channel);

		parsers::simple_expression parser;
		parsers::simple_expression::result_type result;
		if (!parser.parse(primary_key, result)) {
			NSC_LOG_ERROR_STD(_T("Failed to parse primary key: ") + utf8::cvt<std::wstring>(primary_key))
		}
		BOOST_FOREACH(parsers::simple_expression::entry &e, result) {
			if (!e.is_variable) {
				index_lookup_.push_back(simple_string_functor(e.name));
				command_lookup_.push_back(simple_string_functor(e.name));
			} else if (e.name == "command") {
				index_lookup_.push_back(payload_command_functor());
				command_lookup_.push_back(payload_command_functor());
			} else if (e.name == "host") {
				index_lookup_.push_back(header_host_functor());
				command_lookup_.push_back(header_host_functor());
			} else if (e.name == "channel") {
				index_lookup_.push_back(channel_functor());
				command_lookup_.push_back(channel_functor());
			} else if (e.name == "alias") {
				index_lookup_.push_back(payload_alias_functor());
				command_lookup_.push_back(payload_alias_functor());
			} else if (e.name == "alias-or-command") {
				index_lookup_.push_back(payload_alias_or_command_functor());
				command_lookup_.push_back(payload_alias_or_command_functor());
			} else {
				NSC_LOG_ERROR_STD(_T("Invalid index: ") + utf8::cvt<std::wstring>(e.name));
			}
		}
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
	}
	return true;
}
bool Cache::unloadModule() {
	return true;
}

bool Cache::hasCommandHandler() {
	return true;
}
bool Cache::hasMessageHandler() {
	return false;
}

NSCAPI::nagiosReturn Cache::handleRAWNotification(const wchar_t* channel_w, std::string request, std::string &reply) {
	NSCAPI::nagiosReturn ret = NSCAPI::isSuccess;
	std::string channel = utf8::cvt<std::string>(channel_w);
	Plugin::SubmitRequestMessage request_msg;
	Plugin::SubmitResponseMessage response_msg;
	request_msg.ParseFromString(request);
	nscapi::functions::make_return_header(response_msg.mutable_header(), request_msg.header());
	BOOST_FOREACH(const Plugin::QueryResponseMessage::Response &payload ,request_msg.payload()) {
		std::string key;
		BOOST_FOREACH(index_lookup_function &f, index_lookup_) {
			key += f(channel, request_msg.header(), payload);
		}
		std::string data = payload.SerializeAsString();
		NSC_DEBUG_MSG(_T("Adding to index: ") + utf8::cvt<std::wstring>(key));
		{
			boost::unique_lock<boost::shared_mutex> lock(cache_mutex_);
			if (!lock) {
				nscapi::functions::append_simple_submit_response_payload(response_msg.add_payload(), payload.command(), NSCAPI::hasFailed, "Failed to get lock");
				ret = NSCAPI::hasFailed;
				continue;
			}
			cache_[key] = data;
		}
		nscapi::functions::append_simple_submit_response_payload(response_msg.add_payload(), payload.command(), NSCAPI::isSuccess, "message has been cached");
	}
	response_msg.SerializeToString(&reply);
	return NSCAPI::isSuccess;
}

NSCAPI::nagiosReturn Cache::handleRAWCommand(const wchar_t *command, const std::string &request, std::string &reply) {
	NSCAPI::nagiosReturn ret = NSCAPI::returnOK;
	std::wstring cmd = command;
	cache_query query;
	if (cmd != _T("checkcache") && cmd != _T("check_cache"))
		return NSCAPI::returnIgnored;

	Plugin::QueryRequestMessage request_msg;
	Plugin::QueryResponseMessage response_msg;
	request_msg.ParseFromString(request);
	nscapi::functions::make_return_header(response_msg.mutable_header(), request_msg.header());
	std::string not_found_msg = "Entry not found";
	NSCAPI::nagiosReturn not_found_code = NSCAPI::returnUNKNOWN;
	BOOST_FOREACH(const Plugin::QueryRequestMessage::Request &payload ,request_msg.payload()) {
		std::string key;
		try {
			BOOST_FOREACH(const std::string &arg, payload.arguments()) {
				std::pair<std::string,std::string> a = strEx::split(arg, std::string("="));
				if (a.first == "host") query.host = a.second;
				else if (a.first == "index") key = a.second;
				else if (a.first == "command") query.command = a.second;
				else if (a.first == "channel") query.channel = a.second;
				else if (a.first == "alias") query.alias = a.second;
				else if (a.first == "not-found-msg") not_found_msg = a.second;
				else if (a.first == "not-found-code") not_found_code = nscapi::plugin_helper::translateReturn(a.second);
				else {
					throw std::string("invalid syntax: ") + a.first;
				}
			}
		} catch (const std::string &error) {
			nscapi::functions::append_simple_query_response_payload(response_msg.add_payload(), payload.command(), NSCAPI::returnUNKNOWN, std::string("Invalid syntax: ") + error);
			continue;
		} catch (...) {
			nscapi::functions::append_simple_query_response_payload(response_msg.add_payload(), payload.command(), NSCAPI::returnUNKNOWN, "Invalid syntax");
			continue;
		}
		if (key.empty()) {
			BOOST_FOREACH(command_lookup_function &f, command_lookup_) {
				key += f(query);
			}
		}
		NSC_DEBUG_MSG(_T("Searching for index: ") + utf8::cvt<std::wstring>(key));
		boost::optional<std::string> data;
		{

			boost::shared_lock<boost::shared_mutex> lock(cache_mutex_);
			if (!lock) {
				nscapi::functions::append_simple_query_response_payload(response_msg.add_payload(), payload.command(), NSCAPI::returnUNKNOWN, "Failed to get lock");
				ret = NSCAPI::returnUNKNOWN;
				continue;
			}
			std::map<std::string,std::string>::const_iterator cit = cache_.find(key);
			if (cit != cache_.end())
				data = cit->second;
		}
		if (data) {
			::Plugin::QueryResponseMessage::Response* rsp = response_msg.add_payload();
			rsp->ParseFromString(*data);
			ret = nscapi::functions::gbp_to_nagios_status(rsp->result());
		} else {
			nscapi::functions::append_simple_query_response_payload(response_msg.add_payload(), payload.command(), not_found_code, not_found_msg);
			ret = NSCAPI::returnUNKNOWN;
		}
	}
	response_msg.SerializeToString(&reply);
	return ret;
}
NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(Cache)
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_HANDLE_CMD_DEF()
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF()
