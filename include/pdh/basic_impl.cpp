#include "basic_impl.hpp"

namespace PDH {
	NativeExternalPDH::fpPdhLookupPerfNameByIndex NativeExternalPDH::pPdhLookupPerfNameByIndex = NULL;
	NativeExternalPDH::fpPdhLookupPerfIndexByName NativeExternalPDH::pPdhLookupPerfIndexByName = NULL;
	NativeExternalPDH::fpPdhExpandCounterPath NativeExternalPDH::pPdhExpandCounterPath = NULL;
	NativeExternalPDH::fpPdhGetCounterInfo NativeExternalPDH::pPdhGetCounterInfo = NULL;
	NativeExternalPDH::fpPdhAddCounter NativeExternalPDH::pPdhAddCounter = NULL;
	NativeExternalPDH::fpPdhRemoveCounter NativeExternalPDH::pPdhRemoveCounter = NULL;
	NativeExternalPDH::fpPdhGetFormattedCounterValue NativeExternalPDH::pPdhGetFormattedCounterValue = NULL;
	NativeExternalPDH::fpPdhOpenQuery NativeExternalPDH::pPdhOpenQuery = NULL;
	NativeExternalPDH::fpPdhCloseQuery NativeExternalPDH::pPdhCloseQuery = NULL;
	NativeExternalPDH::fpPdhCollectQueryData NativeExternalPDH::pPdhCollectQueryData = NULL;
	NativeExternalPDH::fpPdhValidatePath NativeExternalPDH::pPdhValidatePath = NULL;
	NativeExternalPDH::fpPdhEnumObjects NativeExternalPDH::pPdhEnumObjects = NULL;
	NativeExternalPDH::fpPdhEnumObjectItems NativeExternalPDH::pPdhEnumObjectItems = NULL;
	HMODULE NativeExternalPDH::PDH_ = NULL;
}
