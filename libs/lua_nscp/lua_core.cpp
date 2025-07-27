

#include <boost/optional/optional.hpp>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <lua/lua_cpp.hpp>
#include <lua/lua_core.hpp>

void lua::lua_runtime::register_query(const std::string &command, const std::string &description) {
  throw lua_exception("The method or operation is not implemented(reg_query).");
}

void lua::lua_runtime::register_subscription(const std::string &channel, const std::string &description) {
  throw lua_exception("The method or operation is not implemented(reg_sub).");
}

void lua::lua_runtime::on_query(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple,
                                const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                                const PB::Commands::QueryRequestMessage &request_message) {
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
    NSCAPI::nagiosReturn ret = NSCAPI::query_return_codes::returnUNKNOWN;
    if (lua.size() < 3) {
      NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
      nscapi::protobuf::functions::append_simple_query_response_payload(response, command, NSCAPI::query_return_codes::returnUNKNOWN, "Invalid return", "");
      return;
    }
    std::string msg, perf;
    perf = lua.pop_string();
    msg = lua.pop_string();
    ret = lua.pop_code();
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
  lua_wrapper lua(prep_function(information, "main"));
  lua.push_array(opts);
  if (lua.pcall(1, 2, 0) != 0) return nscapi::protobuf::functions::set_response_bad(*response, "Failed to handle command main: " + lua.pop_string());
  NSCAPI::nagiosReturn ret = NSCAPI::exec_return_codes::returnERROR;
  if (lua.size() < 2) {
    NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
    nscapi::protobuf::functions::append_simple_exec_response_payload(response, "", NSCAPI::exec_return_codes::returnERROR, "Invalid return");
    return;
  }
  std::string msg;
  msg = lua.pop_string();
  ret = lua.pop_code();
  lua.gc(LUA_GCCOLLECT, 0);
  nscapi::protobuf::functions::append_simple_exec_response_payload(response, "", ret, msg);
}
void lua::lua_runtime::on_exec(std::string command, script_information *information, lua::lua_traits::function_type function, bool simple,
                               const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response,
                               const PB::Commands::ExecuteRequestMessage &request_message) {
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
    NSCAPI::nagiosReturn ret = NSCAPI::exec_return_codes::returnERROR;
    if (lua.size() < 3) {
      NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
      nscapi::protobuf::functions::append_simple_exec_response_payload(response, command, NSCAPI::exec_return_codes::returnERROR, "Invalid return");
      return;
    }
    std::string msg, perf;
    msg = lua.pop_string();
    ret = lua.pop_code();
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
  lua_wrapper lua(prep_function(information, function));
  int cmd_args = 1;
  if (function.object_ref != 0) cmd_args = 2;
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
      NSC_LOG_ERROR_STD("Failed to handle channel: " + channel + ": " + lua.pop_string());
      return;
    }
    if (lua.size() < 2) {
      NSC_LOG_ERROR_STD("Invalid return: " + lua.dump_stack());
      nscapi::protobuf::functions::append_simple_submit_response_payload(response, channel, NSCAPI::bool_return::isfalse, "Invalid return");
      return;
    }
    std::string msg, perf;
    msg = lua.pop_string();
    bool ret = lua.pop_boolean();
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

void lua::lua_runtime::load(scripts::script_information<lua_traits> *info) {
  std::string base_path = info->user_data.base_path_;
  lua_wrapper lua_instance(info->user_data.L);
  lua_instance.set_userdata(lua::lua_traits::user_data_tag, info);
  lua_instance.openlibs();
  lua_script::luaopen(info->user_data.L);
  for (lua_runtime_plugin_type &plugin : plugins) {
    plugin->load(lua_instance);
  }
  lua_instance.append_path(base_path + "/scripts/lua/lib/?.lua;" + base_path + "scripts/lua/?;");
  if (lua_instance.loadfile(info->script) != 0) throw lua::lua_exception("Failed to load script: " + info->script + ": " + lua_instance.pop_string());
  if (lua_instance.pcall(0, 0, 0) != 0) throw lua::lua_exception("Failed to execute script: " + info->script + ": " + lua_instance.pop_string());
  lua_instance.gc(LUA_GCCOLLECT, 0);
}
void lua::lua_runtime::start(scripts::script_information<lua_traits> *info) {
  lua_wrapper lua_instance(info->user_data.L);
  int index = lua_instance.getglobal("on_start");
  if (lua_instance.is_function()) {
    int index = lua_instance.getglobal("on_start");
    if (lua_instance.pcall(0, 0, 0) != 0) {
      throw lua_exception("Failed to start script: " + info->script + ": " + lua_instance.pop_string());
    }
  }
}

void lua::lua_runtime::unload(scripts::script_information<lua_traits> *info) {
  lua_wrapper lua_instance(info->user_data.L);
  for (lua_runtime_plugin_type &plugin : plugins) {
    plugin->unload(lua_instance);
  }
  lua_instance.gc(LUA_GCCOLLECT, 0);
  lua_instance.remove_userdata(lua::lua_traits::user_data_tag);
}