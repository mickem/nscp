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

#include <atlbase.h>
#include <WbemCli.h>

#include <boost/lexical_cast.hpp>
#include <error/error.hpp>
#include <list>
#include <string>
#include <utility>

namespace wmi_impl {
class ComError {
 public:
  static std::wstring getWMIError(HRESULT hres) {
    switch (hres) {
      case S_OK:
        return L"";
      case WBEM_E_ACCESS_DENIED:
        return _T("The current user does not have permission to view the result set.");
      case WBEM_E_FAILED:
        return _T("This indicates other unspecified errors.");
      case WBEM_E_INVALID_PARAMETER:
        return _T("An invalid parameter was specified.");
      case WBEM_E_INVALID_QUERY:
        return _T("The query was not syntactically valid.");
      case WBEM_E_INVALID_QUERY_TYPE:
        return _T("The requested query language is not supported.");
      case WBEM_E_OUT_OF_MEMORY:
        return _T("There was not enough memory to complete the operation.");
      case WBEM_E_SHUTTING_DOWN:
        return _T("Windows Management service was stopped and restarted. A new call to ConnectServer is required.");
      case WBEM_E_TRANSPORT_FAILURE:
        return _T("This indicates the failure of the remote procedure call (RPC) link between the current process and Windows Management.");
      case WBEM_E_NOT_FOUND:
        return _T("The query specifies a class that does not exist.");
      default:
        return L"Unknown error.";
    }
  }
  static std::string getComError(HRESULT hr);
};

class wmi_exception : public std::exception {
  std::string message_;
  HRESULT code_;

 public:
  wmi_exception(const HRESULT code, const std::string& str) : code_(code) { message_ = str + ":" + error::format::from_system(code); }
  wmi_exception(const wmi_exception&) noexcept = default;
  wmi_exception& operator=(const wmi_exception&) noexcept = default;
  wmi_exception(wmi_exception&&) noexcept = default;
  wmi_exception& operator=(wmi_exception&&) noexcept = default;
  ~wmi_exception() noexcept override = default;
  const char* what() const noexcept override { return message_.c_str(); }

  const std::string& reason() const noexcept { return message_; }
  HRESULT get_code() const noexcept { return code_; }
};

struct row {
  CComPtr<IWbemClassObject> row_obj;
  const std::list<std::string>& columns;
  explicit row(const std::list<std::string>& columns) : row_obj(), columns(columns) {}

  std::string get_string(const std::string& col) const;
  std::string to_string() const;
  long long get_int(const std::string& col) const;
};

struct row_enumerator {
  row row_instance;
  CComPtr<IEnumWbemClassObject> enumerator_obj;
  explicit row_enumerator(const std::list<std::string>& columns) : row_instance(columns), enumerator_obj() {}
  bool has_next();
  row& get_next();
};
struct header_enumerator {
  CComPtr<IEnumWbemClassObject> enumerator_obj;
  header_enumerator() : enumerator_obj() {}
  std::list<std::string> get() const;
};
struct wmi_service {
  CComPtr<IWbemServices> service;
  std::string ns;
  std::string username;
  std::string password;
  bool is_initialized;
  wmi_service(std::string ns, std::string username, std::string password)
      : service(), ns(std::move(ns)), username(std::move(username)), password(std::move(password)), is_initialized(false) {}
  CComPtr<IWbemServices>& get();
};
struct query {
  std::string wql_query;
  wmi_service instance;
  std::list<std::string> columns;
  query(std::string wql_query, const std::string& ns, const std::string& username, const std::string& password)
      : wql_query(std::move(wql_query)), instance(ns, username, password) {}

  std::list<std::string> get_columns();
  row_enumerator execute();
};

struct instances {
  std::string super_class;
  wmi_service instance;
  std::list<std::string> columns;
  instances(std::string super_class, const std::string& ns, const std::string& username, const std::string& password)
      : super_class(std::move(super_class)), instance(ns, username, password) {}
  row_enumerator get();
};
struct classes {
  std::string super_class;
  wmi_service instance;
  std::list<std::string> columns;
  classes(std::string super_class, const std::string& ns, const std::string& username, const std::string& password)
      : super_class(std::move(super_class)), instance(ns, username, password) {}
  row_enumerator get();
};
}  // namespace wmi_impl