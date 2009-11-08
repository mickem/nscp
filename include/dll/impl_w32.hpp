#pragma once
#include <unicode_char.hpp>
#include <boost/noncopyable.hpp>
#include <error.hpp>

namespace dll {
	namespace win32 {
		class impl : public boost::noncopyable {
		private:
			HMODULE handle_;
			std::wstring module_;
		public:
			impl(std::wstring module) : module_(module), handle_(NULL) {}

			void load_library() {
				if (handle_ != NULL)
					unload_library();
				handle_ = LoadLibrary(module_.c_str());
				if (handle_ == NULL)
					throw dll_exception(_T("Could not load library: ") + error::lookup::last_error() + _T(": ") + module_);
			}
			LPVOID load_proc(std::string name) {
				if (handle_ == NULL)
					throw dll_exception(_T("Failed to load process from module: ") + module_);
				return GetProcAddress(handle_, name.c_str());
			}

			void unload_library() {
				if (handle_ == NULL)
					return;
				FreeLibrary(handle_);
				handle_ = NULL;
			}

			bool is_loaded() const { return handle_ != NULL; }
			std::wstring get_file() const { return module_; }
		};
	}
}


