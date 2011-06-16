#pragma once


#include <settings/macros.h>

namespace setting_keys {
	namespace task_scheduler {
		DEFINE_PATH(SECTION, TASK_SCHED_SECTION);
		DESCRIBE_SETTING_ADVANCED(SECTION, "TASK SCHEDULER", "???");

		DEFINE_SETTING_S(SYNTAX, TASK_SCHED_SECTION, "syntax", "%title% last run: %most-recent-run-time% (%exit-code%)");
		DESCRIBE_SETTING_ADVANCED(SYNTAX, "SYNTAX", "Set this to use a specific syntax string for all commands (that don't specify one)");
	}

}