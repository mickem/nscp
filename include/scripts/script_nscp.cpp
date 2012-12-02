#include <boost/foreach.hpp>

#include <utf8.hpp>
#include <nscapi/functions.hpp>

#include <scripts/script_nscp.hpp>




std::list<std::string> scripts::nscp::settings_provider_impl::get_section(std::string section)
{
	std::list<std::string> ret;
	BOOST_FOREACH(std::wstring s, core_->getSettingsSection(utf8::cvt<std::wstring>(section))) {
		ret.push_back(utf8::cvt<std::string>(s));
	}
	return ret;
}

std::string scripts::nscp::settings_provider_impl::get_string(std::string path, std::string key, std::string value)
{
	return utf8::cvt<std::string>(core_->getSettingsString(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), utf8::cvt<std::wstring>(value)));
}

void scripts::nscp::settings_provider_impl::set_string(std::string path, std::string key, std::string value)
{
	core_->SetSettingsString(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), utf8::cvt<std::wstring>(value));
}

bool scripts::nscp::settings_provider_impl::get_bool(std::string path, std::string key, bool value)
{
	return core_->getSettingsInt(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), value?1:0)==1;
}

void scripts::nscp::settings_provider_impl::set_bool(std::string path, std::string key, bool value)
{
	core_->SetSettingsInt(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), value?1:0);
}

int scripts::nscp::settings_provider_impl::get_int(std::string path, std::string key, int value)
{
	return core_->getSettingsInt(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), value);
}

void scripts::nscp::settings_provider_impl::set_int(std::string path, std::string key, int value)
{
	core_->SetSettingsInt(utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), value);
}

void scripts::nscp::settings_provider_impl::register_path(std::string path, std::string title, std::string description, bool advanced)
{
	core_->settings_register_path(plugin_id, utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(title), utf8::cvt<std::wstring>(description),advanced);
}

void scripts::nscp::settings_provider_impl::register_key(std::string path, std::string key, std::string type, std::string title, std::string description, std::string defaultValue)
{
	NSCAPI::settings_type iType = scripts::settings_provider::parse_type(type);
	core_->settings_register_key(plugin_id, utf8::cvt<std::wstring>(path), utf8::cvt<std::wstring>(key), iType, utf8::cvt<std::wstring>(title), utf8::cvt<std::wstring>(description),utf8::cvt<std::wstring>(defaultValue), false);
}

void scripts::nscp::settings_provider_impl::save()
{
	core_->settings_save();
}

void scripts::nscp::nscp_runtime_impl::register_command(const std::string type, const std::string &command, const std::string &description) 
{
	if (type == tags::query_tag || type == tags::simple_query_tag)
		core_->registerCommand(plugin_id, utf8::cvt<std::wstring>(command), utf8::cvt<std::wstring>(description));
}

bool scripts::nscp::core_provider_impl::submit_simple_message(const std::string channel, const std::string command, const NSCAPI::nagiosReturn code, const std::string & message, const std::string & perf, std::string & result)
{
	std::string request, response;
	nscapi::functions::create_simple_submit_request(channel, command, code, message, perf, request);
	bool ret = core_->submit_message(utf8::cvt<std::wstring>(channel), request, response) == NSCAPI::isSuccess;
	nscapi::functions::parse_simple_submit_response(response, result);
	return ret;
}

NSCAPI::nagiosReturn scripts::nscp::core_provider_impl::simple_query(const std::string command, const std::list<std::string> & argument, std::string & msg, std::string & perf)
{
	std::string request, response;
	nscapi::functions::create_simple_query_request(command, argument, request);
	bool ret = core_->query(utf8::cvt<std::wstring>(command), request, response) == NSCAPI::isSuccess;
	nscapi::functions::parse_simple_query_response(response, msg, perf);
	return ret;
}

NSCAPI::nagiosReturn scripts::nscp::core_provider_impl::exec_simple_command(const std::string target, const std::string command, const std::list<std::string> &argument, std::list<std::string> & result)
{
	std::string request, response;
	nscapi::functions::create_simple_exec_request(command, argument, request);
	bool ret = core_->exec_command(utf8::cvt<std::wstring>(target), utf8::cvt<std::wstring>(command), request, response) == NSCAPI::isSuccess;
	nscapi::functions::parse_simple_exec_response(response, result);
	return ret;
}

NSCAPI::nagiosReturn scripts::nscp::core_provider_impl::exec_command(const std::string target, const std::string &request, std::string &response)
{
	return core_->query(utf8::cvt<std::wstring>(target), request, response);
}

NSCAPI::nagiosReturn scripts::nscp::core_provider_impl::query(const std::string target, const std::string &request, std::string &response)
{
	return core_->query(utf8::cvt<std::wstring>(target), request, response);
}

NSCAPI::nagiosReturn scripts::nscp::core_provider_impl::submit(const std::string target, const std::string &request, std::string &response)
{
	return core_->submit_message(utf8::cvt<std::wstring>(target), request, response);
}

NSCAPI::nagiosReturn scripts::nscp::core_provider_impl::reload(const std::string module)
{
	return core_->reload(utf8::cvt<std::wstring>(module));
}

void scripts::nscp::core_provider_impl::log(NSCAPI::log_level::level level, const std::string file, int line, const std::string message)
{
	core_->log(level, utf8::cvt<std::string>(file), line, utf8::cvt<std::wstring>(message));
}

const std::string scripts::nscp::tags::simple_query_tag = "simple:query";
const std::string scripts::nscp::tags::simple_exec_tag = "simple:exec";
const std::string scripts::nscp::tags::simple_submit_tag = "simple:submit";
const std::string scripts::nscp::tags::query_tag = "query";
const std::string scripts::nscp::tags::exec_tag = "exec";
const std::string scripts::nscp::tags::submit_tag = "submit";

