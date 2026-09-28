// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#include "check_cluster.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <stdexcept>
#include <tuple>

#include "cluster_source_win.hpp"

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {
using namespace check_cluster;
using response_type = PB::Commands::QueryResponseMessage::Response;

response_type run(object_kind kind, const source &fetch, const std::vector<std::string> &arguments = {}) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_cluster");
  for (const auto &arg : arguments) request.add_arguments(arg);
  response_type response;
  check_from(request, &response, kind, fetch);
  return response;
}
response_type run_rows(object_kind kind, std::vector<record> rows, const std::vector<std::string> &arguments = {}) {
  return run(kind, [rows](object_kind) { return rows; }, arguments);
}
std::string message(const response_type &response) {
  std::string result;
  for (const auto &line : response.lines()) result += line.message();
  return result;
}

TEST(CheckCluster, DefaultThresholdsUseObjectSpecificStates) {
  using test_case = std::tuple<object_kind, long long, PB::Common::ResultCode>;
  const std::vector<test_case> cases = {{object_kind::group, 0, PB::Common::OK},           {object_kind::group, 1, PB::Common::CRITICAL},
                                        {object_kind::group, 2, PB::Common::CRITICAL},     {object_kind::group, 3, PB::Common::WARNING},
                                        {object_kind::group, 4, PB::Common::WARNING},      {object_kind::resource, 0, PB::Common::OK},
                                        {object_kind::resource, 1, PB::Common::WARNING},   {object_kind::resource, 2, PB::Common::OK},
                                        {object_kind::resource, 3, PB::Common::OK},        {object_kind::resource, 4, PB::Common::CRITICAL},
                                        {object_kind::resource, 128, PB::Common::WARNING}, {object_kind::resource, 129, PB::Common::WARNING},
                                        {object_kind::resource, 130, PB::Common::WARNING}, {object_kind::node, 0, PB::Common::OK},
                                        {object_kind::node, 1, PB::Common::CRITICAL},      {object_kind::node, 2, PB::Common::WARNING},
                                        {object_kind::node, 3, PB::Common::WARNING},       {object_kind::network, 0, PB::Common::CRITICAL},
                                        {object_kind::network, 1, PB::Common::CRITICAL},   {object_kind::network, 2, PB::Common::WARNING},
                                        {object_kind::network, 3, PB::Common::OK}};
  for (const auto &item : cases) {
    const auto kind = std::get<0>(item);
    const auto state = std::get<1>(item);
    SCOPED_TRACE(std::to_string(static_cast<int>(kind)) + "/" + std::to_string(state));
    const auto response = run_rows(kind, {{"role", "node-b", "SQL", "SQL Server", state}});
    EXPECT_EQ(response.result(), std::get<2>(item)) << message(response);
    EXPECT_NE(message(response).find(state_name(kind, state)), std::string::npos);
  }
}

TEST(CheckCluster, FailoverChangesOwnerWithoutLosingRoleOrAlerting) {
  for (const auto &owner : {"node-a", "node-b"}) {
    const auto response = run_rows(object_kind::group, {{"SQL", owner, "", "", 0}}, {"name=sql"});
    EXPECT_EQ(response.result(), PB::Common::OK);
    EXPECT_NE(message(response).find(std::string("owner=") + owner), std::string::npos);
  }
}

TEST(CheckCluster, FiltersAndDetailSyntaxUseOwnedResourceValues) {
  const auto response =
      run_rows(object_kind::resource, {{"DB", "node-b", "SQL", "SQL Server", 3}, {"Other", "node-a", "Other", "Generic Service", 4}},
               {"filter=group = 'SQL' and type = 'SQL Server'", "critical=state = 'offline'", "detail-syntax=${name}/${group}/${owner}/${type}/${state_id}"});
  EXPECT_EQ(response.result(), PB::Common::CRITICAL);
  EXPECT_NE(message(response).find("DB/SQL/node-b/SQL Server/3"), std::string::npos);
  EXPECT_EQ(message(response).find("Other"), std::string::npos);
}

TEST(CheckCluster, ExplicitMissingNameCannotBeHiddenByEmptyState) {
  const auto response = run_rows(object_kind::group, {{"SQL", "node-a", "", "", 0}}, {"name=missing", "empty-state=ok"});
  EXPECT_EQ(response.result(), PB::Common::UNKNOWN);
  EXPECT_EQ(message(response), "Cluster object 'missing' not found");
}

