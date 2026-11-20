// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <win/service_account.hpp>

#ifdef WIN32

#include <windows.h>
// windows.h has to come first; the rest depend on its types.
#include <lm.h>
#include <sddl.h>

#include <memory>
#include <str/utf8.hpp>
#include <vector>

namespace nsclient {
namespace windows_service_account {

namespace {

struct local_free {
  void operator()(void *p) const {
    if (p != nullptr) ::LocalFree(p);
  }
};
typedef std::unique_ptr<void, local_free> parsed_sid;

parsed_sid parse_sid(const std::string &text) {
  PSID raw_sid = nullptr;
  if (!::ConvertStringSidToSidA(text.c_str(), &raw_sid)) return parsed_sid();
  return parsed_sid(raw_sid);
}

std::string sid_text(const PSID sid) {
  char *raw_text = nullptr;
  if (!::ConvertSidToStringSidA(sid, &raw_text)) return "<unknown sid>";
  const std::unique_ptr<char, local_free> text(raw_text);
  return std::string(raw_text);
}

// `DOMAIN\name` for a SID, or false when the host has no account for it.
bool account_of(const PSID sid, std::wstring &name, std::wstring &domain) {
  DWORD name_size = 0;
  DWORD domain_size = 0;
  SID_NAME_USE use = SidTypeUnknown;
  ::LookupAccountSidW(nullptr, sid, nullptr, &name_size, nullptr, &domain_size, &use);
  if (name_size == 0) return false;
  std::vector<wchar_t> name_buffer(name_size, 0);
  std::vector<wchar_t> domain_buffer(domain_size == 0 ? 1 : domain_size, 0);
  if (!::LookupAccountSidW(nullptr, sid, name_buffer.data(), &name_size, domain_buffer.data(), &domain_size, &use)) return false;
  name = name_buffer.data();
  domain = domain_buffer.data();
  return true;
}

}  // namespace

bool set_group_membership(const std::string &member_sid, const std::string &group_sid, const bool add, std::string &error) {
  const parsed_sid member = parse_sid(member_sid);
  if (!member) {
    error = "'" + member_sid + "' is not a SID";
    return false;
  }
  const parsed_sid group = parse_sid(group_sid);
  if (!group) {
    error = "'" + group_sid + "' is not a SID";
    return false;
  }
  // NetLocalGroup* takes the group by name, and the name is localised - so it
  // is looked up from the SID rather than written down here.
  std::wstring group_name, group_domain;
  if (!account_of(group.get(), group_name, group_domain)) {
    error = "the group " + group_sid + " does not exist on this host (GetLastError=" + std::to_string(::GetLastError()) + ")";
    return false;
  }

  LOCALGROUP_MEMBERS_INFO_0 info = {};
  info.lgrmi0_sid = member.get();
  const NET_API_STATUS status = add ? ::NetLocalGroupAddMembers(nullptr, group_name.c_str(), 0, reinterpret_cast<LPBYTE>(&info), 1)
                                    : ::NetLocalGroupDelMembers(nullptr, group_name.c_str(), 0, reinterpret_cast<LPBYTE>(&info), 1);
  if (status == NERR_Success) return true;
  if (add && status == ERROR_MEMBER_IN_ALIAS) return true;
  if (!add && status == ERROR_MEMBER_NOT_IN_ALIAS) return true;
  error = std::string(add ? "adding " : "removing ") + member_sid + (add ? " to " : " from ") + utf8::cvt<std::string>(group_name) +
          " failed: error=" + std::to_string(status);
  return false;
}

std::string describe_process_identity() {
  HANDLE raw_token = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &raw_token)) {
    return "<unknown account: OpenProcessToken failed, GetLastError=" + std::to_string(::GetLastError()) + ">";
  }
  struct close_token {
    HANDLE handle;
    ~close_token() { ::CloseHandle(handle); }
  } const token = {raw_token};

  std::string result;
  {
    DWORD size = 0;
    ::GetTokenInformation(token.handle, TokenUser, nullptr, 0, &size);
    std::vector<unsigned char> buffer(size == 0 ? 1 : size, 0);
    if (size != 0 && ::GetTokenInformation(token.handle, TokenUser, buffer.data(), size, &size)) {
      const PSID sid = reinterpret_cast<const TOKEN_USER *>(buffer.data())->User.Sid;
      std::wstring name, domain;
      if (account_of(sid, name, domain)) {
        result = utf8::cvt<std::string>(domain.empty() ? name : domain + L"\\" + name) + " (" + sid_text(sid) + ")";
      } else {
        result = sid_text(sid);
      }
    } else {
      result = "<unknown account>";
    }
  }
  {
    DWORD size = 0;
    ::GetTokenInformation(token.handle, TokenPrivileges, nullptr, 0, &size);
    std::vector<unsigned char> buffer(size == 0 ? 1 : size, 0);
    if (size != 0 && ::GetTokenInformation(token.handle, TokenPrivileges, buffer.data(), size, &size)) {
      const TOKEN_PRIVILEGES *privileges = reinterpret_cast<const TOKEN_PRIVILEGES *>(buffer.data());
      result += ", privileges:";
      if (privileges->PrivilegeCount == 0) result += " none";
      for (DWORD i = 0; i < privileges->PrivilegeCount; i++) {
        LUID luid = privileges->Privileges[i].Luid;
        wchar_t name[128] = {};
        DWORD name_size = 128;
        if (::LookupPrivilegeNameW(nullptr, &luid, name, &name_size)) {
          result += " " + utf8::cvt<std::string>(std::wstring(name));
        } else {
          result += " <unnamed>";
        }
      }
    } else {
      result += ", privileges: <unreadable>";
    }
  }
  return result;
}

}  // namespace windows_service_account
}  // namespace nsclient

#endif
