// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <facts/host_facts.hpp>

namespace system_facts {

// Gather this Windows host's facts. Every source is local and cheap - the
// cached version info, GetNativeSystemInfo, GlobalMemoryStatusEx, CPUID and
// two registry values - because this runs on the module load path, where a
// WMI query or a resolver lookup would hold up the whole service start. That
// rules out Win32_ComputerSystem (check_hardware's source) for the vendor and
// model: the same SMBIOS strings are in HKLM\HARDWARE\DESCRIPTION\System\BIOS,
// which the kernel fills in at boot and reads back instantly.
//
// A fact that cannot be determined is left empty; see host_facts::publish.
host_facts::facts gather();

}  // namespace system_facts
