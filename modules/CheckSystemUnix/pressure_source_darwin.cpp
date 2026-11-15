// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Darwin reader for system.pressure. macOS has no pressure stall information,
// and its fault counters (vm_statistics64 `faults`, `pageins`) are not the
// same figure as Linux's major faults, so nothing is published rather than a
// number that reads like one and is not.

#include "pressure.h"

namespace pressure {

sample read_pressure() { return sample(); }

}  // namespace pressure
