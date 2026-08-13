// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Windows.h must precede lm.h; the capital W keeps clang-format's
// case-sensitive include sort from breaking that order.
#include <Windows.h>
#include <lm.h>

#include <memory>

namespace check_ad {

// Buffers handed out by the netapi32 family - NetGetJoinInformation,
// I_NetLogonControl2, DsGetDcName - are all released with NetApiBufferFree.
// Owning them through unique_ptr puts that release on every path, including
// the early returns these checks take whenever a lookup fails.
struct net_api_deleter {
  void operator()(void *buffer) const noexcept {
    if (buffer != nullptr) NetApiBufferFree(buffer);
  }
};

template <typename T>
using net_api_ptr = std::unique_ptr<T, net_api_deleter>;

// Adopt a raw out-parameter the API just filled in:
//   T *raw = nullptr;
//   const DWORD rc = SomeNetApi(..., &raw);
//   const auto owned = check_ad::adopt_net_api(raw);
template <typename T>
net_api_ptr<T> adopt_net_api(T *buffer) {
  return net_api_ptr<T>(buffer);
}

}  // namespace check_ad
