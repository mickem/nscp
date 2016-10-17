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