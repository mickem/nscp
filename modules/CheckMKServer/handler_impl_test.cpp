// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for handler_impl::process(), the bridge between a check_mk TCP
// connection and the Lua script that renders the agent output. The contract
// is small but load-bearing: find the script's registered s_callback, run it
// under the Lua GIL with a packet to fill in, and hand that packet back - or
// an empty packet when there is no callback or the script blows up (an empty
// agent dump is how a broken script surfaces to the poller, rather than a
// crashed connection thread).
//
// The tests drive a real Lua runtime with the real check_mk glue, loading
// tiny inline scripts; only the surrounding NSClient++ runtime is mocked out.

#include "handler_impl.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <lua/lua_core.hpp>
#include <memory>
#include <scripts/script_interface.hpp>
#include <string>

// Normally provided by NSC_WRAP_DLL(); the module's logging macros need it.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

// The scripts here never touch Core() or Settings(), so the providers can be
// left empty; register_command is what the mk.server_callback registration
// funnels through and is a no-op for the manager-side bookkeeping we rely on.
struct fake_nscp_runtime : public scripts::nscp_runtime_interface {
  void register_command(const std::string, const std::string &, const std::string &) override {}
  std::shared_ptr<scripts::settings_provider> get_settings_provider() override { return {}; }
  std::shared_ptr<scripts::core_provider> get_core_provider() override { return {}; }
};

// A script manager with one inline Lua script loaded through the real
// lua_runtime + check_mk plugin (the same wiring CheckMKServer::loadModuleEx
// sets up). unload_all() on destruction frees the lua_State.
class loaded_script {
 public:
  explicit loaded_script(const std::string &name, const std::string &source) : path_(name) {
    std::ofstream out(path_.c_str());
    out << source;
    out.close();

    runtime_.reset(new lua::lua_runtime("."));
    runtime_->register_plugin(std::make_shared<check_mk::check_mk_plugin>());
    scripts_.reset(new scripts::script_manager<lua::lua_traits>(runtime_, std::make_shared<fake_nscp_runtime>(), 42, "test"));
    scripts_->add_and_load("test", path_);
  }

  ~loaded_script() {
    scripts_->unload_all();
    std::remove(path_.c_str());
  }

  std::shared_ptr<scripts::script_manager<lua::lua_traits> > manager() { return scripts_; }

 private:
  std::string path_;
  std::shared_ptr<lua::lua_runtime> runtime_;
  std::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts_;
};

}  // namespace

TEST(CheckMKHandlerImpl, TheServerCallbackFillsThePacket) {
  loaded_script script("handler_impl_test_ok.lua",
                       "function server_process(packet)\n"
                       "  local s = section.new()\n"
                       "  s:set_title('check_mk')\n"
                       "  s:add_line('Version: NSClient++')\n"
                       "  s:add_line('AgentOS: test')\n"
                       "  packet:add_section(s)\n"
                       "end\n"
                       "local reg = mk.new()\n"
                       "reg:server_callback(server_process)\n");

  handler_impl handler(script.manager());
  const check_mk::packet packet = handler.process();

  const std::string wire = packet.write();
  EXPECT_NE(wire.find("<<<check_mk>>>"), std::string::npos) << wire;
  EXPECT_NE(wire.find("Version: NSClient++"), std::string::npos) << wire;
  EXPECT_NE(wire.find("AgentOS: test"), std::string::npos) << wire;
}

TEST(CheckMKHandlerImpl, RunningTwiceYieldsAFreshPacketEachTime) {
  // The callback must fill the packet handed to it on every call; state must
  // not leak from one connection to the next.
  loaded_script script("handler_impl_test_twice.lua",
                       "local calls = 0\n"
                       "function server_process(packet)\n"
                       "  calls = calls + 1\n"
                       "  local s = section.new()\n"
                       "  s:set_title('counter')\n"
                       "  s:add_line('call ' .. calls)\n"
                       "  packet:add_section(s)\n"
                       "end\n"
                       "local reg = mk.new()\n"
                       "reg:server_callback(server_process)\n");

  handler_impl handler(script.manager());
  const std::string first = handler.process().write();
  const std::string second = handler.process().write();

  EXPECT_NE(first.find("call 1"), std::string::npos) << first;
  EXPECT_NE(second.find("call 2"), std::string::npos) << second;
  EXPECT_EQ(second.find("call 1"), std::string::npos) << "a previous packet's data leaked: " << second;
}

TEST(CheckMKHandlerImpl, NoRegisteredCallbackYieldsAnEmptyPacket) {
  // A manager with no scripts has no s_callback: the handler must answer with
  // an empty agent dump instead of crashing the connection.
  auto runtime = std::make_shared<lua::lua_runtime>(".");
  auto scripts = std::make_shared<scripts::script_manager<lua::lua_traits> >(runtime, std::make_shared<fake_nscp_runtime>(), 42, "test");

  handler_impl handler(scripts);
  const check_mk::packet packet = handler.process();

  EXPECT_TRUE(packet.write().empty());
}

TEST(CheckMKHandlerImpl, AFailingCallbackYieldsAnEmptyPacket) {
  loaded_script script("handler_impl_test_fail.lua",
                       "function bad_process(packet)\n"
                       "  error('boom')\n"
                       "end\n"
                       "local reg = mk.new()\n"
                       "reg:server_callback(bad_process)\n");

  handler_impl handler(script.manager());
  const check_mk::packet packet = handler.process();

  EXPECT_TRUE(packet.write().empty());
}
