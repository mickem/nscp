// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <string>

namespace error {
namespace win32 {
unsigned int lookup();
std::string failed(unsigned long err1, unsigned long err2 = 0);
std::string format_message(unsigned long attrs, std::string module, unsigned long dwError);
std::string format_message(unsigned long attrs, std::string module, unsigned long dwError, unsigned long *arguments);
}  // namespace win32
}  // namespace error
