// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The C ABI shared between this module and the managed bridge
// (libs/dotnet-plugin-api/Native/Bridge.cs + NativeCore.cs). Keep both in sync.

#include <cstdint>

// Every function pointer crossing the boundary is cdecl: the managed side pins
// it with CallConvCdecl / delegate* unmanaged[Cdecl]. Spelled out because the
// 32-bit Windows default differs between the two worlds (stdcall vs cdecl).
#ifdef _WIN32
#define NSCP_DOTNET_CALL __cdecl
#else
#define NSCP_DOTNET_CALL
#endif

namespace dotnet {

// Operations the managed side can ask the core for, dispatched through one
// callback so only a single function pointer has to cross into managed code.
enum core_op : std::int32_t {
  op_query = 1,
  op_exec = 2,
  op_submit = 3,
  op_reload = 4,
  op_settings = 5,
  op_registry = 6,
  op_log = 7,
};

// Response sink: the callee pushes response bytes back into the caller's buffer.
typedef void(NSCP_DOTNET_CALL *write_fn)(void *wctx, const std::uint8_t *data, std::int32_t len);

// managed -> native: int core(ctx, op, str, data, len, write, wctx); returns 1 on
// success, 0 on failure. `str` carries the target/channel/module for the
// operations that have one (NUL-terminated UTF-8), else NULL.
typedef std::int32_t(NSCP_DOTNET_CALL *core_fn)(void *ctx, std::int32_t op, const char *str, const std::uint8_t *data, std::int32_t len, write_fn write,
                                                void *wctx);

// native -> managed (UnmanagedCallersOnly static methods on NSCP.Core.Native.Bridge).
typedef void *(NSCP_DOTNET_CALL *managed_load_fn)(core_fn core, void *ctx, const char *assembly_path, const char *factory_type, const char *alias,
                                                  std::int32_t plugin_id);
typedef std::int32_t(NSCP_DOTNET_CALL *managed_start_fn)(void *handle, std::int32_t mode);
typedef std::int32_t(NSCP_DOTNET_CALL *managed_unload_fn)(void *handle);
typedef std::int32_t(NSCP_DOTNET_CALL *managed_describe_fn)(void *handle, write_fn write, void *wctx);
typedef std::int32_t(NSCP_DOTNET_CALL *managed_query_fn)(void *handle, const char *command, const std::uint8_t *request, std::int32_t request_len,
                                                         write_fn write, void *wctx);

// Return codes of managed_query_fn.
const std::int32_t query_handled = 1;
const std::int32_t query_ignored = 0;
const std::int32_t query_failed = -1;

const char *const bridge_type_name = "NSCP.Core.Native.Bridge, NSCP.Core";
const char *const bridge_assembly = "NSCP.Core.dll";
const char *const bridge_runtimeconfig = "NSCP.Core.runtimeconfig.json";

}  // namespace dotnet
