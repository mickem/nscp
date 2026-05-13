/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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