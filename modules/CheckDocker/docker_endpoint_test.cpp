// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <string>

#include "docker_endpoint.hpp"

using docker_checks::default_docker_endpoint;
using docker_checks::is_local_docker_endpoint;

namespace {
bool accepted(const std::string &host) {
  std::string error;
  return is_local_docker_endpoint(host, error);
}
std::string rejection(const std::string &host) {
  std::string error;
  EXPECT_FALSE(is_local_docker_endpoint(host, error)) << "expected '" << host << "' to be rejected";
  return error;
}
}  // namespace

TEST(docker_endpoint, the_platform_default_is_accepted) { EXPECT_TRUE(accepted(default_docker_endpoint())); }

TEST(docker_endpoint, empty_is_rejected) { EXPECT_FALSE(rejection("").empty()); }

#ifdef WIN32

// --- Windows: only the local device pipe namespace ---

TEST(docker_endpoint, local_pipe_is_accepted) {
  EXPECT_TRUE(accepted("\\\\.\\pipe\\docker_engine"));
  EXPECT_TRUE(accepted("\\\\?\\pipe\\docker_engine"));
  EXPECT_TRUE(accepted("\\\\.\\pipe\\some-other-name"));
}

TEST(docker_endpoint, unc_host_is_rejected) {
  // The finding: a UNC target makes Windows authenticate outbound over SMB as
  // the service account.
  EXPECT_FALSE(rejection("\\\\attacker\\pipe\\docker_engine").empty());
  EXPECT_FALSE(rejection("\\\\10.0.0.5\\pipe\\x").empty());
  EXPECT_FALSE(rejection("\\\\evil.example.com\\pipe\\docker_engine").empty());
}

TEST(docker_endpoint, unc_rejection_explains_why) {
  const std::string error = rejection("\\\\attacker\\pipe\\docker_engine");
  EXPECT_NE(error.find("SMB"), std::string::npos) << error;
}

TEST(docker_endpoint, rejection_names_both_accepted_forms) {
  // The message has to describe what is actually allowed, or an operator using
  // the \\?\ form is told their working endpoint is the only illegal one.
  const std::string error = rejection("\\\\attacker\\pipe\\docker_engine");
  EXPECT_NE(error.find("\\\\.\\pipe\\"), std::string::npos) << error;
  EXPECT_NE(error.find("\\\\?\\pipe\\"), std::string::npos) << error;
}

TEST(docker_endpoint, the_prefix_is_matched_case_insensitively) {
  // Win32 path and pipe names are case-insensitive, so these all name the same
  // object as the canonical spelling and must not be refused.
  EXPECT_TRUE(accepted("\\\\.\\Pipe\\docker_engine"));
  EXPECT_TRUE(accepted("\\\\.\\PIPE\\docker_engine"));
  EXPECT_TRUE(accepted("\\\\?\\Pipe\\docker_engine"));
}

TEST(docker_endpoint, case_insensitivity_does_not_widen_past_the_local_namespace) {
  // Folding case must not turn a UNC host into an accepted endpoint.
  EXPECT_FALSE(rejection("\\\\ATTACKER\\pipe\\docker_engine").empty());
  EXPECT_FALSE(rejection("\\\\Attacker\\PIPE\\x").empty());
}

TEST(docker_endpoint, non_pipe_paths_are_rejected) {
  EXPECT_FALSE(rejection("C:\\windows\\system32\\config\\sam").empty());
  EXPECT_FALSE(rejection("\\\\.\\C:").empty());
  EXPECT_FALSE(rejection("docker_engine").empty());
  EXPECT_FALSE(rejection("http://attacker/").empty());
}

TEST(docker_endpoint, pipe_name_may_not_contain_separators) {
  // No walking back out of the pipe namespace.
  EXPECT_FALSE(rejection("\\\\.\\pipe\\..\\..\\x").empty());
  EXPECT_FALSE(rejection("\\\\.\\pipe\\a\\b").empty());
  EXPECT_FALSE(rejection("\\\\.\\pipe\\a/b").empty());
  EXPECT_FALSE(rejection("\\\\.\\pipe\\").empty());
}

#else

// --- POSIX: an absolute, non-traversing socket path ---

TEST(docker_endpoint, absolute_socket_path_is_accepted) {
  EXPECT_TRUE(accepted("/var/run/docker.sock"));
  EXPECT_TRUE(accepted("/run/docker.sock"));
}

TEST(docker_endpoint, relative_paths_are_rejected) {
  EXPECT_FALSE(rejection("docker.sock").empty());
  EXPECT_FALSE(rejection("../docker.sock").empty());
  EXPECT_FALSE(rejection("attacker.example.com").empty());
}

TEST(docker_endpoint, traversal_is_rejected) {
  EXPECT_FALSE(rejection("/var/run/../../etc/passwd").empty());
  EXPECT_FALSE(rejection("/var/run/..").empty());
}

#endif

// ---------------------------------------------------------------------------
// is_configured_docker_endpoint: even a perfectly local endpoint is the
// operator's to choose. A caller who can name it makes the agent connect to
// any socket or pipe on the host and reports back what happened, which is a
// read-only probe with the agent's privileges.
// ---------------------------------------------------------------------------

namespace {
std::string configured_rejection(const std::string &requested, const std::string &configured) {
  std::string error;
  if (docker_checks::is_configured_docker_endpoint(requested, configured, error)) return "";
  return error.empty() ? "<refused without a reason>" : error;
}
}  // namespace

TEST(docker_endpoint_configured, repeating_the_configured_endpoint_is_allowed) {
  EXPECT_EQ(configured_rejection("/run/docker.sock", "/run/docker.sock"), "");
}

TEST(docker_endpoint_configured, repeating_the_platform_default_is_allowed_when_nothing_is_configured) {
  EXPECT_EQ(configured_rejection(docker_checks::default_docker_endpoint(), ""), "");
}

TEST(docker_endpoint_configured, another_local_endpoint_is_refused) {
  const std::string error = configured_rejection("/run/user/1000/docker.sock", "/run/docker.sock");
  ASSERT_FALSE(error.empty());
  EXPECT_NE(error.find("[/settings/docker]"), std::string::npos) << error;
}

TEST(docker_endpoint_configured, the_refusal_does_not_echo_the_requested_path) {
  // Echoing it back is half the oracle: it confirms the agent read the value.
  const std::string error = configured_rejection("/run/secret-probe.sock", "/run/docker.sock");
  EXPECT_EQ(error.find("/run/secret-probe.sock"), std::string::npos) << error;
}

#ifdef WIN32
TEST(docker_endpoint_configured, pipe_names_compare_case_insensitively) {
  EXPECT_EQ(configured_rejection("\\\\.\\PIPE\\Docker_Engine", "\\\\.\\pipe\\docker_engine"), "");
}
#endif
