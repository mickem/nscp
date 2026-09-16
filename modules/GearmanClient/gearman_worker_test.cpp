// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/**
 * The worker loop, driven end to end against a fake gearmand.
 *
 * The fake is a real socket on the loopback interface speaking the real
 * binary protocol, so the connection layer is exercised too - the bugs this
 * tier is meant to catch (a packet read as the answer to the wrong request, a
 * worker that never reconnects, a shutdown that hangs) do not reproduce
 * against a mocked transport. What is stubbed is only what is on the other
 * side of the worker's two interfaces: the check itself and the log.
 *
 * Everything here has a deadline. A test that hangs on a worker that is not
 * going to answer tells you much less than one that fails in a second.
 */
#include "gearman_worker.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gearman_crypt.hpp"
#include "gearman_job.hpp"
#include "gearman_protocol.hpp"

using namespace gearman;

namespace {

const char *const test_key = "nscp-test-key";
const char *const test_queue = "hostgroup_test";
const char *const local_name = "win-srv01";

bool wait_until(const std::function<bool()> &predicate, const int timeout_ms = 10000) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return predicate();
}

// ---------------------------------------------------------------------------
// The fake job server
// ---------------------------------------------------------------------------

/**
 * Just enough gearmand for a worker: it accepts connections, answers
 * GRAB_JOB from a queue the test fills, acknowledges submitted results and
 * records what it was told. One thread per connection, each blocking on its
 * own io_context with a short deadline so a shutdown is prompt and nobody
 * touches a socket another thread owns.
 */
class fake_gearmand {
 public:
  struct submission {
    std::string queue;
    std::string unique;
    std::string payload;
  };

  fake_gearmand() : acceptor_(io_, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0)) {
    port_ = acceptor_.local_endpoint().port();
    accept_thread_ = std::thread([this] { accept_loop(); });
  }

  ~fake_gearmand() { shutdown(); }

  void shutdown() {
    if (stop_.exchange(true)) return;
    if (accept_thread_.joinable()) accept_thread_.join();
    for (std::thread &session : sessions_) {
      if (session.joinable()) session.join();
    }
    sessions_.clear();
  }

  unsigned short port() const { return port_; }

  /** Hand this payload to the next worker that asks. */
  void queue_job(const std::string &function, const std::string &payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_.push_back({function, payload});
  }

  std::vector<std::string> abilities() {
    std::lock_guard<std::mutex> lock(mutex_);
    return abilities_;
  }
  std::vector<std::string> client_ids() {
    std::lock_guard<std::mutex> lock(mutex_);
    return client_ids_;
  }
  std::vector<submission> submissions() {
    std::lock_guard<std::mutex> lock(mutex_);
    return submissions_;
  }
  std::vector<std::string> completed() {
    std::lock_guard<std::mutex> lock(mutex_);
    return completed_;
  }
  std::vector<std::string> failed() {
    std::lock_guard<std::mutex> lock(mutex_);
    return failed_;
  }
  int connections() const { return connections_.load(); }
  int live_connections() const { return live_.load(); }

  /** Close whatever the workers are connected on, so they have to reconnect. */
  void drop_connections() { drop_generation_.fetch_add(1); }

 private:
  struct queued_job {
    std::string function;
    std::string payload;
  };

  void accept_loop() {
    while (!stop_.load()) {
      // The socket is created on the io_context its own session thread will
      // drive: an Asio socket belongs to one thread at a time, and accepting
      // into a foreign context is explicitly allowed.
      auto session_io = std::make_shared<boost::asio::io_context>();
      auto socket = std::make_shared<boost::asio::ip::tcp::socket>(*session_io);
      bool done = false;
      boost::system::error_code accept_error;
      acceptor_.async_accept(*socket, [&done, &accept_error](const boost::system::error_code &e) {
        accept_error = e;
        done = true;
      });
      while (!done && !stop_.load()) {
        io_.restart();
        io_.run_one_for(std::chrono::milliseconds(20));
      }
      if (!done) {
        boost::system::error_code ignored;
        acceptor_.cancel(ignored);
        io_.restart();
        io_.run();
        break;
      }
      if (accept_error) continue;
      connections_.fetch_add(1);
      live_.fetch_add(1);
      sessions_.emplace_back([this, session_io, socket] { session(session_io, socket); });
    }
  }

  void session(const std::shared_ptr<boost::asio::io_context> &io_holder, const std::shared_ptr<boost::asio::ip::tcp::socket> &socket_holder) {
    boost::asio::io_context &io = *io_holder;
    boost::asio::ip::tcp::socket &socket = *socket_holder;
    const int generation = drop_generation_.load();

    std::string buffer;
    bool sleeping = false;
    std::vector<char> chunk(4096);
    while (!stop_.load() && drop_generation_.load() == generation) {
      packet request;
      std::size_t consumed = 0;
      std::string error;
      if (decode_packet(buffer, request, consumed, error) == decode_result::ok) {
        buffer.erase(0, consumed);
        if (!handle(socket, request, sleeping)) break;
        continue;
      }

      // A worker that went to sleep is woken as soon as the test queues work.
      if (sleeping && has_job()) {
        sleeping = false;
        if (!write(socket, packet_type::noop, {})) break;
      }

      bool done = false;
      boost::system::error_code read_error;
      std::size_t read_bytes = 0;
      socket.async_read_some(boost::asio::buffer(chunk), [&](const boost::system::error_code &e, const std::size_t bytes) {
        read_error = e;
        read_bytes = bytes;
        done = true;
      });
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
      while (!done && std::chrono::steady_clock::now() < deadline) {
        io.restart();
        io.run_one_for(std::chrono::milliseconds(5));
      }
      if (!done) {
        boost::system::error_code ignored;
        socket.cancel(ignored);
        io.restart();
        io.run();
        continue;
      }
      if (read_error || read_bytes == 0) break;
      buffer.append(chunk.data(), read_bytes);
    }

    boost::system::error_code ignored;
    socket.close(ignored);
    live_.fetch_sub(1);
  }

  bool has_job() {
    std::lock_guard<std::mutex> lock(mutex_);
    return !jobs_.empty();
  }

  bool write(boost::asio::ip::tcp::socket &socket, const packet_type type, const std::vector<std::string> &args) {
    const std::string data = encode_packet(type, args, packet_magic::response);
    boost::system::error_code write_error;
    boost::asio::write(socket, boost::asio::buffer(data), write_error);
    return !write_error;
  }

  bool handle(boost::asio::ip::tcp::socket &socket, const packet &request, bool &sleeping) {
    switch (request.type) {
      case packet_type::set_client_id: {
        std::lock_guard<std::mutex> lock(mutex_);
        client_ids_.push_back(request.arg(0));
        return true;
      }
      case packet_type::reset_abilities: {
        std::lock_guard<std::mutex> lock(mutex_);
        abilities_.clear();
        return true;
      }
      case packet_type::can_do: {
        std::lock_guard<std::mutex> lock(mutex_);
        abilities_.push_back(request.arg(0));
        return true;
      }
      case packet_type::pre_sleep:
        sleeping = true;
        return true;
      case packet_type::grab_job: {
        sleeping = false;
        queued_job job;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          if (jobs_.empty()) return write(socket, packet_type::no_job, {});
          job = jobs_.front();
          jobs_.erase(jobs_.begin());
        }
        return write(socket, packet_type::job_assign, {next_handle(), job.function, job.payload});
      }
      case packet_type::submit_job_bg: {
        {
          std::lock_guard<std::mutex> lock(mutex_);
          submissions_.push_back({request.arg(0), request.arg(1), request.arg(2)});
        }
        return write(socket, packet_type::job_created, {next_handle()});
      }
      case packet_type::work_complete: {
        std::lock_guard<std::mutex> lock(mutex_);
        completed_.push_back(request.arg(0));
        return true;
      }
      case packet_type::work_fail: {
        std::lock_guard<std::mutex> lock(mutex_);
        failed_.push_back(request.arg(0));
        return true;
      }
      default:
        return true;
    }
  }

  std::string next_handle() { return "H:fake:" + std::to_string(handle_counter_.fetch_add(1)); }

  boost::asio::io_context io_;
  boost::asio::ip::tcp::acceptor acceptor_;
  unsigned short port_ = 0;
  std::thread accept_thread_;
  std::vector<std::thread> sessions_;

  std::atomic<bool> stop_{false};
  std::atomic<int> connections_{0};
  std::atomic<int> live_{0};
  std::atomic<int> drop_generation_{0};
  std::atomic<int> handle_counter_{1};

  std::mutex mutex_;
  std::vector<queued_job> jobs_;
  std::vector<std::string> abilities_;
  std::vector<std::string> client_ids_;
  std::vector<submission> submissions_;
  std::vector<std::string> completed_;
  std::vector<std::string> failed_;
};

