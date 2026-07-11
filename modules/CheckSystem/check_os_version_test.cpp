// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "filter.hpp"

#include <gtest/gtest.h>

using os_version_filter::arch_from_native;
using os_version_filter::format_kernel_version;

TEST(CheckOsVersionKernel, FormatsFourPartVersion) {
  EXPECT_EQ(format_kernel_version(10, 0, 19045, 3803), "10.0.19045.3803");
}

TEST(CheckOsVersionKernel, ZeroUbrWhenUnavailable) {
  // Pre-Windows 10 has no UBR registry value; the caller passes 0.
  EXPECT_EQ(format_kernel_version(6, 1, 7601, 0), "6.1.7601.0");
}

TEST(CheckOsVersionArch, MapsKnownArchitectures) {
  EXPECT_EQ(arch_from_native(PROCESSOR_ARCHITECTURE_AMD64), "x64");
  EXPECT_EQ(arch_from_native(PROCESSOR_ARCHITECTURE_ARM64), "arm64");
  EXPECT_EQ(arch_from_native(PROCESSOR_ARCHITECTURE_ARM), "arm");
  EXPECT_EQ(arch_from_native(PROCESSOR_ARCHITECTURE_IA64), "ia64");
  EXPECT_EQ(arch_from_native(PROCESSOR_ARCHITECTURE_INTEL), "x86");
}

TEST(CheckOsVersionArch, UnknownArchitectureFallsBack) {
  EXPECT_EQ(arch_from_native(PROCESSOR_ARCHITECTURE_UNKNOWN), "unknown");
  EXPECT_EQ(arch_from_native(0xBEEF), "unknown");
}
