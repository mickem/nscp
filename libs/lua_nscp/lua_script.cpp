#include <map>

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/thread.hpp>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <scripts/script_nscp.hpp>

#include <lua/lua_cpp.hpp>
#include <lua/lua_script.hpp>

const std::string lua::lua_traits::user_data_tag = "nscp.userdata.info";

//////////////////////////////////////////////////////////////////////////
// Core Wrapper
lua::core_wrapper::core_wrapper(lua_State *L, bool) : isExisting(false) {
	lua::lua_wrapper instance(L);
	info = instance.get_userdata<script_information*>(lua::lua_traits::user_data_tag);
}

int lua::core_wrapper::create_pb_query(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	try {
		std::list<std::string> arguments;
		int arg_count = lua_instance.size();
		if (arg_count < 2)
			return lua_instance.error("Incorrect syntax: create_pb_query(command, args)");
		if (lua_instance.is_table()) {
			std::list<std::string> table = lua_instance.pop_array();
			arguments.insert(arguments.begin(), table.begin(), table.end());
		} else {
			arguments.push_front(lua_instance.pop_string());
		}
		std::string command = lua_instance.pop_string();
		std::string buffer;
		nscapi::protobuf::functions::create_simple_query_request(command, arguments, buffer);
		lua_instance.push_raw_string(buffer);
		return 1;
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
}

int lua::core_wrapper::simple_query(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	try {
		std::list<std::string> arguments;
		int arg_count = lua_instance.size();
		if (arg_count < 2)
			return lua_instance.error("Incorrect syntax: simple_query(command, args)");
		if (lua_instance.is_table()) {
			std::list<std::string> table = lua_instance.pop_array();
			arguments.insert(arguments.begin(), table.begin(), table.end());
		} else {
			arguments.push_front(lua_instance.pop_string());
		}
		std::string command = lua_instance.pop_string();
		std::string message;
		std::string perf;
		NSCAPI::nagiosReturn ret = get()->simple_query(command, arguments, message, perf);
		lua_instance.push_code(ret);
		lua_instance.push_string(message);
		lua_instance.push_string(perf);
		return lua_instance.size();
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
}
int lua::core_wrapper::query(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	try {
		if (lua_instance.size() < 1)
			return lua_instance.error("Incorrect syntax: query(data)");
		std::string data = lua_instance.pop_string();
		std::string response;
		lua_instance.push_boolean(get()->query(data, response));
		lua_instance.push_raw_string(response);
		return 2;
	} catch (...) {
		return lua_instance.error("Unknown exception in: simple_query");
	}
}
int lua::core_wrapper::simple_exec(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	try {
		if (lua_instance.size() < 3)
			return lua_instance.error("Incorrect syntax: simple_exec(target, command, arguments)");
		std::list<std::string> arguments = lua_instance.pop_array();
		std::string command = lua_instance.pop_string();
		std::string target = lua_instance.pop_string();
		std::list<std::string> result;
		NSCAPI::nagiosReturn ret = get()->exec_simple_command(target, command, arguments, result);
		lua_instance.push_code(ret);
		lua_instance.push_array(result);
		return lua_instance.size();
	} catch (...) {
		return lua_instance.error("Unknown exception in: simple_query");
	}
}
int lua::core_wrapper::exec(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	NSC_LOG_ERROR_STD("Unsupported API called: exec");
	return lua_instance.error("Unsupported API called: exec");
}
int lua::core_wrapper::simple_submit(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	try {
		if (lua_instance.size() < 5)
			return lua_instance.error("Incorrect syntax: simple_submit(channel, command, code, message, perf)");
		std::string perf = lua_instance.pop_string();
		std::string message = lua_instance.pop_string();
		NSCAPI::nagiosReturn code = lua_instance.pop_code();
		std::string command = lua_instance.pop_string();
		std::string channel = lua_instance.pop_string();
		std::string result;
		NSCAPI::nagiosReturn ret = get()->submit_simple_message(channel, command, code, message, perf, result);
		lua_instance.push_code(ret);
		lua_instance.push_string(result);
		return lua_instance.size();
	} catch (...) {
		return lua_instance.error("Unknown exception in: simple_query");
	}
}
int lua::core_wrapper::submit(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	NSC_LOG_ERROR_STD("Unsupported API called: submit");
	return lua_instance.error("Unsupported API called: submit");
}
int lua::core_wrapper::reload(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	if (lua_instance.size() < 1)
		return lua_instance.error("Incorrect syntax: reload([<module>]);");
	std::string module = "module";
	get()->reload(lua_instance.pop_string());
	return 0;
}
int lua::core_wrapper::log(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	// log([level], message)
	if (lua_instance.size() < 2)
		return lua_instance.error("Incorrect syntax: log(<level>, <message>);");
	std::string message = lua_instance.pop_string();
	std::string level = lua_instance.pop_string();
	get()->log(nscapi::logging::parse(level), __FILE__, __LINE__, message);
	return 0;
}
boost::shared_ptr<lua::core_provider> lua::core_wrapper::get() {
	return info->get_core_provider();
}

const char lua::core_wrapper::className[] = "Core";
const Luna<lua::core_wrapper>::FunctionType lua::core_wrapper::Functions[] = {
	{ "create_pb_query", &lua::core_wrapper::create_pb_query },
	{ "simple_query", &lua::core_wrapper::simple_query },
	{ "query", &lua::core_wrapper::query },
	{ "simple_exec", &lua::core_wrapper::simple_exec },
	{ "exec", &lua::core_wrapper::exec },
	{ "simple_submit", &lua::core_wrapper::simple_submit },
	{ "submit", &lua::core_wrapper::submit },
	{ "reload", &lua::core_wrapper::reload },
	{ "log", &lua::core_wrapper::log },
	{ 0 }
};
const Luna<lua::core_wrapper>::PropertyType lua::core_wrapper::Properties[] = { {0} };

//////////////////////////////////////////////////////////////////////////
// Registry wrapper

lua::registry_wrapper::registry_wrapper(lua_State *L, bool) : isExisting(false) {
	lua::lua_wrapper instance(L);
	info = instance.get_userdata<script_information*>(lua::lua_traits::user_data_tag);
}

boost::optional<int> read_registration(lua::lua_wrapper &lua_instance, std::string &command, lua::lua_traits::function &fun, std::string &description) {
	// ...(name, function, description)
	// ...(name, instance, function, description)
	std::string funname;
	int count = lua_instance.size();
	if (count < 3)
		return lua_instance.error("Incorrect syntax: ...(name, [instance], function, description);");
	if (!lua_instance.pop_string(description))
		return lua_instance.error("Invalid description");
	if (lua_instance.pop_string(funname))
		lua_instance.getglobal(funname);
	if (!lua_instance.pop_function_ref(fun.function_ref))
		return lua_instance.error("Invalid function");
	if (!lua_instance.is_string()) {
		if (!lua_instance.pop_instance_ref(fun.object_ref))
			return lua_instance.error("Invalid object");
	}
	if (!lua_instance.pop_string(command))
		return lua_instance.error("Invalid command");
	return boost::optional<int>();
}
int lua::registry_wrapper::register_function(lua_State *L) {
	// void = (cmd, function, desc)
	std::string command, description;
	lua::lua_traits::function fundata;
	lua_wrapper lua_instance(L);
	boost::optional<int> error = read_registration(lua_instance, command, fundata, description);
	if (error)
		return *error;

	if (description.empty())
		description = "Lua script: " + command;
	info->register_command(scripts::nscp::tags::query_tag, command, description, fundata);
	return lua_instance.size();
}
int lua::registry_wrapper::register_simple_function(lua_State *L) {
	// void = (cmd, function, desc)
	std::string command, description;
	lua::lua_traits::function fundata;
	lua_wrapper lua_instance(L);
	boost::optional<int> error = read_registration(lua_instance, command, fundata, description);
	if (error)
		return *error;

	if (description.empty())
		description = "Lua script: " + command;
	info->register_command(scripts::nscp::tags::simple_query_tag, command, description, fundata);
	return lua_instance.size();
}
int lua::registry_wrapper::register_cmdline(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	NSC_LOG_ERROR_STD("Unsupported API called: exec");
	return lua_instance.error("Unsupported API called: exec");
}
int lua::registry_wrapper::register_simple_cmdline(lua_State *L) {
	std::string command, description;
	lua::lua_traits::function fundata;
	lua_wrapper lua_instance(L);
	boost::optional<int> error = read_registration(lua_instance, command, fundata, description);
	if (error)
		return *error;
	info->register_command(scripts::nscp::tags::simple_exec_tag, command, description, fundata);
	return lua_instance.size();
}
int lua::registry_wrapper::subscription(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	NSC_LOG_ERROR_STD("Unsupported API called: exec");
	return lua_instance.error("Unsupported API called: exec");
}
int lua::registry_wrapper::simple_subscription(lua_State *L) {
	std::string command, description;
	lua::lua_traits::function fundata;
	lua_wrapper lua_instance(L);
	boost::optional<int> error = read_registration(lua_instance, command, fundata, description);
	if (error)
		return *error;
	info->register_command("simple_submit", command, description, fundata);
	return lua_instance.size();
}

const char lua::registry_wrapper::className[] = "Registry";
const Luna<lua::registry_wrapper>::FunctionType lua::registry_wrapper::Functions[] = {
	{ "query", &lua::registry_wrapper::register_function },
	{ "simple_query", &lua::registry_wrapper::register_simple_function },
	{ "cmdline", &lua::registry_wrapper::register_cmdline },
	{ "simple_cmdline", &lua::registry_wrapper::register_simple_cmdline },
	{ "subscription", &lua::registry_wrapper::subscription },
	{ "simple_subscription", &lua::registry_wrapper::simple_subscription },
	{ 0 }
};
const Luna<lua::registry_wrapper>::PropertyType lua::registry_wrapper::Properties[] = { {0} };

//////////////////////////////////////////////////////////////////////////
// Settings

lua::settings_wrapper::settings_wrapper(lua_State *L, bool) : info(NULL), isExisting(false) {
	lua::lua_wrapper instance(L);
	info = instance.get_userdata<script_information*>(lua::lua_traits::user_data_tag);
}

int lua::settings_wrapper::get_section(lua_State *L) {
	lua_wrapper lua_instance(L);
	if (info == NULL)
		return lua_instance.error("Invalid core");
	if (lua_instance.size() < 1)
		return lua_instance.error("Invalid syntax: get_section([section])");

	std::string v = lua_instance.pop_string();
	try {
		lua_instance.push_array(get()->get_section(v));
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
	return lua_instance.size();
}
int lua::settings_wrapper::get_string(lua_State *L) {
	lua_wrapper lua_instance(L);
	if (info == NULL)
		return lua_instance.error("Invalid core");
	if (lua_instance.size() < 3)
		return lua_instance.error("Invalid syntax: get_string(section, key, value)");
	std::string v = lua_instance.pop_string();
	std::string k = lua_instance.pop_string();
	std::string s = lua_instance.pop_string();
	try {
		lua_instance.push_string(get()->get_string(s, k, v));
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
	return lua_instance.size();
}
int lua::settings_wrapper::set_string(lua_State *L) {
	lua_wrapper lua_instance(L);
	if (info == NULL)
		return lua_instance.error("Invalid core");
	if (lua_instance.size() < 3)
		return lua_instance.error("Invalid syntax: set_string(section, key, value)");
	std::string v = lua_instance.pop_string();
	std::string k = lua_instance.pop_string();
	std::string s = lua_instance.pop_string();
	try {
		get()->set_string(s, k, v);
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
	return lua_instance.size();
}
int lua::settings_wrapper::get_bool(lua_State *L) {
	lua_wrapper lua_instance(L);
	if (info == NULL)
		return lua_instance.error("Invalid core");
	if (lua_instance.size() < 3)
		return lua_instance.error("Invalid syntax: get_bool(section, key, [value])");
	bool v = lua_instance.pop_boolean();
	std::string k = lua_instance.pop_string();
	std::string s = lua_instance.pop_string();
	try {
		lua_instance.push_boolean(get()->get_int(s, k, v ? 1 : 0) == 1);
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
	return lua_instance.size();
}
int lua::settings_wrapper::set_bool(lua_State *L) {
	lua_wrapper lua_instance(L);
	if (info == NULL)
		return lua_instance.error("Invalid core");
	if (lua_instance.size() < 3)
		return lua_instance.error("Invalid syntax: set_bool(section, key, value)");
	bool v = lua_instance.pop_boolean();
	std::string k = lua_instance.pop_string();
	std::string s = lua_instance.pop_string();
	try {
		get()->set_int(s, k, v ? 1 : 0);
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
	return lua_instance.size();
}
int lua::settings_wrapper::get_int(lua_State *L) {
	lua_wrapper lua_instance(L);
	if (info == NULL)
		return lua_instance.error("Invalid core");
	if (lua_instance.size() < 3)
		return lua_instance.error("Invalid syntax: get_int(section, key, [value])");
	int v = lua_instance.pop_int();
	std::string k = lua_instance.pop_string();
	std::string s = lua_instance.pop_string();
	try {
		lua_instance.push_int(get()->get_int(s, k, v));
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
	return lua_instance.size();
}
int lua::settings_wrapper::set_int(lua_State *L) {
	lua_wrapper lua_instance(L);
	if (info == NULL)
		return lua_instance.error("Invalid core");
	if (lua_instance.size() < 3)
		return lua_instance.error("Invalid syntax: set_int(section, key, value)");
	int v = lua_instance.pop_int();
	std::string k = lua_instance.pop_string();
	std::string s = lua_instance.pop_string();
	try {
		get()->set_int(s, k, v);
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
	return lua_instance.size();
}
int lua::settings_wrapper::save(lua_State *L) {
	lua_wrapper lua_instance(L);
	if (info == NULL)
		return lua_instance.error("Invalid core");
	try {
		get()->save();
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
	return lua_instance.size();
}
int lua::settings_wrapper::register_path(lua_State *L) {
	lua_wrapper lua_instance(L);
	if (lua_instance.size() < 3)
		return lua_instance.error("Invalid syntax: register_path(path, title, description)");
	std::string description = lua_instance.pop_string();
	std::string title = lua_instance.pop_string();
	std::string path = lua_instance.pop_string();
	try {
		get()->register_path(path, title, description, false);
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
	return lua_instance.size();
}

int lua::settings_wrapper::register_key(lua_State *L) {
	lua_wrapper lua_instance(L);

	if (lua_instance.size() < 5)
		return lua_instance.error("Invalid syntax: register_key(path, key, type, title, description, default)");
	std::string defaultValue = lua_instance.pop_string();
	std::string description = lua_instance.pop_string();
	std::string title = lua_instance.pop_string();
	std::string type = lua_instance.pop_string();
	std::string key = lua_instance.pop_string();
	std::string path = lua_instance.pop_string();
	try {
		get()->register_key(path, key, type, title, description, defaultValue);
	} catch (...) {
		return lua_instance.error("Unknown exception");
	}
	return lua_instance.size();
}
boost::shared_ptr<lua::settings_provider> lua::settings_wrapper::get() {
	return info->get_settings_provider();
}

const char lua::settings_wrapper::className[] = "Settings";
const Luna<lua::settings_wrapper>::FunctionType lua::settings_wrapper::Functions[] = {
	{ "get_section", &lua::settings_wrapper::get_section },
	{ "get_string", &lua::settings_wrapper::get_string },
	{ "set_string", &lua::settings_wrapper::set_string },
	{ "get_bool", &lua::settings_wrapper::get_bool },
	{ "set_bool", &lua::settings_wrapper::set_bool },
	{ "get_int", &lua::settings_wrapper::get_int },
	{ "set_int", &lua::settings_wrapper::set_int },
	{ "save", &lua::settings_wrapper::save },
	{ "register_path", &lua::settings_wrapper::register_path },
	{ "register_key", &lua::settings_wrapper::register_key },
	{ 0 }
};
const Luna<lua::settings_wrapper>::PropertyType lua::settings_wrapper::Properties[] = { {0} };

//////////////////////////////////////////////////////////////////////////
// traits

static int log_any(lua_State *L, int mode) {
	lua::lua_wrapper lua_instance(L);
	lua::lua_wrapper::stack_trace trace = lua_instance.get_stack_trace();
	if (lua_instance.size() < 1)
		return lua_instance.error("Invalid syntax: log(message)");
	std::string str = lua_instance.pop_string();
	GET_CORE()->log(mode, trace.first, trace.second, str);
	return 0;
}
static int info(lua_State *L) {
	return log_any(L, NSCAPI::log_level::info);
}
static int error(lua_State *L) {
	return log_any(L, NSCAPI::log_level::error);
}
static int lua_sleep(lua_State *L) {
	lua::lua_wrapper lua_instance(L);
	int time = lua_instance.pop_int();
	boost::this_thread::sleep(boost::posix_time::milliseconds(time));
	return 0;
}

const luaL_Reg my_funcs[] = {
	{"sleep", lua_sleep},
	{"info", info},
	{"print", info},
	{"error", error},
	{NULL, NULL}
};

void lua::lua_script::luaopen(lua_State *L) {
	luaL_register(L, "nscp", my_funcs);
	lua_pop(L, 1);
	Luna<core_wrapper>::Register(L, "nscp");
	Luna<registry_wrapper>::Register(L, "nscp");
	Luna<settings_wrapper>::Register(L, "nscp");
#ifdef HAVE_LUA_PB
	lua_protobuf_Plugin_open(L);
#else
	GET_CORE()->log(NSCAPI::log_level::debug, __FILE__, __LINE__, "Lua not compiled with protocol buffer support");

#endif
}

boost::optional<boost::filesystem::path> lua::lua_script::find_script(boost::filesystem::path root, std::string file) {
	std::list<boost::filesystem::path> checks;
	checks.push_back(file);
	checks.push_back(root / "scripts" / "lua" / file);
	checks.push_back(root / "scripts" / file);
	checks.push_back(root / "lua" / file);
	checks.push_back(root / file);
	BOOST_FOREACH(boost::filesystem::path c, checks) {
		if (boost::filesystem::exists(c))
			return boost::optional<boost::filesystem::path>(c);
		if (boost::filesystem::exists(c.string() + ".lua"))
			return boost::optional<boost::filesystem::path>(c.string() + ".lua");
	}
	return boost::optional<boost::filesystem::path>();
}