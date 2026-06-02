// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// We are using the Visual Studio Compiler and building Shared libraries

#if defined(_WIN32)
#if defined(lib_mongoose_NOLIB)
#define NSCAPI_EXPORT
#else
#if defined(lib_mongoose_EXPORTS)
#define NSCAPI_EXPORT __declspec(dllexport)
#else
#define NSCAPI_EXPORT __declspec(dllimport)
#endif /* lib_mongoose_EXPORTS */
#endif /* lib_mongoose_NOLIB */
#else  /* defined (_WIN32) */
#if defined(lib_mongoose_NOLIB)
#define NSCAPI_EXPORT
#else
#if defined(lib_mongoose_EXPORTS)
//      #define NSCAPI_EXPORT __attribute__ ((visibility("default")))
#define NSCAPI_EXPORT
#else
#define NSCAPI_EXPORT
#endif /* lib_mongoose_EXPORTS */
#endif /* lib_mongoose_NOLIB */
#endif
