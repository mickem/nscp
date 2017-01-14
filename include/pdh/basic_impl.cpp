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
	NativeExternalPDH::fpPdhLookupPerfNameByIndex NativeExternalPDH::pPdhLookupPerfNameByIndex = NULL;
	NativeExternalPDH::fpPdhLookupPerfIndexByName NativeExternalPDH::pPdhLookupPerfIndexByName = NULL;
	NativeExternalPDH::fpPdhExpandCounterPath NativeExternalPDH::pPdhExpandCounterPath = NULL;
	NativeExternalPDH::fpPdhGetCounterInfo NativeExternalPDH::pPdhGetCounterInfo = NULL;
	NativeExternalPDH::fpPdhAddCounter NativeExternalPDH::pPdhAddCounter = NULL;
	NativeExternalPDH::fpPdhAddEnglishCounter NativeExternalPDH::pPdhAddEnglishCounter = NULL;
	NativeExternalPDH::fpPdhRemoveCounter NativeExternalPDH::pPdhRemoveCounter = NULL;
	NativeExternalPDH::fpPdhGetRawCounterValue NativeExternalPDH::pPdhGetRawCounterValue = NULL;
	NativeExternalPDH::fpPdhGetFormattedCounterValue NativeExternalPDH::pPdhGetFormattedCounterValue = NULL;
	NativeExternalPDH::fpPdhOpenQuery NativeExternalPDH::pPdhOpenQuery = NULL;
	NativeExternalPDH::fpPdhCloseQuery NativeExternalPDH::pPdhCloseQuery = NULL;
	NativeExternalPDH::fpPdhCollectQueryData NativeExternalPDH::pPdhCollectQueryData = NULL;
	NativeExternalPDH::fpPdhValidatePath NativeExternalPDH::pPdhValidatePath = NULL;
	NativeExternalPDH::fpPdhEnumObjects NativeExternalPDH::pPdhEnumObjects = NULL;
	NativeExternalPDH::fpPdhEnumObjectItems NativeExternalPDH::pPdhEnumObjectItems = NULL;
	NativeExternalPDH::fpPdhExpandWildCardPath NativeExternalPDH::pPdhExpandWildCardPath = NULL;
	HMODULE NativeExternalPDH::PDH_ = NULL;
}