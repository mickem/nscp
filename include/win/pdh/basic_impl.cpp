// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "basic_impl.hpp"

namespace PDH {
NativeExternalPDH::fpPdhLookupPerfNameByIndex NativeExternalPDH::pPdhLookupPerfNameByIndex = nullptr;
NativeExternalPDH::fpPdhLookupPerfIndexByName NativeExternalPDH::pPdhLookupPerfIndexByName = nullptr;
NativeExternalPDH::fpPdhExpandCounterPath NativeExternalPDH::pPdhExpandCounterPath = nullptr;
NativeExternalPDH::fpPdhGetCounterInfo NativeExternalPDH::pPdhGetCounterInfo = nullptr;
NativeExternalPDH::fpPdhAddCounter NativeExternalPDH::pPdhAddCounter = nullptr;
NativeExternalPDH::fpPdhAddEnglishCounter NativeExternalPDH::pPdhAddEnglishCounter = nullptr;
NativeExternalPDH::fpPdhRemoveCounter NativeExternalPDH::pPdhRemoveCounter = nullptr;
NativeExternalPDH::fpPdhGetRawCounterValue NativeExternalPDH::pPdhGetRawCounterValue = nullptr;
NativeExternalPDH::fpPdhGetFormattedCounterValue NativeExternalPDH::pPdhGetFormattedCounterValue = nullptr;
NativeExternalPDH::fpPdhOpenQuery NativeExternalPDH::pPdhOpenQuery = nullptr;
NativeExternalPDH::fpPdhCloseQuery NativeExternalPDH::pPdhCloseQuery = nullptr;
NativeExternalPDH::fpPdhCollectQueryData NativeExternalPDH::pPdhCollectQueryData = nullptr;
NativeExternalPDH::fpPdhValidatePath NativeExternalPDH::pPdhValidatePath = nullptr;
NativeExternalPDH::fpPdhEnumObjects NativeExternalPDH::pPdhEnumObjects = nullptr;
NativeExternalPDH::fpPdhEnumObjectItems NativeExternalPDH::pPdhEnumObjectItems = nullptr;
NativeExternalPDH::fpPdhExpandWildCardPath NativeExternalPDH::pPdhExpandWildCardPath = nullptr;
HMODULE NativeExternalPDH::PDH_ = nullptr;
}  // namespace PDH