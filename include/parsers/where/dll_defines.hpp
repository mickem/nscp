// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// We are using the Visual Studio Compiler and building Shared libraries
// Only define NSCAPI_EXPORT if not already defined by nscapi/dll_defines.hpp
#ifndef NSCAPI_EXPORT
#if defined(_WIN32)
#if defined(nscp_where_filter_NOLIB)
#define NSCAPI_EXPORT
#else
#if defined(nscp_where_filter_EXPORTS)
#define NSCAPI_EXPORT __declspec(dllexport)
#else
#define NSCAPI_EXPORT __declspec(dllimport)
#endif /* nscp_where_filter_EXPORTS */
#endif /* nscp_where_filter_NOLIB */
#else  /* defined (_WIN32) */
#define NSCAPI_EXPORT
#endif
#endif /* NSCAPI_EXPORT */