TEST(CheckCluster, EmptySnapshotAndEmptyFilterHaveExplicitContracts) {
  for (const auto kind : {object_kind::group, object_kind::resource, object_kind::node, object_kind::network}) {
    EXPECT_EQ(run_rows(kind, {}).result(), PB::Common::UNKNOWN);
    EXPECT_EQ(run_rows(kind, {}, {"empty-state=ok"}).result(), PB::Common::OK);
    const auto response = run_rows(kind, {{"x", "", "", "", 0}}, {"filter=name = 'missing'"});
    EXPECT_EQ(response.result(), PB::Common::UNKNOWN);
    EXPECT_NE(message(response).find("matched"), std::string::npos);
  }
}

TEST(CheckCluster, UnknownStatesAreUnknownEvenWithDisabledThresholds) {
  for (const auto kind : {object_kind::group, object_kind::resource, object_kind::node, object_kind::network}) {
    for (const auto state : {-1, 999}) {
      const auto response = run_rows(kind, {{"x", "", "", "", state}}, {"warning=state_id < -2", "critical=state_id < -2"});
      EXPECT_EQ(response.result(), PB::Common::UNKNOWN);
      EXPECT_NE(message(response).find(std::to_string(state)), std::string::npos);
    }
  }
}

TEST(CheckCluster, AcquisitionFailureNeverBecomesEmptySuccess) {
  const auto response = run(object_kind::group, [](object_kind) -> std::vector<record> { throw std::runtime_error("access denied"); }, {"empty-state=ok"});
  EXPECT_EQ(response.result(), PB::Common::UNKNOWN);
  EXPECT_NE(message(response).find("access denied"), std::string::npos);
}

TEST(CheckCluster, HelpDoesNotAcquireCluster) {
  bool called = false;
  const auto response = run(object_kind::group,
                            [&called](object_kind) {
                              called = true;
                              return std::vector<record>{};
                            },
                            {"help"});
  EXPECT_FALSE(called);
  EXPECT_NE(message(response).find("name"), std::string::npos);
}

// Native adapter tests use the real Windows signatures; only the calls into
// the OS are replaced. No test fixture is exposed in production arguments.
struct fake_cluster {
  std::wstring name = L"role";
  std::wstring owner = L"node-b";
  std::wstring group = L"SQL";
  std::wstring type = L"SQL Server";
  DWORD enumeration_type = 0;
  DWORD open_error = 0;
  DWORD enum_error = 0;
  DWORD state_error = 0;
  int cluster_closes = 0, enum_closes = 0, object_closes = 0;
  int enum_calls = 0, state_calls = 0, type_calls = 0;
  bool fail_after_first = false;
};
fake_cluster *active = nullptr;

