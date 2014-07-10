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

#include <map>
#include <vector>
#include <ostream>
#include <fstream>

#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/optional.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>
#include <nscapi/nscapi_helper.hpp>

#include <parsers/expression/expression.hpp>

#include <settings/client/settings_client.hpp>

#include "SimpleFileWriter.h"

namespace sh = nscapi::settings_helper;

struct simple_string_functor {
	std::string value;
	simple_string_functor(std::string value) : value(value) {}
	simple_string_functor(const simple_string_functor &other) : value(other.value) {}
	const simple_string_functor& operator=(const simple_string_functor &other) {
		value = other.value;
		return *this;
	}
	std::string operator() (const std::string, const Plugin::Common::Header &, const Plugin::QueryResponseMessage::Response &) {
		return value;
	}
};
struct header_host_functor {
	std::string operator() (const std::string channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage::Response &) {
		std::string sender = hdr.sender_id();
		for (int i=0;i<hdr.hosts_size();i++) {
			if (hdr.hosts(i).id() == sender)
				return hdr.hosts(i).host();
		}
		return "";
	}
};
struct payload_command_functor {
	std::string operator() (const std::string, const Plugin::Common::Header &, const Plugin::QueryResponseMessage::Response &payload) {
		return payload.command();
	}
};
struct channel_functor {
	std::string operator() (const std::string channel, const Plugin::Common::Header &, const Plugin::QueryResponseMessage::Response &) {
		return channel;
	}
};
struct payload_alias_functor {
	std::string operator() (const std::string, const Plugin::Common::Header &, const Plugin::QueryResponseMessage::Response &payload) {
		return payload.alias();
	}
};
struct payload_message_functor {
	std::string operator() (const std::string, const Plugin::Common::Header &, const Plugin::QueryResponseMessage::Response &payload) {
		return payload.message();
	}
};
struct payload_result_functor {
	std::string operator() (const std::string, const Plugin::Common::Header &, const Plugin::QueryResponseMessage::Response &payload) {
		return nscapi::plugin_helper::translateReturn(nscapi::protobuf::functions::gbp_to_nagios_status(payload.result()));
	}
};
struct payload_alias_or_command_functor {
	std::string operator() (const std::string, const Plugin::Common::Header &, const Plugin::QueryResponseMessage::Response &payload) {
		if (payload.has_alias())
			return payload.alias();
		return payload.command();
	}
};

std::string simple_string_fun(std::string key) {
	return key;
}
bool SimpleFileWriter::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	std::string primary_key;
	std::string channel;
	try {
		sh::settings_registry settings(get_settings_proxy());
		
		settings.set_alias(alias, "writers/file");
		
		settings.alias().add_path_to_settings()
			("FILE WRITER", "Section for simple file writer module (SimpleFileWriter.dll).")

			;

		settings.alias().add_key_to_settings()
			("syntax", sh::string_key(&primary_key, "${alias-or-command} ${result} ${message}"),
			"MESSAGE SYNTAX", "The syntax of the message to write to the line.\nCan be any arbitrary string as well as include any of the following special keywords:"
			"${command} = The command name, ${host} the host, ${channel} the recieving channel, ${alias} the alias for the command, ${alias-or-command} = alias if set otherweise command, ${message} = the message data (no escape), ${result} = The result status (number).")

			("file", sh::path_key(&filename_, "output.txt"),
			"FILE TO WRITE TO", "The filename to write output to.")

			("channel", sh::string_key(&channel, "FILE"),
			"CHANNEL", "The channel to listen to.")

			;

		settings.register_all();
		settings.notify();

		nscapi::core_helper::core_proxy core(get_core(), get_id());
		core.register_channel(channel);

		parsers::simple_expression parser;
		parsers::simple_expression::result_type result;
		if (!parser.parse(primary_key, result)) {
			NSC_LOG_ERROR_STD("Failed to parse primary key: " + primary_key)
		}
		BOOST_FOREACH(parsers::simple_expression::entry &e, result) {
			if (!e.is_variable) {
				index_lookup_.push_back(simple_string_functor(e.name));
			} else if (e.name == "command") {
				index_lookup_.push_back(payload_command_functor());
			} else if (e.name == "host") {
				index_lookup_.push_back(header_host_functor());
			} else if (e.name == "channel") {
				index_lookup_.push_back(channel_functor());
			} else if (e.name == "alias") {
				index_lookup_.push_back(payload_alias_functor());
			} else if (e.name == "alias-or-command") {
				index_lookup_.push_back(payload_alias_or_command_functor());
			} else if (e.name == "message") {
				index_lookup_.push_back(payload_message_functor());
			} else if (e.name == "result") {
				index_lookup_.push_back(payload_result_functor());
			} else {
				NSC_LOG_ERROR_STD("Invalid index: " + e.name);
			}
		}
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("Failed to register command: ", e);
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("load", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("load");
		return false;
	}
	return true;
}

void SimpleFileWriter::handleNotification(const std::string &, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message) {
	std::string key;
	BOOST_FOREACH(index_lookup_function &f, index_lookup_) {
		key += f(request.command(), request_message.header(), request);
	}
	std::string data = request.SerializeAsString();
	{
		boost::unique_lock<boost::shared_mutex> lock(cache_mutex_);
		if (!lock) {
			nscapi::protobuf::functions::append_simple_submit_response_payload(response, request.command(), NSCAPI::hasFailed, "Failed to get lock");
			return;
		}
		std::ofstream out;
		out.open(filename_.c_str(), std::ios::out|std::ios::app);
		out << key << std::endl;
	}
	nscapi::protobuf::functions::append_simple_submit_response_payload(response, request.command(), NSCAPI::isSuccess, "message has been written");
}
