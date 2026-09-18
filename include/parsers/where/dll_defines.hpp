// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Export control for the nscp_where_filter shared library.
//
// This used to define NSCAPI_EXPORT, guarded by `#ifndef NSCAPI_EXPORT` so it
// would not clash with the identically named macro in nscapi/dll_defines.hpp
// and libs/mongoose-cpp/dll_defines.hpp. That is not a guard, it is a coin
// toss: in a translation unit that includes two of them, whichever lands first
// decides dllexport-vs-dllimport for the symbols of the others. A test that
// compiled a plugin_api source (so NSCAPI_EXPORT was empty) and also linked
// nscp_where_filter then emitted its own copies of the where nodes and failed
// to link with LNK2005 against the import library.
//
// So this library says NSCP_WHERE_EXPORT, as nscp_protobuf, nscp_net and
// nscp_client each say their own. libs/mongoose-cpp still shares the name; it
// is reached only from WEBServer and has not bitten yet.

#if defined(_WIN32)
#if defined(nscp_where_filter_NOLIB)
#define NSCP_WHERE_EXPORT
#else
#if defined(nscp_where_filter_EXPORTS)
#define NSCP_WHERE_EXPORT __declspec(dllexport)
#else
#define NSCP_WHERE_EXPORT __declspec(dllimport)
#endif /* nscp_where_filter_EXPORTS */
#endif /* nscp_where_filter_NOLIB */
#else  /* defined(_WIN32) */
#define NSCP_WHERE_EXPORT
#endif