DWORD copy_string(const std::wstring &value, LPWSTR buffer, LPDWORD length) {
  const DWORD capacity = *length;
  *length = static_cast<DWORD>(value.size());
  if (capacity <= value.size()) return ERROR_MORE_DATA;
  std::copy(value.begin(), value.end(), buffer);
  buffer[value.size()] = 0;
  return ERROR_SUCCESS;
}
HCLUSTER WINAPI open_cluster(LPCWSTR name, DWORD access, LPDWORD) {
  EXPECT_EQ(name, nullptr);
  EXPECT_EQ(access, GENERIC_READ);
  if (active->open_error) {
    SetLastError(active->open_error);
    return nullptr;
  }
  return reinterpret_cast<HCLUSTER>(1);
}
BOOL WINAPI close_cluster(HCLUSTER) {
  ++active->cluster_closes;
  return TRUE;
}
HCLUSENUM WINAPI open_enum(HCLUSTER, DWORD type) {
  active->enumeration_type = type;
  return reinterpret_cast<HCLUSENUM>(2);
}
DWORD WINAPI close_enum(HCLUSENUM) {
  ++active->enum_closes;
  return ERROR_SUCCESS;
}
DWORD WINAPI enumerate(HCLUSENUM, DWORD index, LPDWORD type, LPWSTR name, LPDWORD length) {
  ++active->enum_calls;
  if (active->enum_error) return active->enum_error;
  if (index != 0) return active->fail_after_first ? ERROR_ACCESS_DENIED : ERROR_NO_MORE_ITEMS;
  *type = active->enumeration_type;
  return copy_string(active->name, name, length);
}
HGROUP WINAPI open_group(HCLUSTER, LPCWSTR, DWORD access, LPDWORD) {
  EXPECT_EQ(access, GENERIC_READ);
  return reinterpret_cast<HGROUP>(3);
}
BOOL WINAPI close_group(HGROUP) {
  ++active->object_closes;
  return TRUE;
}
CLUSTER_GROUP_STATE WINAPI group_state(HGROUP, LPWSTR owner, LPDWORD length) {
  ++active->state_calls;
  const DWORD error = active->state_error ? active->state_error : copy_string(active->owner, owner, length);
  SetLastError(error);
  return error ? ClusterGroupStateUnknown : ClusterGroupOnline;
}
HRESOURCE WINAPI open_resource(HCLUSTER, LPCWSTR, DWORD access, LPDWORD) {
  EXPECT_EQ(access, GENERIC_READ);
  return reinterpret_cast<HRESOURCE>(4);
}
BOOL WINAPI close_resource(HRESOURCE) {
  ++active->object_closes;
  return TRUE;
}
CLUSTER_RESOURCE_STATE WINAPI resource_state(HRESOURCE, LPWSTR owner, LPDWORD owner_length, LPWSTR group, LPDWORD group_length) {
  ++active->state_calls;
  const auto owner_error = copy_string(active->owner, owner, owner_length);
  const auto group_error = copy_string(active->group, group, group_length);
  const DWORD error = active->state_error ? active->state_error : (owner_error ? owner_error : group_error);
  SetLastError(error);
  return error ? ClusterResourceStateUnknown : ClusterResourceOnline;
}
DWORD WINAPI resource_control(HRESOURCE, HNODE, DWORD code, LPVOID, DWORD, LPVOID buffer, DWORD bytes, LPDWORD returned) {
  EXPECT_EQ(code, static_cast<DWORD>(CLUSCTL_RESOURCE_GET_RESOURCE_TYPE));
  ++active->type_calls;
  *returned = static_cast<DWORD>((active->type.size() + 1) * sizeof(wchar_t));
  DWORD length = bytes / sizeof(wchar_t);
  return copy_string(active->type, static_cast<LPWSTR>(buffer), &length);
}
HNODE WINAPI open_node(HCLUSTER, LPCWSTR, DWORD access, LPDWORD) {
  EXPECT_EQ(access, GENERIC_READ);
  return reinterpret_cast<HNODE>(5);
}
BOOL WINAPI close_node(HNODE) {
  ++active->object_closes;
  return TRUE;
}
CLUSTER_NODE_STATE WINAPI node_state(HNODE) {
  if (active->state_error) {
    SetLastError(active->state_error);
    return ClusterNodeStateUnknown;
  }
  return ClusterNodePaused;
}
HNETWORK WINAPI open_network(HCLUSTER, LPCWSTR, DWORD access, LPDWORD) {
  EXPECT_EQ(access, GENERIC_READ);
  return reinterpret_cast<HNETWORK>(6);
}
BOOL WINAPI close_network(HNETWORK) {
  ++active->object_closes;
  return TRUE;
}
CLUSTER_NETWORK_STATE WINAPI network_state(HNETWORK) { return ClusterNetworkPartitioned; }

class ClusterNative : public ::testing::Test {
 protected:
  fake_cluster fake;
  native::api api;
  void SetUp() override {
    active = &fake;
    api.open_cluster = open_cluster;
    api.close_cluster = close_cluster;
    api.open_enum = open_enum;
    api.enumerate = enumerate;
    api.close_enum = close_enum;
    api.open_group = open_group;
    api.close_group = close_group;
    api.group_state = group_state;
    api.open_resource = open_resource;
    api.close_resource = close_resource;
    api.resource_state = resource_state;
    api.resource_control = resource_control;
    api.open_node = open_node;
    api.close_node = close_node;
    api.node_state = node_state;
    api.open_network = open_network;
    api.close_network = close_network;
    api.network_state = network_state;
  }
  void TearDown() override { active = nullptr; }
};

