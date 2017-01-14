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