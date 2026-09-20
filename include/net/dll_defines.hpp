// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Export control for the nscp_net shared library.
//
// Its own macro name for the same reason client/dll_defines.hpp has one: every
// library here names its export macro after itself, so that no translation unit
// including two of these headers can have one library's linkage decided by the
// other's. See parsers/where/dll_defines.hpp for what that used to cost.
//
// nscp_net_EXPORTS is defined by CMake while building the library itself.
// nscp_net_NOLIB is defined by a consumer that compiles socket_helpers.cpp /
// allowed_hosts.cpp into its own binary instead of linking the DLL: the
// static (XP) build, installer_lib, and every unit test.
//
// Note that USE_SSL stays a per-consumer choice, so a declaration guarded by
// it is annotated only where it is declared at all. The macro says nothing
// about SSL either way.

#if defined(_WIN32)
#if defined(nscp_net_NOLIB)
#define NSCP_NET_EXPORT
#else
#if defined(nscp_net_EXPORTS)
#define NSCP_NET_EXPORT __declspec(dllexport)
#else
#define NSCP_NET_EXPORT __declspec(dllimport)
#endif /* nscp_net_EXPORTS */
#endif /* nscp_net_NOLIB */
#else  /* defined(_WIN32) */
// Everything is visible by default on the unix builds, exactly as it was when
// each consumer compiled these sources privately.
#define NSCP_NET_EXPORT
#endif