TEST_F(ClusterNative, ResizesNamesAndOwnersAndClosesAllHandles) {
  fake.name = std::wstring(400, L'x') + L"\u00e5";
  fake.owner = std::wstring(400, L'n');
  const auto rows = native::fetch(api, object_kind::group);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows.front().name, std::string(400, 'x') + "\xc3\xa5");
  EXPECT_EQ(rows.front().owner, std::string(400, 'n'));
  EXPECT_EQ(fake.enumeration_type, static_cast<DWORD>(CLUSTER_ENUM_GROUP));
  EXPECT_EQ(fake.enum_calls, 3);
  EXPECT_EQ(fake.state_calls, 2);
  EXPECT_EQ(fake.cluster_closes, 1);
  EXPECT_EQ(fake.enum_closes, 1);
  EXPECT_EQ(fake.object_closes, 1);
}

TEST_F(ClusterNative, ResourceBuffersUseCharactersForOwnersAndBytesForTypes) {
  fake.owner = std::wstring(400, L'n');
  fake.group = std::wstring(600, L'g');
  fake.type = std::wstring(700, L't');
  const auto rows = native::fetch(api, object_kind::resource);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows.front().owner, std::string(400, 'n'));
  EXPECT_EQ(rows.front().group, std::string(600, 'g'));
  EXPECT_EQ(rows.front().type, std::string(700, 't'));
  EXPECT_EQ(rows.front().state_id, 2);
  EXPECT_EQ(fake.type_calls, 2);
  EXPECT_EQ(fake.object_closes, 1);
}

TEST_F(ClusterNative, NodeAndNetworkReadersSelectCorrectEnumeration) {
  EXPECT_EQ(native::fetch(api, object_kind::node).front().state_id, 2);
  EXPECT_EQ(fake.enumeration_type, static_cast<DWORD>(CLUSTER_ENUM_NODE));
  EXPECT_EQ(native::fetch(api, object_kind::network).front().state_id, 2);
  EXPECT_EQ(fake.enumeration_type, static_cast<DWORD>(CLUSTER_ENUM_NETWORK));
  EXPECT_EQ(fake.object_closes, 2);
}

TEST_F(ClusterNative, OpenFailureIsUnknownAndDoesNotCloseInvalidHandles) {
  fake.open_error = ERROR_ACCESS_DENIED;
  const auto response = run(object_kind::group, [this](object_kind kind) { return native::fetch(api, kind); });
  EXPECT_EQ(response.result(), PB::Common::UNKNOWN);
  EXPECT_NE(message(response).find("Windows error 5"), std::string::npos);
  EXPECT_EQ(fake.cluster_closes, 0);
}

TEST_F(ClusterNative, StateFailureClosesAllHandles) {
  fake.state_error = ERROR_ACCESS_DENIED;
  EXPECT_THROW(native::fetch(api, object_kind::group), std::runtime_error);
  EXPECT_EQ(fake.cluster_closes, 1);
  EXPECT_EQ(fake.enum_closes, 1);
  EXPECT_EQ(fake.object_closes, 1);
}

TEST_F(ClusterNative, NativeErrorSurvivesObjectNameConversion) {
  fake.name = L"n\u00f6de";
  fake.state_error = ERROR_ACCESS_DENIED;
  const auto response = run(object_kind::node, [this](object_kind kind) { return native::fetch(api, kind); });
  EXPECT_EQ(response.result(), PB::Common::UNKNOWN);
  EXPECT_NE(message(response).find("GetClusterNodeState 'n\xc3\xb6"
                                   "de' (Windows error 5)"),
            std::string::npos);
  EXPECT_EQ(fake.cluster_closes, 1);
  EXPECT_EQ(fake.enum_closes, 1);
  EXPECT_EQ(fake.object_closes, 1);
}

TEST_F(ClusterNative, PartialEnumerationFailsWholeCheck) {
  fake.fail_after_first = true;
  const auto response = run(object_kind::group, [this](object_kind kind) { return native::fetch(api, kind); }, {"empty-state=ok"});
  EXPECT_EQ(response.result(), PB::Common::UNKNOWN);
  EXPECT_NE(message(response).find("ClusterEnum"), std::string::npos);
  EXPECT_EQ(fake.cluster_closes, 1);
  EXPECT_EQ(fake.enum_closes, 1);
  EXPECT_EQ(fake.object_closes, 1);
}

TEST_F(ClusterNative, RepeatedBufferGrowthIsBounded) {
  fake.enum_error = ERROR_MORE_DATA;
  EXPECT_THROW(native::fetch(api, object_kind::group), std::runtime_error);
  EXPECT_EQ(fake.enum_calls, 8);
  EXPECT_EQ(fake.cluster_closes, 1);
  EXPECT_EQ(fake.enum_closes, 1);
}
}  // namespace
