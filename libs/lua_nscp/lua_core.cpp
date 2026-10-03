// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/optional/optional.hpp>
#include <lua/lua_core.hpp>
#include <lua/lua_cpp.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <nscapi/protobuf/functions_submit.hpp>

boost::recursive_mutex &lua::lua_gil::mutex() {
  static boost::recursive_mutex m;
  return m;
}

namespace {
// pcall pads a handler's results to the count asked for, so a value the
// handler did not return is a nil in its slot - never a short stack. The
// handlers used to count the stack instead, which never came up short on a
// query (a missing message read as the string "NIL") and always did on a
// command-line handler, whose every answer was refused as an invalid return.
bool top_is_nil(lua::lua_wrapper &lua) { return lua_isnil(lua.L, -1) != 0; }

// The string on top of the stack, or "" for a nil: a handler that returns only
// a status has no message.
std::string pop_optional_string(lua::lua_wrapper &lua) {
  if (top_is_nil(lua)) {
    lua.pop();
    return "";
  }
  return lua.pop_string();
}
}  // namespace

void lua::lua_runtime::register_query(const std::string &command, const std::string &description) {
  throw lua_exception("The method or operation is not implemented(reg_query).");
}

void lua::lua_runtime::register_subscription(const std::string &channel, const std::string &description) {
  throw lua_exception("The method or operation is not implemented(reg_sub).");
}

void lua::lua_runtime::on_query(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple,
                                const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                                const PB::Commands::QueryRequestMessage &request_message) {
  lua_gil::guard gil;
  lua_wrapper lua(prep_function(information, function));
  int args = 2;
  if (function.object_ref != 0) args = 3;
  if (simple) {
    std::list<std::string> argslist;
    for (int i = 0; i < request.arguments_size(); i++) argslist.push_back(request.arguments(i));
    lua.push_string(command);
    lua.push_array(argslist);
    if (lua.pcall(args, 3, 0) != 0)
      return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command: " + command + ": " + lua.pop_string());
    const std::string perf = pop_optional_string(lua);
    const std::string msg = pop_optional_string(lua);
    if (top_is_nil(lua)) {
      lua.pop();
      const std::string error = "Invalid return from " + command + ": expected (code, message, perf)";
      NSC_LOG_ERROR_STD(error);
      nscapi::protobuf::functions::append_simple_query_response_payload(response, command, NSCAPI::query_return_codes::returnUNKNOWN, error, "");
      return;
    }
    const NSCAPI::nagiosReturn ret = lua.pop_code();
    lua.gc(LUA_GCCOLLECT, 0);
    nscapi::protobuf::functions::append_simple_query_response_payload(response, command, ret, msg, perf);
  } else {
    lua.push_string(command);
    lua.push_raw_string(request.SerializeAsString());
    lua.push_raw_string(request_message.SerializeAsString());
    args++;
    if (lua.pcall(args, 1, 0) != 0)
      return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command: " + command + ": " + lua.pop_string());
    if (lua.size() < 1) {
      NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
      nscapi::protobuf::functions::append_simple_query_response_payload(response, command, NSCAPI::query_return_codes::returnUNKNOWN, "Invalid return data",
                                                                        "");
      return;
    }
    PB::Commands::QueryResponseMessage local_response;
    std::string data = lua.pop_raw_string();
    response->ParseFromString(data);
    lua.gc(LUA_GCCOLLECT, 0);
  }
}

