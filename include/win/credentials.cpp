#include <win/credentials.hpp>
#include <win/windows.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <wincred.h>
#include <utf8.hpp>

void save_credential(const std::string &alias, const std::string &password) {
  const auto alias_w = utf8::cvt<std::wstring>(alias);
  const auto password_w = utf8::cvt<std::wstring>(password);
  CREDENTIALW cred = {};
  cred.Type = CRED_TYPE_GENERIC;
  cred.TargetName = const_cast<LPWSTR>(alias_w.c_str());
  cred.CredentialBlobSize = static_cast<DWORD>(password_w.length() * 2 + 1);
  cred.CredentialBlob = (LPBYTE)password_w.c_str();
  cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

  if (!CredWrite(&cred, 0)) {
    throw nsclient::nsclient_exception("Failed to write credentials: " + alias);
  }
}

boost::optional<std::string> read_credential(const std::string &alias) {
  PCREDENTIALW pcred;
  const std::wstring alias_w = utf8::cvt<std::wstring>(alias);
  if (!CredRead(alias_w.c_str(), CRED_TYPE_GENERIC, 0, &pcred)) {
    return boost::optional<std::string>();
  }
  const std::wstring password_w(reinterpret_cast<wchar_t *>(pcred->CredentialBlob), pcred->CredentialBlobSize / sizeof(wchar_t));
  auto password = utf8::cvt<std::string>(password_w);
  CredFree(pcred);
  return password;
}