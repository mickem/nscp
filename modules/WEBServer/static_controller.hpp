// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <Controller.h>

#include <boost/filesystem/operations.hpp>
#include <string>

#include "session_manager_interface.hpp"

class StaticController : public Mongoose::Controller {
  std::shared_ptr<session_manager_interface> session;
  boost::filesystem::path base;

 public:
  StaticController(const std::shared_ptr<session_manager_interface> &session, const std::string &path);

  Mongoose::Response *handleRequest(Mongoose::Request &request);
  bool handles(std::string method, std::string url);

  // Built-in HTML served when the web bundle hasn't been installed yet
  // (e.g. on a fresh .deb/.rpm install before `nscp web install-ui`).
  // Self-contained: no external assets, inline CSS only. Exposed for
  // unit testing — production paths reach it via handleRequest.
  static const std::string &placeholder_html();
};