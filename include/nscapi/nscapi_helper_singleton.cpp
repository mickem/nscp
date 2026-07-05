// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/nscapi_helper_singleton.hpp>

nscapi::helper_singleton::helper_singleton() : core_(new nscapi::core_wrapper()) {}