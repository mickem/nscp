// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_core_wrapper.hpp>

#include <memory>

#include <string>

#include "Vcclr.h"
#include <clr/clr_scoped_ptr.hpp>

typedef cli::array<System::Byte> protobuf_data;

std::string to_nstring(System::String^ s);
System::String^ to_mstring(const std::string &s);
std::string to_nstring(protobuf_data^ byteArray);
protobuf_data^ to_pbd(const std::string &buffer);
