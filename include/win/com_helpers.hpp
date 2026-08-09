// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <Objbase.h>

#include <error/error.hpp>
#include <win/windows.hpp>

namespace com_helper {
class com_exception {
  std::string error_;
  HRESULT result_;

 public:
  com_exception(std::string error) : error_(error) {}
  com_exception(std::string error, HRESULT result) : error_(error), result_(result) { error_ += error::format::from_system(result); }
  std::string reason() { return error_; }
};

class initialize_com {
  bool isInitialized_;

 public:
  initialize_com() : isInitialized_(false) {}
  ~initialize_com() { unInitialize(); }

  void initialize() {
    HRESULT hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hRes)) throw com_exception("CoInitialize failed: ", hRes);
    isInitialized_ = true;
    // hRes = CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_PKT,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE,NULL);
    hRes = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    if (FAILED(hRes)) throw com_exception("CoInitializeSecurity failed: ", hRes);
  }
  void unInitialize() {
    if (!isInitialized_) return;
    CoUninitialize();
    isInitialized_ = false;
  }
  bool isInitialized() const { return isInitialized_; }
};

// Scoped COM (MTA) initialisation for the current thread. Balances the
// CoInitializeEx in the destructor on every exit path, including exceptions,
// and encodes the two subtle rules every hand-rolled copy had to re-derive:
//  - RPC_E_CHANGED_MODE means COM is already initialised on this thread in a
//    different apartment mode: it is usable (is_ready() is true) but must NOT
//    be uninitialised by us, so the destructor does nothing.
//  - CoInitializeSecurity is process-global and deliberately not called here.
class mta_scope {
  const HRESULT hr_;

 public:
  mta_scope() : hr_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
  ~mta_scope() {
    if (SUCCEEDED(hr_)) CoUninitialize();
  }
  mta_scope(const mta_scope &) = delete;
  mta_scope &operator=(const mta_scope &) = delete;

  // COM is usable on this thread (initialised by us, or already initialised).
  bool is_ready() const { return SUCCEEDED(hr_) || hr_ == RPC_E_CHANGED_MODE; }
  HRESULT result() const { return hr_; }
};
};  // namespace com_helper