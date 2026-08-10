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
