// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "auth_rate_limiter.hpp"

#include <gtest/gtest.h>

TEST(AuthRateLimiter, NewIpIsNotBlocked) {
  auth_rate_limiter rl;
  EXPECT_FALSE(rl.is_blocked("1.2.3.4"));
}

TEST(AuthRateLimiter, BlocksAfterMaxFailures) {
  auth_rate_limiter rl;
  for (int i = 0; i < auth_rate_limiter::kDefaultMaxFailures; ++i) {
    EXPECT_FALSE(rl.is_blocked("1.2.3.4"));
    rl.record_failure("1.2.3.4");
  }
  EXPECT_TRUE(rl.is_blocked("1.2.3.4"));
}

TEST(AuthRateLimiter, SuccessClearsCounter) {
  auth_rate_limiter rl;
  for (int i = 0; i < auth_rate_limiter::kDefaultMaxFailures - 1; ++i) {
    rl.record_failure("1.2.3.4");
  }
  rl.record_success("1.2.3.4");
  // After the success, one more failure should not push us into blocked state.
  rl.record_failure("1.2.3.4");
  EXPECT_FALSE(rl.is_blocked("1.2.3.4"));
}

TEST(AuthRateLimiter, BlockingIsPerIp) {
  auth_rate_limiter rl;
  for (int i = 0; i < auth_rate_limiter::kDefaultMaxFailures; ++i) {
    rl.record_failure("1.2.3.4");
  }
  EXPECT_TRUE(rl.is_blocked("1.2.3.4"));
  EXPECT_FALSE(rl.is_blocked("5.6.7.8"));
}

TEST(AuthRateLimiter, DisabledWhenMaxFailuresZero) {
  auth_rate_limiter rl;
  rl.set_max_failures(0);
  for (int i = 0; i < 1000; ++i) rl.record_failure("1.2.3.4");
  EXPECT_FALSE(rl.is_blocked("1.2.3.4"));
}

TEST(AuthRateLimiter, BlockDurationDoublesPerOffense) {
  // 1-based offense count: the first block is the configured base.
  EXPECT_EQ(auth_rate_limiter::block_duration_seconds(60, 1), 60);
  EXPECT_EQ(auth_rate_limiter::block_duration_seconds(60, 2), 120);
  EXPECT_EQ(auth_rate_limiter::block_duration_seconds(60, 3), 240);
  EXPECT_EQ(auth_rate_limiter::block_duration_seconds(60, 4), 480);
}

TEST(AuthRateLimiter, BlockDurationIsCapped) {
  // Six doublings of 60 s would be 64 min; the ceiling clamps it, and it stays
  // there no matter how many further offenses accumulate.
  EXPECT_EQ(auth_rate_limiter::block_duration_seconds(60, 6), 1920);
  EXPECT_EQ(auth_rate_limiter::block_duration_seconds(60, 7), auth_rate_limiter::kMaxBlockSeconds);
  EXPECT_EQ(auth_rate_limiter::block_duration_seconds(60, 100), auth_rate_limiter::kMaxBlockSeconds);
}

TEST(AuthRateLimiter, BlockDurationNeverShortensAConfiguredValue) {
  // A base at or above the ceiling already exceeds every escalated block, so
  // it is handed back unchanged rather than clamped - and it does not escalate
  // either, which is what the setting's description says.
  const int base = static_cast<int>(auth_rate_limiter::kMaxBlockSeconds) * 3;
  EXPECT_EQ(auth_rate_limiter::block_duration_seconds(base, 1), base);
  EXPECT_EQ(auth_rate_limiter::block_duration_seconds(base, 10), base);

  const int at_ceiling = static_cast<int>(auth_rate_limiter::kMaxBlockSeconds);
  EXPECT_EQ(auth_rate_limiter::block_duration_seconds(at_ceiling, 1), at_ceiling);
  EXPECT_EQ(auth_rate_limiter::block_duration_seconds(at_ceiling, 5), at_ceiling);
}

TEST(AuthRateLimiter, OnlyAMachineSpeedRunEscalates) {
  auth_rate_limiter rl;
  // The whole failure budget spent within the burst window: a guessing loop.
  EXPECT_TRUE(rl.is_burst(0));
  EXPECT_TRUE(rl.is_burst((auth_rate_limiter::kDefaultMaxFailures - 1) * auth_rate_limiter::kBurstGapSeconds - 1));
  // One failure every kBurstGapSeconds or slower: a person, or a poller.
  EXPECT_FALSE(rl.is_burst((auth_rate_limiter::kDefaultMaxFailures - 1) * auth_rate_limiter::kBurstGapSeconds));
  EXPECT_FALSE(rl.is_burst(600));
}

TEST(AuthRateLimiter, ASlowTrickleNeverEscalatesPastTheBaseBlock) {
  // The case that matters behind NAT or a reverse proxy, where every client
  // shares one peer address: a monitoring client retrying a stale password
  // every 5 s must not ratchet the shared address up to the ceiling and lock
  // everyone else out of logging in. It stays at the base block - exactly the
  // pre-escalation behaviour.
  auth_rate_limiter rl;
  const std::string ip = "10.0.0.1";
  std::time_t now = 1000;
  for (int round = 0; round < 8; ++round) {
    for (int i = 0; i < auth_rate_limiter::kDefaultMaxFailures; ++i) {
      rl.record_failure_at(ip, now);
      now += 5;
    }
    EXPECT_EQ(rl.blocked_until(ip) - (now - 5), auth_rate_limiter::kDefaultBlockSeconds) << "round " << round;
    // Wait out the block before the next round, as a retrying client does.
    now += auth_rate_limiter::kDefaultBlockSeconds;
  }
}

TEST(AuthRateLimiter, ASlowRunDropsAFastGuesserBackToTheBase) {
  auth_rate_limiter rl;
  const std::string ip = "10.0.0.2";
  std::time_t now = 1000;
  // Two machine-speed rounds: the block doubles.
  for (int round = 0; round < 2; ++round) {
    for (int i = 0; i < auth_rate_limiter::kDefaultMaxFailures; ++i) rl.record_failure_at(ip, now);
    now += auth_rate_limiter::kDefaultBlockSeconds;
  }
  EXPECT_EQ(rl.blocked_until(ip) - (now - auth_rate_limiter::kDefaultBlockSeconds), 2 * auth_rate_limiter::kDefaultBlockSeconds);

  // Then a slow run from the same address - the guesser gave up and what is
  // left is an ordinary client failing occasionally.
  for (int i = 0; i < auth_rate_limiter::kDefaultMaxFailures; ++i) {
    rl.record_failure_at(ip, now);
    now += 30;
  }
  EXPECT_EQ(rl.blocked_until(ip) - (now - 30), auth_rate_limiter::kDefaultBlockSeconds);
}

TEST(AuthRateLimiter, QuietTimeResetsTheEscalation) {
  auth_rate_limiter rl;
  const std::string ip = "10.0.0.3";
  std::time_t now = 1000;
  for (int round = 0; round < 3; ++round) {
    for (int i = 0; i < auth_rate_limiter::kDefaultMaxFailures; ++i) rl.record_failure_at(ip, now);
    now += auth_rate_limiter::kDefaultBlockSeconds;
  }
  EXPECT_GT(rl.blocked_until(ip) - now, auth_rate_limiter::kDefaultBlockSeconds);

  // Quiet for well past the decay window, then a fresh burst: base block again.
  now = rl.blocked_until(ip) + auth_rate_limiter::kOffenseDecaySeconds + 1;
  for (int i = 0; i < auth_rate_limiter::kDefaultMaxFailures; ++i) rl.record_failure_at(ip, now);
  EXPECT_EQ(rl.blocked_until(ip) - now, auth_rate_limiter::kDefaultBlockSeconds);
}

TEST(AuthRateLimiter, ConsecutiveBlocksEscalate) {
  auth_rate_limiter rl;
  const std::string ip = "1.2.3.4";
  const auto trip = [&] {
    for (int i = 0; i < auth_rate_limiter::kDefaultMaxFailures; ++i) rl.record_failure(ip);
  };

  trip();
  const std::time_t now = std::time(nullptr);
  const std::time_t first = rl.blocked_until(ip);
  EXPECT_GE(first - now, auth_rate_limiter::kDefaultBlockSeconds - 1);

  // The block is still running, but a second round of failures from the same
  // IP must buy a longer one, not the same fixed window again.
  trip();
  const std::time_t second = rl.blocked_until(ip);
  EXPECT_GE(second - now, 2 * auth_rate_limiter::kDefaultBlockSeconds - 1);
  EXPECT_GT(second, first);
}

TEST(AuthRateLimiter, SuccessResetsEscalation) {
  auth_rate_limiter rl;
  const std::string ip = "1.2.3.4";
  for (int round = 0; round < 3; ++round) {
    for (int i = 0; i < auth_rate_limiter::kDefaultMaxFailures; ++i) rl.record_failure(ip);
    rl.record_success(ip);
  }
  // A client that authenticates successfully in between is not an offender:
  // the next block starts from the base duration again.
  for (int i = 0; i < auth_rate_limiter::kDefaultMaxFailures; ++i) rl.record_failure(ip);
  const std::time_t now = std::time(nullptr);
  EXPECT_LE(rl.blocked_until(ip) - now, auth_rate_limiter::kDefaultBlockSeconds);
}
