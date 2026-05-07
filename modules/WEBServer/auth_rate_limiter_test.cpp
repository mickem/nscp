#include "auth_rate_limiter.hpp"

#include <gtest/gtest.h>

TEST(AuthRateLimiter, NewIpIsNotBlocked) {
  auth_rate_limiter rl;
  EXPECT_FALSE(rl.is_blocked("1.2.3.4"));
}

TEST(AuthRateLimiter, BlocksAfterMaxFailures) {
  auth_rate_limiter rl;
  for (int i = 0; i < auth_rate_limiter::kMaxFailures; ++i) {
    EXPECT_FALSE(rl.is_blocked("1.2.3.4"));
    rl.record_failure("1.2.3.4");
  }
  EXPECT_TRUE(rl.is_blocked("1.2.3.4"));
}

TEST(AuthRateLimiter, SuccessClearsCounter) {
  auth_rate_limiter rl;
  for (int i = 0; i < auth_rate_limiter::kMaxFailures - 1; ++i) {
    rl.record_failure("1.2.3.4");
  }
  rl.record_success("1.2.3.4");
  // After the success, one more failure should not push us into blocked state.
  rl.record_failure("1.2.3.4");
  EXPECT_FALSE(rl.is_blocked("1.2.3.4"));
}

TEST(AuthRateLimiter, BlockingIsPerIp) {
  auth_rate_limiter rl;
  for (int i = 0; i < auth_rate_limiter::kMaxFailures; ++i) {
    rl.record_failure("1.2.3.4");
  }
  EXPECT_TRUE(rl.is_blocked("1.2.3.4"));
  EXPECT_FALSE(rl.is_blocked("5.6.7.8"));
}