void lua::lua_runtime::exec_main(script_information *information, const std::vector<std::string> &opts,
                                 PB::Commands::ExecuteResponseMessage::Response *response) {
  lua_gil::guard gil;
  lua_wrapper lua(prep_function(information, "main"));
  lua.push_array(opts);
  if (lua.pcall(1, 2, 0) != 0) return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command main: " + lua.pop_string());
  const std::string msg = pop_optional_string(lua);
  if (top_is_nil(lua)) {
    lua.pop();
    const std::string error = "Invalid return from main: expected (code, message)";
    NSC_LOG_ERROR_STD(error);
    nscapi::protobuf::functions::append_simple_exec_response_payload(response, "", NSCAPI::exec_return_codes::returnERROR, error);
    return;
  }
  const NSCAPI::nagiosReturn ret = lua.pop_code();
  lua.gc(LUA_GCCOLLECT, 0);
  nscapi::protobuf::functions::append_simple_exec_response_payload(response, "", ret, msg);
}
void lua::lua_runtime::on_exec(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple,
                               const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response,
                               const PB::Commands::ExecuteRequestMessage &request_message) {
  lua_gil::guard gil;
  lua_wrapper lua(prep_function(information, function));
  int args = 2;
  if (function.object_ref != 0) args = 3;
  if (simple) {
    std::list<std::string> argslist;
    for (int i = 0; i < request.arguments_size(); i++) argslist.push_back(request.arguments(i));
    lua.push_string(command);
    lua.push_array(argslist);
    if (lua.pcall(args, 2, 0) != 0)
      return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command: " + command + ": " + lua.pop_string());
    const std::string msg = pop_optional_string(lua);
    if (top_is_nil(lua)) {
      lua.pop();
      const std::string error = "Invalid return from " + command + ": expected (code, message)";
      NSC_LOG_ERROR_STD(error);
      nscapi::protobuf::functions::append_simple_exec_response_payload(response, command, NSCAPI::exec_return_codes::returnERROR, error);
      return;
    }
    const NSCAPI::nagiosReturn ret = lua.pop_code();
    lua.gc(LUA_GCCOLLECT, 0);
    nscapi::protobuf::functions::append_simple_exec_response_payload(response, command, ret, msg);
  } else {
    lua.push_string(command);
    lua.push_raw_string(request.SerializeAsString());
    lua.push_raw_string(request_message.SerializeAsString());
    args++;
    if (lua.pcall(args, 1, 0) != 0)
      return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command: " + command + ": " + lua.pop_string());
    if (lua.size() < 1) {
      NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
      nscapi::protobuf::functions::append_simple_exec_response_payload(response, command, NSCAPI::exec_return_codes::returnERROR, "Invalid return data");
      return;
    }
    PB::Commands::QueryResponseMessage local_response;
    std::string data = lua.pop_raw_string();
    response->ParseFromString(data);
    lua.gc(LUA_GCCOLLECT, 0);
  }
}

void lua::lua_runtime::on_submit(std::string channel, script_information *information, lua::lua_traits::function_type function, bool simple,
                                 const PB::Commands::QueryResponseMessage::Response &request, PB::Commands::SubmitResponseMessage::Response *response) {
  lua_gil::guard gil;
  lua_wrapper lua(prep_function(information, function));
  // cmd_args is the leading self argument (1 when the handler is a bound method,
  // 0 otherwise); the fixed channel/command/... args are added on at each pcall.
  int cmd_args = 0;
  if (function.object_ref != 0) cmd_args = 1;
  if (simple) {
    lua.push_string(channel);
    lua.push_string(request.command());
    auto code = nscapi::protobuf::functions::gbp_to_nagios_status(request.result());
    lua.push_string(lua.code_to_string(code));
    lua_createtable(lua.L, 0, static_cast<int>(request.lines_size()));
    for (auto &line : request.lines()) {
      lua.push_string(line.message());
      std::string perf = nscapi::protobuf::functions::build_performance_data(line, nscapi::protobuf::functions::no_truncation);
      lua.push_string(perf);
      lua_settable(lua.L, -3);
    }
    if (lua.pcall(cmd_args + 4, 2, 0) != 0) {
      // Answered, not just logged: with no payload the caller could only
      // report an invalid response, not what went wrong.
      const std::string error = "Failed to handle channel: " + channel + ": " + lua.pop_string();
      NSC_LOG_ERROR_STD(error);
      nscapi::protobuf::functions::append_simple_submit_response_payload(response, channel, NSCAPI::bool_return::isfalse, error);
      return;
    }
    const std::string msg = pop_optional_string(lua);
    const bool ret = lua.pop_boolean();
    lua.gc(LUA_GCCOLLECT, 0);
    nscapi::protobuf::functions::append_simple_submit_response_payload(response, channel, ret ? NSCAPI::bool_return::istrue : NSCAPI::bool_return::isfalse,
                                                                       msg);
  } else {
    lua.push_string(channel);
    lua.push_raw_string(request.SerializeAsString());
    if (lua.pcall(cmd_args + 2, 1, 0) != 0)
      return nscapi::protobuf::functions::append_simple_submit_response_payload(response, channel, NSCAPI::bool_return::isfalse,
                                                                                "Failed to handle command: " + channel + ": " + lua.pop_string());
    if (lua.size() < 1) {
      NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
      nscapi::protobuf::functions::append_simple_submit_response_payload(response, channel, NSCAPI::bool_return::isfalse, "Invalid return");
      return;
    }
    PB::Commands::SubmitResponseMessage local_response;
    std::string data = lua.pop_raw_string();
    response->ParseFromString(data);
    lua.gc(LUA_GCCOLLECT, 0);
  }
}

