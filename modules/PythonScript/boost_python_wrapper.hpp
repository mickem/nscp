// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Wrapper for Boost.Python includes to suppress third-party warnings
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)  // conversion from 'long double' to 'double', possible loss of data
#endif

#include <boost/python.hpp>
#include <boost/python/dict.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif
