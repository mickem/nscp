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

#include <gtest/gtest.h>

#ifdef WIN32

#include <wmi/wmi_query.hpp>

// Test fixture for WMI tests that require COM initialization
class WmiQueryTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    com_initialized_ = SUCCEEDED(hr) || hr == S_FALSE;  // S_FALSE means already initialized
    const HRESULT hRes = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    if (FAILED(hRes)) {
      // print error
    }
  }

  static void TearDownTestSuite() {
    if (com_initialized_) {
      CoUninitialize();
    }
  }

  static bool com_initialized_;
};

bool WmiQueryTest::com_initialized_ = false;

// ============================================================================
// ComError tests
// ============================================================================

TEST(ComErrorTest, GetWMIErrorAccessDenied) {
  const std::wstring error = wmi_impl::ComError::getWMIError(WBEM_E_ACCESS_DENIED);
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(error, L"The current user does not have permission to view the result set.");
}

TEST(ComErrorTest, GetWMIErrorFailed) {
  const std::wstring error = wmi_impl::ComError::getWMIError(WBEM_E_FAILED);
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(error, L"This indicates other unspecified errors.");
}

TEST(ComErrorTest, GetWMIErrorInvalidParameter) {
  const std::wstring error = wmi_impl::ComError::getWMIError(WBEM_E_INVALID_PARAMETER);
  EXPECT_EQ(error, L"An invalid parameter was specified.");
}

TEST(ComErrorTest, GetWMIErrorInvalidQuery) {
  const std::wstring error = wmi_impl::ComError::getWMIError(WBEM_E_INVALID_QUERY);
  EXPECT_EQ(error, L"The query was not syntactically valid.");
}

TEST(ComErrorTest, GetWMIErrorInvalidQueryType) {
  const std::wstring error = wmi_impl::ComError::getWMIError(WBEM_E_INVALID_QUERY_TYPE);
  EXPECT_EQ(error, L"The requested query language is not supported.");
}

TEST(ComErrorTest, GetWMIErrorOutOfMemory) {
  const std::wstring error = wmi_impl::ComError::getWMIError(WBEM_E_OUT_OF_MEMORY);
  EXPECT_EQ(error, L"There was not enough memory to complete the operation.");
}

TEST(ComErrorTest, GetWMIErrorShuttingDown) {
  const std::wstring error = wmi_impl::ComError::getWMIError(WBEM_E_SHUTTING_DOWN);
  EXPECT_EQ(error, L"Windows Management service was stopped and restarted. A new call to ConnectServer is required.");
}

TEST(ComErrorTest, GetWMIErrorTransportFailure) {
  const std::wstring error = wmi_impl::ComError::getWMIError(WBEM_E_TRANSPORT_FAILURE);
  EXPECT_EQ(error, L"This indicates the failure of the remote procedure call (RPC) link between the current process and Windows Management.");
}

TEST(ComErrorTest, GetWMIErrorNotFound) {
  const std::wstring error = wmi_impl::ComError::getWMIError(WBEM_E_NOT_FOUND);
  EXPECT_EQ(error, L"The query specifies a class that does not exist.");
}

TEST(ComErrorTest, GetWMIErrorUnknown) {
  const std::wstring error = wmi_impl::ComError::getWMIError(0x12345678);
  EXPECT_EQ(error, L"Unknown error.");
}

TEST(ComErrorTest, GetWMIErrorSuccess) {
  const std::wstring error = wmi_impl::ComError::getWMIError(S_OK);
  EXPECT_EQ(error, L"");
}

// ============================================================================
// wmi_exception tests
// ============================================================================

TEST(WmiExceptionTest, ConstructorWithMessage) {
  const wmi_impl::wmi_exception ex(E_FAIL, "Test error message");
  EXPECT_NE(std::string(ex.what()).find("Test error message"), std::string::npos);
}

TEST(WmiExceptionTest, ConstructorWithSuccessCode) {
  const wmi_impl::wmi_exception ex(S_OK, "Success error");
  EXPECT_NE(std::string(ex.what()).find("Success error"), std::string::npos);
}

TEST(WmiExceptionTest, GetCode) {
  const wmi_impl::wmi_exception ex(WBEM_E_INVALID_QUERY, "Invalid query");
  EXPECT_EQ(ex.get_code(), WBEM_E_INVALID_QUERY);
}

TEST(WmiExceptionTest, ReasonContainsMessage) {
  const wmi_impl::wmi_exception ex(E_ACCESSDENIED, "Access denied error");
  std::string reason = ex.reason();
  EXPECT_NE(reason.find("Access denied error"), std::string::npos);
}

TEST(WmiExceptionTest, WhatEqualsReason) {
  const wmi_impl::wmi_exception ex(E_FAIL, "Test message");
  EXPECT_EQ(std::string(ex.what()), ex.reason());
}

TEST(WmiExceptionTest, InheritsFromStdException) {
  const wmi_impl::wmi_exception ex(E_FAIL, "Test");
  const std::exception* base_ptr = &ex;
  EXPECT_NE(base_ptr->what(), nullptr);
}

// ============================================================================
// wmi_service tests
// ============================================================================

TEST(WmiServiceTest, ConstructorInitializesFields) {
  const wmi_impl::wmi_service svc("root\\cimv2", "user", "pass");
  EXPECT_EQ(svc.ns, "root\\cimv2");
  EXPECT_EQ(svc.username, "user");
  EXPECT_EQ(svc.password, "pass");
  EXPECT_FALSE(svc.is_initialized);
}

TEST(WmiServiceTest, ConstructorWithEmptyCredentials) {
  const wmi_impl::wmi_service svc("root\\cimv2", "", "");
  EXPECT_EQ(svc.ns, "root\\cimv2");
  EXPECT_TRUE(svc.username.empty());
  EXPECT_TRUE(svc.password.empty());
  EXPECT_FALSE(svc.is_initialized);
}

// ============================================================================
// query tests
// ============================================================================

TEST(QueryTest, ConstructorInitializesFields) {
  const wmi_impl::query q("SELECT * FROM Win32_Process", "root\\cimv2", "", "");
  EXPECT_EQ(q.wql_query, "SELECT * FROM Win32_Process");
  EXPECT_TRUE(q.columns.empty());
}

TEST(QueryTest, ConstructorWithCredentials) {
  const wmi_impl::query q("SELECT Name FROM Win32_Service", "root\\cimv2", "admin", "password123");
  EXPECT_EQ(q.wql_query, "SELECT Name FROM Win32_Service");
}

// ============================================================================
// instances tests
// ============================================================================

TEST(InstancesTest, ConstructorInitializesFields) {
  const wmi_impl::instances inst("Win32_Process", "root\\cimv2", "", "");
  EXPECT_EQ(inst.super_class, "Win32_Process");
  EXPECT_TRUE(inst.columns.empty());
}

// ============================================================================
// classes tests
// ============================================================================

TEST(ClassesTest, ConstructorInitializesFields) {
  const wmi_impl::classes cls("Win32_Process", "root\\cimv2", "", "");
  EXPECT_EQ(cls.super_class, "Win32_Process");
  EXPECT_TRUE(cls.columns.empty());
}

// ============================================================================
// row tests
// ============================================================================

TEST(RowTest, ConstructorStoresColumns) {
  const std::list<std::string> columns = {"Name", "ProcessId", "Status"};
  const wmi_impl::row r(columns);
  EXPECT_EQ(r.columns.size(), 3);
}

TEST(RowTest, ConstructorWithEmptyColumns) {
  const std::list<std::string> columns;
  const wmi_impl::row r(columns);
  EXPECT_TRUE(r.columns.empty());
}

// ============================================================================
// row_enumerator tests
// ============================================================================

TEST(RowEnumeratorTest, ConstructorStoresColumns) {
  const std::list<std::string> columns = {"Col1", "Col2"};
  const wmi_impl::row_enumerator re(columns);
  EXPECT_EQ(re.row_instance.columns.size(), 2);
}

// ============================================================================
// Integration tests (require actual WMI connection)
// ============================================================================

TEST_F(WmiQueryTest, QueryExecuteBasicQuery) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  try {
    wmi_impl::query q("SELECT Name FROM Win32_ComputerSystem", "root\\cimv2", "", "");
    const auto columns = q.get_columns();
    EXPECT_FALSE(columns.empty());

    auto enumerator = q.execute();
    EXPECT_TRUE(enumerator.has_next());
    auto& row = enumerator.get_next();
    const std::string name = row.get_string("Name");
    EXPECT_FALSE(name.empty());
  } catch (const wmi_impl::wmi_exception& ex) {
    // WMI access might be restricted in test environment
    GTEST_SKIP() << "WMI access failed: " << ex.what();
  }
}

TEST_F(WmiQueryTest, QueryExecuteWithMultipleRows) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  try {
    wmi_impl::query q("SELECT Name, ProcessId FROM Win32_Process", "root\\cimv2", "", "");
    auto enumerator = q.execute();

    int count = 0;
    while (enumerator.has_next()) {
      const auto& row = enumerator.get_next();
      // Just verify we can iterate - don't assert on specific values
      count++;
      if (count > 5) break;  // Don't iterate through all processes
    }
    EXPECT_GT(count, 0);
  } catch (const wmi_impl::wmi_exception& ex) {
    GTEST_SKIP() << "WMI access failed: " << ex.what();
  }
}