// ---------------------------------------------------------------------------
// Stubs for the worker's two interfaces
// ---------------------------------------------------------------------------

class stub_executor : public query_executor {
 public:
  struct call {
    std::string command;
    std::list<std::string> arguments;
    unsigned int timeout = 0;
  };

  query_result execute(const std::string &command, const std::list<std::string> &arguments, const unsigned int timeout_seconds) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      calls_.push_back({command, arguments, timeout_seconds});
    }
    if (delay_ms_ > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
    if (throw_) throw std::runtime_error("the check exploded");
    return result_;
  }

  std::vector<call> calls() {
    std::lock_guard<std::mutex> lock(mutex_);
    return calls_;
  }

  query_result result_;
  int delay_ms_ = 0;
  bool throw_ = false;

 private:
  std::mutex mutex_;
  std::vector<call> calls_;
};

class stub_logger : public worker_logger {
 public:
  void error(const std::string &message) override { record("error", message); }
  void warning(const std::string &message) override { record("warning", message); }
  void info(const std::string &message) override { record("info", message); }
  void debug(const std::string &message) override { record("debug", message); }

  bool logged(const std::string &level, const std::string &needle) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &line : lines_) {
      if (line.first == level && line.second.find(needle) != std::string::npos) return true;
    }
    return false;
  }

 private:
  void record(const std::string &level, const std::string &message) {
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.emplace_back(level, message);
  }
  std::mutex mutex_;
  std::vector<std::pair<std::string, std::string>> lines_;
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

std::string make_job(const std::string &host, const std::string &service, const std::string &command_line, const long long timeout = 10,
                     const std::string &core_time = std::string(), const std::string &key = test_key) {
  check_job job;
  job.type = service.empty() ? "host" : "service";
  job.host_name = host;
  job.service_description = service;
  job.command_line = command_line;
  job.result_queue = default_result_queue;
  job.target_queue = test_queue;
  job.core_time = core_time.empty() ? now_timestamp() : core_time;
  job.timeout = timeout;
  return encode_payload(format_job(job), envelope(true, key));
}

class WorkerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    executor_ = std::make_shared<stub_executor>();
    logger_ = std::make_shared<stub_logger>();
    executor_->result_.return_code = 0;
    executor_->result_.output = "OK: all good|'load'=5%;80;90";

    config_.servers.emplace_back("127.0.0.1", std::to_string(server_.port()));
    config_.crypto = envelope(true, test_key);
    config_.queues.emplace_back(test_queue);
    config_.host_names.insert(local_name);
    config_.workers = 1;
    config_.client_id = "nscp-test";
    config_.source = "NSClient++ 0.19.0 on win-srv01";
    // Short enough that a test never sits on a deadline, long enough that a
    // loaded machine does not trip one by accident.
    config_.connect_timeout = 5;
    config_.idle_timeout = 2;
  }

  void TearDown() override {
    pool_.stop(5);
    server_.shutdown();
  }

  void start() { pool_.start(config_, executor_, logger_); }

  /** The first result the worker submitted, decoded. */
  check_result first_result() {
    const auto submissions = server_.submissions();
    EXPECT_FALSE(submissions.empty());
    return parse_result(decode_payload(submissions.front().payload, envelope(true, test_key)));
  }

  bool wait_for_submission(const std::size_t count = 1) {
    return wait_until([this, count] { return server_.submissions().size() >= count; });
  }

  fake_gearmand server_;
  worker_config config_;
  std::shared_ptr<stub_executor> executor_;
  std::shared_ptr<stub_logger> logger_;
  worker_pool pool_;
};

// ---------------------------------------------------------------------------
// Server list
// ---------------------------------------------------------------------------

TEST(ServerList, defaults_the_port_and_drops_blank_entries) {
  const auto servers = parse_server_list(" gearmand.example.com , , other:4731 ,");
  ASSERT_EQ(servers.size(), 2u);
  EXPECT_EQ(servers[0].host, "gearmand.example.com");
  EXPECT_EQ(servers[0].port, "4730");
  EXPECT_EQ(servers[1].host, "other");
  EXPECT_EQ(servers[1].port, "4731");
}

TEST(ServerList, reads_a_bracketed_ipv6_literal_with_and_without_a_port) {
  const auto servers = parse_server_list("[::1]:4730,[fe80::1]");
  ASSERT_EQ(servers.size(), 2u);
  EXPECT_EQ(servers[0].host, "::1");
  EXPECT_EQ(servers[0].port, "4730");
  EXPECT_EQ(servers[1].host, "fe80::1");
  EXPECT_EQ(servers[1].port, "4730");
}

TEST(ServerList, leaves_a_bare_ipv6_literal_alone) {
  // Without brackets every colon is part of the address, so there is no port
  // to split off - splitting on the last one would resolve a different host.
  const auto servers = parse_server_list("::1");
  ASSERT_EQ(servers.size(), 1u);
  EXPECT_EQ(servers[0].host, "::1");
  EXPECT_EQ(servers[0].port, "4730");
}

TEST(ServerList, an_empty_list_yields_nothing) { EXPECT_TRUE(parse_server_list("  ").empty()); }

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, announces_its_queues_and_names_itself) {
  config_.queues.emplace_back("servicegroup_db");
  start();
  ASSERT_TRUE(wait_until([this] { return server_.abilities().size() >= 2; }));
  EXPECT_EQ(server_.abilities(), (std::vector<std::string>{test_queue, "servicegroup_db"}));
  // The client id is what an operator sees in gearman_top, so it has to name
  // the worker and not just the host.
  ASSERT_FALSE(server_.client_ids().empty());
  EXPECT_EQ(server_.client_ids().front(), "nscp-test-1");
}

TEST_F(WorkerTest, every_worker_gets_its_own_connection) {
  config_.workers = 3;
  start();
  ASSERT_TRUE(wait_until([this] { return server_.connections() >= 3; }));
  EXPECT_EQ(server_.client_ids().size(), 3u);
}

// ---------------------------------------------------------------------------
// Running a job
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, runs_a_service_check_and_submits_the_result) {
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu \"warn=load gt 80\" time=5m"));
  ASSERT_TRUE(wait_for_submission());

  const auto calls = executor_->calls();
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].command, "check_cpu");
  // The quoting rules are the ones aliases and script definitions use, so a
  // threshold with a space in it arrives as one argument and not as three.
  EXPECT_EQ(calls[0].arguments, (std::list<std::string>{"warn=load gt 80", "time=5m"}));
  EXPECT_EQ(calls[0].timeout, 10u);

  EXPECT_EQ(server_.submissions().front().queue, default_result_queue);
  // The unique id is what keeps two results for the same service from being
  // queued twice over; mod_gearman builds it the same way.
  EXPECT_EQ(server_.submissions().front().unique, "win-srv01-CPU load");

  const check_result result = first_result();
  EXPECT_EQ(result.type, "active");
  EXPECT_EQ(result.host_name, local_name);
  EXPECT_EQ(result.service_description, "CPU load");
  EXPECT_EQ(result.return_code, 0);
  EXPECT_EQ(result.output, "OK: all good|'load'=5%;80;90");
  EXPECT_EQ(result.source, "NSClient++ 0.19.0 on win-srv01");
  EXPECT_EQ(result.exited_ok, 1);
  EXPECT_FALSE(result.start_time.empty());
  EXPECT_FALSE(result.finish_time.empty());
  // Quoted back so the core can work out its own latency.
  EXPECT_FALSE(result.core_start_time.empty());

  // And the job is closed, so gearmand does not hand it to anybody else.
  EXPECT_TRUE(wait_until([this] { return !server_.completed().empty(); }));
}

TEST_F(WorkerTest, a_host_check_carries_no_service_description) {
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "", "check_always_ok"));
  ASSERT_TRUE(wait_for_submission());

  const check_result result = first_result();
  EXPECT_EQ(result.host_name, local_name);
  EXPECT_TRUE(result.service_description.empty());
  EXPECT_EQ(server_.submissions().front().unique, local_name);
}

TEST_F(WorkerTest, newlines_in_the_output_survive_the_round_trip) {
  executor_->result_.output = "line one\nline two|'x'=1";
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "multi", "check_ok"));
  ASSERT_TRUE(wait_for_submission());
  // On the wire it is the two characters \n, which parse_result puts back.
  EXPECT_EQ(first_result().output, "line one\nline two|'x'=1");
}

TEST_F(WorkerTest, keeps_grabbing_after_a_job) {
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "first", "check_ok"));
  ASSERT_TRUE(wait_for_submission(1));
  // The second one is queued while the worker is asleep, so this also covers
  // PRE_SLEEP and the NOOP that wakes it.
  server_.queue_job(test_queue, make_job(local_name, "second", "check_ok"));
  ASSERT_TRUE(wait_for_submission(2));
  EXPECT_EQ(executor_->calls().size(), 2u);
}

// ---------------------------------------------------------------------------
// Filtering
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, refuses_a_job_for_another_host_but_still_answers_it) {
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job("some-other-host", "CPU load", "check_cpu"));
  ASSERT_TRUE(wait_for_submission());

  EXPECT_TRUE(executor_->calls().empty());
  const check_result result = first_result();
  EXPECT_EQ(result.return_code, 3);
  EXPECT_EQ(result.host_name, "some-other-host");
  EXPECT_NE(result.output.find("does not answer for some-other-host"), std::string::npos);
  // Answering rather than staying silent is the point: the core would
  // otherwise wait out its orphan timeout on every check.
  EXPECT_TRUE(wait_until([this] { return !server_.completed().empty(); }));
}

