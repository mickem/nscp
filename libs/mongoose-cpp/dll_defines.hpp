// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Export control for the nscp_mongoose shared library.
//
// This used to call its macro NSCAPI_EXPORT, the same name nscapi/dll_defines.hpp
// uses, and both define it unconditionally. Any translation unit that included
// the two - every WEBServer source does - got a C4005 redefinition, and the
// linkage of one library's symbols was then decided by the other's header
// depending on include order. It was loud but ignored, and in WEBServer_test,
// where plugin_api_NOLIB makes the nscapi macro expand to nothing, losing the
// race would have left the mongoose classes emitted into the test instead of
// imported.
//
// So this library says NSCP_MONGOOSE_EXPORT, as plugin_api, nscp_protobuf,
// nscp_net, nscp_client and nscp_where_filter each say their own.

#if defined(_WIN32)
#if defined(lib_mongoose_NOLIB)
#define NSCP_MONGOOSE_EXPORT
#else
#if defined(lib_mongoose_EXPORTS)
#define NSCP_MONGOOSE_EXPORT __declspec(dllexport)
#else
#define NSCP_MONGOOSE_EXPORT __declspec(dllimport)
#endif /* lib_mongoose_EXPORTS */
#endif /* lib_mongoose_NOLIB */
#else  /* defined(_WIN32) */
#define NSCP_MONGOOSE_EXPORT
#endif
