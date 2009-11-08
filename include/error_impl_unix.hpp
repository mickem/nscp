#pragma once
#include <unicode_char.hpp>
#include <string>

namespace error {
	class format {
	public:
		static std::wstring from_system(unsigned long dwError) {
			return _T("ERROR TODO");
		}
		static std::wstring from_module(std::wstring module, unsigned long dwError) {
			return _T("ERROR TODO");
		}
		static std::wstring from_module(std::wstring module, unsigned long dwError, unsigned long *arguments) {
			return _T("ERROR TODO");
		}
		class message {
		public:
			static std::wstring from_module(std::wstring module, unsigned long dwError) {
				return _T("ERROR TODO");
			}

			static std::wstring from_module_x64(std::wstring module, unsigned long dwError, wchar_t* argList[], unsigned long argCount) {
				return _T("ERROR TODO");
			}
		private:

			static std::wstring from_module_wrapper(std::wstring module, unsigned long dwError, ...) {
				return _T("ERROR TODO");
			}

			static std::wstring __from_module(std::wstring module, unsigned long dwError, va_list *arguments) {
				return _T("ERROR TODO");
			}
		public:
			static std::wstring from_system(unsigned long dwError, unsigned long *arguments) {
				return _T("ERROR TODO");
			}
		};
	};
	class lookup {
	public:
		static std::wstring last_error(unsigned long dwLastError = -1) {
			return _T("ERROR TODO");
		}
		static std::string last_error_ansi(unsigned long dwLastError = -1) {
			return "ERROR TODO";
		}
	};
}
