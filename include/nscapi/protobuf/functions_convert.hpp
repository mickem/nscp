// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/dll_defines.hpp>
#include <nscapi/protobuf/common.hpp>
#include <string>

namespace nscapi {
namespace protobuf {
namespace functions {

// Message conversion functions
NSCAPI_EXPORT void make_submit_from_query(std::string &message, const std::string &channel, const std::string &alias = "", const std::string &target = "",
                                          const std::string &source = "");
NSCAPI_EXPORT void make_query_from_exec(std::string &data);
NSCAPI_EXPORT void make_query_from_submit(std::string &data);
NSCAPI_EXPORT void make_exec_from_submit(std::string &data);

// Header manipulation
NSCAPI_EXPORT void make_return_header(PB::Common::Header *target, const PB::Common::Header &source);

}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
