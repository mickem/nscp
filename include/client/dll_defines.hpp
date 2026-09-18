// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Export control for the nscp_client shared library.
//
// Deliberately its own macro name rather than the NSCAPI_EXPORT that
// nscapi/, parsers/where/ and libs/mongoose-cpp/ all share: those three guard
// their definition with `#ifndef NSCAPI_EXPORT`, so in a translation unit that
// includes more than one of them whichever header lands first decides
// dllexport-vs-dllimport for the others. Adding a fourth user of that name
// would widen the trap; nscp_protobuf already avoids it the same way with
// NSCAPI_PROTOBUF_EXPORT.
//
// nscp_client_EXPORTS is defined by CMake while building the library itself.
// nscp_client_NOLIB is defined by a consumer that compiles
// command_line_parser.cpp into its own binary instead of linking the DLL - the
// static (XP) build, where NSCP_MAKE_LIBRARY produces a static library, and
// every unit test, which drives the parser without a core. There the macro
// must vanish: __declspec(dllimport) on a symbol the same binary defines does
// not link.

#if defined(_WIN32)
#if defined(nscp_client_NOLIB)
#define NSCP_CLIENT_EXPORT
#else
#if defined(nscp_client_EXPORTS)
#define NSCP_CLIENT_EXPORT __declspec(dllexport)
#else
#define NSCP_CLIENT_EXPORT __declspec(dllimport)
#endif /* nscp_client_EXPORTS */
#endif /* nscp_client_NOLIB */
#else  /* defined(_WIN32) */
// Everything is visible by default on the unix builds, exactly as it was when
// each consumer compiled the parser privately.
#define NSCP_CLIENT_EXPORT
#endif