TEST_F(WorkerTest, matches_the_host_name_case_insensitively) {
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job("WIN-SRV01", "CPU load", "check_cpu"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_EQ(executor_->calls().size(), 1u);
  EXPECT_EQ(first_result().return_code, 0);
}

TEST_F(WorkerTest, extra_host_names_are_answered_for) {
  config_.host_names.insert("db-cluster-vip");
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job("db-cluster-vip", "CPU load", "check_cpu"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_EQ(executor_->calls().size(), 1u);
}

TEST_F(WorkerTest, without_host_binding_any_host_is_executed) {
  // Proxy mode: `mode = proxy` clears the binding, and the command line the
  // core defines names its own target, so the check runs for a host this
  // agent is not.
  config_.bind_to_host = false;
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job("some-other-host", "CPU load", "check_nrpe -H 10.0.0.5"));
  ASSERT_TRUE(wait_for_submission());
  ASSERT_EQ(executor_->calls().size(), 1u);
  EXPECT_EQ(executor_->calls()[0].command, "check_nrpe");
  EXPECT_EQ(first_result().host_name, "some-other-host");
}

TEST_F(WorkerTest, discards_a_job_older_than_max_age) {
  config_.max_age = 30;
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  const std::string stale = format_timestamp(static_cast<std::time_t>(std::time(nullptr)) - 300, 0);
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu", 10, stale));
  ASSERT_TRUE(wait_for_submission());

  EXPECT_TRUE(executor_->calls().empty());
  const check_result result = first_result();
  EXPECT_EQ(result.return_code, 3);
  EXPECT_NE(result.output.find("max age"), std::string::npos);
}

TEST_F(WorkerTest, max_age_leaves_a_fresh_job_alone) {
  config_.max_age = 30;
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_EQ(executor_->calls().size(), 1u);
}

TEST_F(WorkerTest, an_unreadable_core_time_is_not_aged_out) {
  // Refusing a check because one field could not be parsed would be a worse
  // failure than running it a little late.
  config_.max_age = 30;
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu", 10, "not-a-timestamp"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_EQ(executor_->calls().size(), 1u);
}

TEST_F(WorkerTest, refuses_arguments_when_they_are_not_allowed) {
  config_.allow_arguments = false;
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu time=5m"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_TRUE(executor_->calls().empty());
  EXPECT_EQ(first_result().return_code, 3);
  EXPECT_NE(first_result().output.find("allow arguments"), std::string::npos);
}

TEST_F(WorkerTest, a_bare_command_is_still_run_when_arguments_are_not_allowed) {
  config_.allow_arguments = false;
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_EQ(executor_->calls().size(), 1u);
}

TEST_F(WorkerTest, refuses_metacharacters_by_default) {
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu arg=a&b"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_TRUE(executor_->calls().empty());
  EXPECT_NE(first_result().output.find("metacharacters"), std::string::npos);
}

TEST_F(WorkerTest, a_threshold_written_with_a_greater_than_needs_the_option) {
  // Worth its own case because it is the one every operator meets: a Nagios
  // check_command reads `warn=load>80` and `>` is in the metachar set the
  // guard rejects. The filter language spells the same threshold `gt`, which
  // is the way out that does not widen what a job may contain.
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu warn=load>80"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_TRUE(executor_->calls().empty());
  EXPECT_NE(first_result().output.find("allow nasty characters"), std::string::npos);
}

TEST_F(WorkerTest, allows_metacharacters_when_told_to) {
  config_.allow_nasty_characters = true;
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu arg=a&b"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_EQ(executor_->calls().size(), 1u);
}

// ---------------------------------------------------------------------------
// Timeouts and failures
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, a_timed_out_check_reports_the_configured_status) {
  config_.timeout_return = 2;
  executor_->result_.timed_out = true;
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "slow", "check_timeout"));
  ASSERT_TRUE(wait_for_submission());

  const check_result result = first_result();
  EXPECT_EQ(result.return_code, 2);
  EXPECT_EQ(result.output, "(Check Timed Out)");
  // Not 0: exited_ok=0 makes the core throw the output away and report its
  // own "did not exit properly" instead of the reason we just worked out.
  EXPECT_EQ(result.exited_ok, 1);
}

TEST_F(WorkerTest, the_timeout_status_is_configurable) {
  config_.timeout_return = 3;
  executor_->result_.timed_out = true;
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "slow", "check_timeout"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_EQ(first_result().return_code, 3);
}

TEST_F(WorkerTest, a_job_without_a_timeout_falls_back_to_the_configured_bound) {
  config_.default_job_timeout = 42;
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu", 0));
  ASSERT_TRUE(wait_for_submission());
  ASSERT_EQ(executor_->calls().size(), 1u);
  EXPECT_EQ(executor_->calls()[0].timeout, 42u);
}

