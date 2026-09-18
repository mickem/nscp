// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Export control for the nscp_where_filter shared library.
//
// This used to define NSCAPI_EXPORT, guarded by `#ifndef NSCAPI_EXPORT` so it
// would not clash with the identically named macro in nscapi/dll_defines.hpp
// and libs/mongoose-cpp/dll_defines.hpp, which both define it unconditionally.
// That is not a guard, it is a silent forfeit: `#ifndef` means a definition
// already in scope simply wins, with no warning, so the linkage of the where
// nodes was decided by whichever of the three headers the translation unit
// happened to include first. A test that compiled a plugin_api source (so
// NSCAPI_EXPORT was empty) and also linked nscp_where_filter then emitted its
// own copies of the nodes and failed with LNK2005 against the import library.
// Between the other two the clash is at least loud: redefining the name gives
// C4005 and the last include wins.
//
// So this library says NSCP_WHERE_EXPORT, as plugin_api, nscp_protobuf,
// nscp_net, nscp_client and nscp_mongoose each say their own. No two of them
// share a macro name any more.

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
