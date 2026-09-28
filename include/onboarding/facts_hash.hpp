// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The one facts-protocol constant the pacer (onboarding/facts_pacer.hpp)
// needs, apart from the rest of the wire helpers in onboarding/sync.hpp so a
// header-only rule set does not drag boost::json in with it. Defined in
// libs/onboarding/sync.cpp.
namespace onboarding {

// sha256 of `{}`: the hash of the empty facts document, what a host with
// nothing enabled holds and what `none` means.
extern const char *const empty_facts_hash;

}  // namespace onboarding