void lua::lua_runtime::create_user_data(scripts::script_information<lua_traits> *info) { info->user_data.base_path_ = base_path; }

// load, start and unload run script code too - the top-level code, on_start,
// the plugins' unload hooks - so they hold the GIL like every other entry
// into Lua. They used not to, and the Core calls release the GIL around the
// core call they make: an on_start that queried anything unlocked a mutex its
// thread did not hold and then took it for good, and every Lua call after
// that waited forever.
void lua::lua_runtime::load(scripts::script_information<lua_traits> *info) {
  lua_gil::guard gil;
  const std::string &script_base_path = info->user_data.base_path_;
  lua_wrapper lua_instance(info->user_data.L);
  lua_instance.set_userdata(lua::lua_traits::user_data_tag, info);
  lua_instance.openlibs();
  lua_script::luaopen(info->user_data.L);
  for (lua_runtime_plugin_type &plugin : plugins) {
    plugin->load(lua_instance);
  }
  // script_base_path is ${scripts}. It used to be the install base with
  // "/scripts" appended here, which only lands on the scripts folder on
  // Windows: on Linux the base is the directory holding the binary, so
  // require() of anything in lua/lib could not work from a package.
  lua_instance.append_path(script_base_path + "/lua/lib/?.lua;" + script_base_path + "/lua/?.lua");
  if (lua_instance.loadfile(info->script) != 0) throw lua::lua_exception("Failed to load script: " + info->script + ": " + lua_instance.pop_string());
  if (lua_instance.pcall(0, 0, 0) != 0) throw lua::lua_exception("Failed to execute script: " + info->script + ": " + lua_instance.pop_string());
  lua_instance.gc(LUA_GCCOLLECT, 0);
}
void lua::lua_runtime::start(scripts::script_information<lua_traits> *info) {
  lua_gil::guard gil;
  lua_wrapper lua_instance(info->user_data.L);
  lua_instance.getglobal("on_start");
  if (lua_instance.is_function()) {
    lua_instance.getglobal("on_start");
    if (lua_instance.pcall(0, 0, 0) != 0) {
      throw lua_exception("Failed to start script: " + info->script + ": " + lua_instance.pop_string());
    }
  }
}

void lua::lua_runtime::unload(scripts::script_information<lua_traits> *info) {
  lua_gil::guard gil;
  lua_wrapper lua_instance(info->user_data.L);
  for (lua_runtime_plugin_type &plugin : plugins) {
    plugin->unload(lua_instance);
  }
  lua_instance.gc(LUA_GCCOLLECT, 0);
  lua_instance.remove_userdata(lua::lua_traits::user_data_tag);
}