TEST_F(WmiQueryTest, QueryGetColumns) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  try {
    wmi_impl::query q("SELECT Name, ProcessId FROM Win32_Process", "root\\cimv2", "", "");
    const auto columns = q.get_columns();
    EXPECT_FALSE(columns.empty());

    // Should contain at least Name and ProcessId
    bool has_name = false;
    bool has_process_id = false;
    for (const auto& col : columns) {
      if (col == "Name") has_name = true;
      if (col == "ProcessId") has_process_id = true;
    }
    EXPECT_TRUE(has_name);
    EXPECT_TRUE(has_process_id);
  } catch (const wmi_impl::wmi_exception& ex) {
    GTEST_SKIP() << "WMI access failed: " << ex.what();
  }
}

TEST_F(WmiQueryTest, QueryInvalidQueryThrows) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  wmi_impl::query q("INVALID QUERY SYNTAX", "root\\cimv2", "", "");
  EXPECT_THROW(q.execute(), wmi_impl::wmi_exception);
}

TEST_F(WmiQueryTest, QueryInvalidNamespaceThrows) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  wmi_impl::query q("SELECT * FROM Win32_Process", "root\\invalid_namespace_12345", "", "");
  EXPECT_THROW(q.execute(), wmi_impl::wmi_exception);
}

TEST_F(WmiQueryTest, RowGetInt) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  try {
    wmi_impl::query q("SELECT ProcessId FROM Win32_Process", "root\\cimv2", "", "");
    auto enumerator = q.execute();

    if (enumerator.has_next()) {
      auto& row = enumerator.get_next();
      const long long pid = row.get_int("ProcessId");
      EXPECT_GE(pid, 0);
    }
  } catch (const wmi_impl::wmi_exception& ex) {
    GTEST_SKIP() << "WMI access failed: " << ex.what();
  }
}

TEST_F(WmiQueryTest, RowToString) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  try {
    wmi_impl::query q("SELECT Name FROM Win32_ComputerSystem", "root\\cimv2", "", "");
    q.get_columns();  // Initialize columns
    auto enumerator = q.execute();

    if (enumerator.has_next()) {
      auto& row = enumerator.get_next();
      const std::string str = row.to_string();
      EXPECT_FALSE(str.empty());
    }
  } catch (const wmi_impl::wmi_exception& ex) {
    GTEST_SKIP() << "WMI access failed: " << ex.what();
  }
}

TEST_F(WmiQueryTest, InstancesGet) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  try {
    wmi_impl::instances inst("Win32_ComputerSystem", "root\\cimv2", "", "");
    auto enumerator = inst.get();

    EXPECT_TRUE(enumerator.has_next());
  } catch (const wmi_impl::wmi_exception& ex) {
    GTEST_SKIP() << "WMI access failed: " << ex.what();
  }
}

TEST_F(WmiQueryTest, ClassesGet) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  try {
    wmi_impl::classes cls("", "root\\cimv2", "", "");
    auto enumerator = cls.get();

    // Should have at least one class in root\cimv2
    EXPECT_TRUE(enumerator.has_next());
  } catch (const wmi_impl::wmi_exception& ex) {
    GTEST_SKIP() << "WMI access failed: " << ex.what();
  }
}

TEST_F(WmiQueryTest, RowGetStringNonExistentColumn) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  try {
    wmi_impl::query q("SELECT Name FROM Win32_ComputerSystem", "root\\cimv2", "", "");
    auto enumerator = q.execute();

    if (enumerator.has_next()) {
      auto& row = enumerator.get_next();
      EXPECT_THROW(row.get_string("NonExistentColumn12345"), wmi_impl::wmi_exception);
    }
  } catch (const wmi_impl::wmi_exception& ex) {
    GTEST_SKIP() << "WMI access failed: " << ex.what();
  }
}

TEST_F(WmiQueryTest, WmiServiceGet) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  try {
    wmi_impl::wmi_service svc("root\\cimv2", "", "");
    auto& service = svc.get();
    EXPECT_TRUE(svc.is_initialized);
    EXPECT_NE(service.p, nullptr);

    // Calling get() again should return cached service
    auto& service2 = svc.get();
    EXPECT_EQ(service.p, service2.p);
  } catch (const wmi_impl::wmi_exception& ex) {
    GTEST_SKIP() << "WMI access failed: " << ex.what();
  }
}

TEST_F(WmiQueryTest, WmiServiceInvalidNamespace) {
  if (!com_initialized_) {
    GTEST_SKIP() << "COM not initialized";
  }

  wmi_impl::wmi_service svc("root\\nonexistent_namespace_xyz", "", "");
  EXPECT_THROW(svc.get(), wmi_impl::wmi_exception);
}

#endif  // WIN32
