#pragma once


#include <settings/macros.h>

namespace setting_keys {
	namespace check_system {

		DEFINE_PATH(SECTION, CHECK_SYSTEM_SECTION);
		DESCRIBE_SETTING(SECTION, "SYSTEM", "Section for system checks and system settings.");

		const std::wstring PDH_SUBSYSTEM_FAST = _T("fast");
		const std::wstring PDH_SUBSYSTEM_THREAD_SAFE = _T("thread-safe");

		DEFINE_SETTING_S(PDH_SUBSYSTEM, CHECK_SYSTEM_SECTION, "pdh_subsystem", "fast");
		DESCRIBE_SETTING_ADVANCED(PDH_SUBSYSTEM, "PDH_SUBSYSTEM", "TODO");


	}
}