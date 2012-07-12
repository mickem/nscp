#include <nscapi/nscapi_plugin_impl.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>

#include <iostream>

#include <protobuf/plugin.pb.h>

#include <format.hpp>

extern nscapi::helper_singleton* plugin_singleton;

nscapi::core_wrapper* nscapi::impl::simple_plugin::get_core() {
	return plugin_singleton->get_core();
}


void nscapi::impl::simple_plugin::register_command(std::wstring command, std::wstring description, std::list<std::wstring> aliases) {
	BOOST_FOREACH(const std::wstring alias, aliases) {
		register_command(alias, description);
	}
	register_command(command, description);
}

void nscapi::impl::simple_log_handler::handleMessageRAW(std::string data) {
	try {
		Plugin::LogEntry message;
		message.ParseFromString(data);

		for (int i=0;i<message.entry_size();i++) {
			Plugin::LogEntry::Entry msg = message.entry(i);
			handleMessage(msg.level(), msg.file(), msg.line(), msg.message());
		}
	} catch (std::exception &e) {
		std::cout << "Failed to parse data from: " << format::strip_ctrl_chars(data) << e.what() <<  std::endl;;
	} catch (...) {
		std::cout << "Failed to parse data from: " << format::strip_ctrl_chars(data) << std::endl;;
	}
}

NSCAPI::nagiosReturn nscapi::impl::simple_command_handler::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response) {
	nscapi::protobuf::types::decoded_simple_command_data data = nscapi::functions::parse_simple_query_request(char_command, request);
	std::wstring msg, perf;

	NSCAPI::nagiosReturn ret = handleCommand(data.target, boost::algorithm::to_lower_copy(data.command), data.args, msg, perf);
	nscapi::functions::create_simple_query_response(data.command, ret, msg, perf, response);
	return ret;
}



NSCAPI::nagiosReturn nscapi::impl::simple_command_line_exec::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response) {
	nscapi::protobuf::types::decoded_simple_command_data data = nscapi::functions::parse_simple_exec_request(char_command, request);
	std::wstring result;
	NSCAPI::nagiosReturn ret = commandLineExec(data.command, data.args, result);
	if (ret == NSCAPI::returnIgnored)
		return NSCAPI::returnIgnored;
	nscapi::functions::create_simple_exec_response(data.command, ret, result, response);
	return ret;
}

NSCAPI::nagiosReturn nscapi::impl::simple_submission_handler::handleRAWNotification(const wchar_t* channel, std::string request, std::string &response) {
	try {
		std::wstring source, command, msg, perf;
		int code = nscapi::functions::parse_simple_submit_request(request, source, command, msg, perf);
		NSCAPI::nagiosReturn ret = handleSimpleNotification(channel, source, command, code, msg, perf);
		if (ret == NSCAPI::returnIgnored)
			return NSCAPI::returnIgnored;
		nscapi::functions::create_simple_submit_response(channel, command, ret, _T(""), response);
	} catch (std::exception &e) {
		nscapi::plugin_singleton->get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, utf8::cvt<std::wstring>("Failed to parse data from: " + format::strip_ctrl_chars(request) + ": " + e.what()));
	} catch (...) {
		nscapi::plugin_singleton->get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, utf8::cvt<std::wstring>("Failed to parse data from: " + format::strip_ctrl_chars(request)));
	}
	return NSCAPI::returnIgnored;
}