TEST_F(WorkerTest, a_check_that_throws_is_reported_rather_than_dropped) {
  executor_->throw_ = true;
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_EQ(first_result().return_code, 3);
  EXPECT_NE(first_result().output.find("the check exploded"), std::string::npos);
}

// ---------------------------------------------------------------------------
// A payload we cannot read
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, a_job_encrypted_with_another_key_is_failed_not_run) {
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu", 10, std::string(), "a-completely-different-key"));

  ASSERT_TRUE(wait_until([this] { return !server_.failed().empty(); }));
  EXPECT_TRUE(executor_->calls().empty());
  EXPECT_TRUE(server_.submissions().empty());
  EXPECT_GE(pool_.counters().errors.load(), 1);
  // The payload itself must never reach the log.
  EXPECT_TRUE(logger_->logged("error", "wrong key?"));
}

TEST_F(WorkerTest, a_payload_that_is_not_a_job_is_failed) {
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.queue_job(test_queue, encode_payload("type=service\nhost_name=\n\n", envelope(true, test_key)));
  ASSERT_TRUE(wait_until([this] { return !server_.failed().empty(); }));
  EXPECT_TRUE(executor_->calls().empty());
}

// ---------------------------------------------------------------------------
// Connection lifecycle
// ---------------------------------------------------------------------------

TEST_F(WorkerTest, reconnects_and_re_registers_after_the_server_drops_it) {
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  server_.drop_connections();
  ASSERT_TRUE(wait_until([this] { return server_.connections() >= 2; }, 20000));
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));

  // And it is a working connection, not just an open one.
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_EQ(first_result().return_code, 0);
}

TEST_F(WorkerTest, counts_connected_workers) {
  config_.workers = 2;
  start();
  ASSERT_TRUE(wait_until([this] { return pool_.counters().connected.load() == 2; }));
  EXPECT_TRUE(pool_.stop(5));
  EXPECT_EQ(pool_.counters().connected.load(), 0);
}

TEST_F(WorkerTest, counts_the_jobs_it_has_taken) {
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  EXPECT_EQ(pool_.counters().jobs.load(), 0);
  server_.queue_job(test_queue, make_job(local_name, "CPU load", "check_cpu"));
  ASSERT_TRUE(wait_for_submission());
  EXPECT_EQ(pool_.counters().jobs.load(), 1);
  EXPECT_GT(pool_.counters().last_job_time.load(), 0);
}

TEST_F(WorkerTest, a_restart_does_not_leave_the_old_workers_registered) {
  // What a settings reload does: loadModuleEx runs again on the live module.
  // Without the stop the queue ends up with twice the workers, each reload.
  config_.workers = 2;
  start();
  ASSERT_TRUE(wait_until([this] { return server_.live_connections() == 2; }));
  start();
  ASSERT_TRUE(wait_until([this] { return server_.live_connections() == 2; }));
  EXPECT_EQ(pool_.counters().connected.load(), 2);
  EXPECT_EQ(server_.connections(), 4);
}

TEST_F(WorkerTest, stopping_while_idle_returns_promptly) {
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }));
  // The worker spends its life blocked on a read; a shutdown must not wait
  // for that read's deadline, let alone for a job that never comes.
  const auto started = std::chrono::steady_clock::now();
  EXPECT_TRUE(pool_.stop(5));
  EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(3));
  EXPECT_FALSE(pool_.is_running());
}

TEST_F(WorkerTest, a_server_that_is_not_there_is_retried_not_fatal) {
  // Point at a port nothing is listening on, then let the second entry in the
  // list be the real one: the loop rotates through the servers.
  config_.servers.insert(config_.servers.begin(), server_address("127.0.0.1", "1"));
  start();
  ASSERT_TRUE(wait_until([this] { return !server_.abilities().empty(); }, 20000));
  EXPECT_GE(pool_.counters().errors.load(), 1);
}

}  // namespace
