/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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