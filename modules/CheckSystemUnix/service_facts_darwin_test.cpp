// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <facts/service_facts.hpp>
#include <stdexcept>

TEST(service_facts_darwin, reports_unsupported_instead_of_an_empty_inventory) { EXPECT_THROW(service_facts::gather(), std::runtime_error); }
