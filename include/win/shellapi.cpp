#ifdef WIN32
#include <boost/filesystem/operations.hpp>
#include <utf8.hpp>
#include <win/shellapi.hpp>

boost::filesystem::path shellapi::get_module_file_name() {
  wchar_t buff[4096];
  if (GetModuleFileName(nullptr, buff, sizeof(buff) - 1)) {
    const boost::filesystem::path p = std::wstring(buff);
    return p.parent_path();
  }
  return boost::filesystem::initial_path();
}

boost::filesystem::path shellapi::get_special_folder_path(int csidl, boost::filesystem::path fallback) {
  wchar_t buf[MAX_PATH + 1];
  if (SHGetSpecialFolderPath(nullptr, buf, csidl, FALSE)) {
    return utf8::cvt<std::string>(buf);
  }
  return fallback;
}

typedef DWORD(WINAPI *PFGetTempPath)(__in DWORD nBufferLength, __out LPTSTR lpBuffer);

boost::filesystem::path shellapi::get_temp_path() {
  const HMODULE kernel_handle = ::LoadLibrary(L"kernel32");
  if (kernel_handle) {
    const auto proc = reinterpret_cast<PFGetTempPath>(::GetProcAddress(kernel_handle, "GetTempPathW"));
    if (proc) {
      constexpr unsigned int buf_len = 4096;
      const auto buffer = std::unique_ptr<wchar_t[]>(new wchar_t[buf_len + 1]);
      if (proc(buf_len, buffer.get())) {
        return buffer.get();
      }
    }
  }
  return "c:\\temp";
}

#endif