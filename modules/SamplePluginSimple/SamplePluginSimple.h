// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>

// Your implementation class can derive from various helper implementations
// simple_plugin			- Hides ID handling in your plugin and allows you to register and access the various cores.
// simple_command_handler	- Provides a "nagios plugin" like command handler interface (so you wont have to deal with google protocol buffers)
// There is a bunch of others as well for wrapping the other APIs
class SamplePluginSimple : public nscapi::impl::simple_plugin {
 private:
 public:
  SamplePluginSimple();
  virtual ~SamplePluginSimple();

  // Declare exposed API methods (C++ versions)
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void sample_raw_command(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};