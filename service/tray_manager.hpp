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

#pragma once

#include "NSCPlugin.h"

namespace tray_manager {
	class tray_exception {
		std::wstring what_;
	public:
		tray_exception(std::wstring what) : what_(what) {
#ifdef _DEBUG
			std::cout << _T("TrayManager throw an exception: ") << what << std::endl;
#endif

		}
		std::wstring what() { return what_; }
	};

	class tray_manager {
		NSCPlugin *plugin_;
		event_handler event_;
	public:
		tray_manager(NSCPlugin *plugin) : plugin_(plugin), event_(_T("NSCTrayWaitStop")) {}

		boolean start_and_wait() {
			if (plugin_ == NULL)
				return false;
			try {
				plugin_->showTray();
			} catch (NSPluginException e) {
				throw tray_exception(_T("Systemtray manager failed to show tray: ") + e.error_);
			} catch (...) {
				throw tray_exception(_T("Systemtray manager failed to show tray: Unknown exception"));
			}
			event_.accuire(INFINITE);
			try {
				plugin_->hideTray();
			} catch (NSPluginException e) {
				throw tray_exception(_T("Systemtray manager failed to close tray: ") + e.error_);
			} catch (...) {
				throw tray_exception(_T("Systemtray manager failed to close tray: Unknown exception"));
			}
			return true;
		}
		void request_close() {
			event_.set();
		}
	};
}