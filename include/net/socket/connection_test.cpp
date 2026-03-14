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

#include <boost/make_shared.hpp>
#include <net/socket/connection.hpp>
#include <string>
#include <vector>

namespace {

// ============================================================================
// Mock protocol for testing the connection state machine
// ============================================================================
struct MockProtocol {
  static constexpr bool debug_trace = false;
  typedef std::vector<char> outbound_buffer_type;

  bool connect_result = true;
  bool wants_data_result = false;
  bool has_data_result = false;
  bool on_read_result = true;
  outbound_buffer_type outbound = {'t', 'e', 's', 't'};

  struct info_type {
    unsigned int timeout = 30;
  };
  info_type info_;

  mutable std::vector<std::string> debug_logs;
  mutable std::vector<std::string> error_logs;
  int on_write_count = 0;
  int on_read_count = 0;

  void log_debug(std::string, int, std::string msg) const { debug_logs.push_back(msg); }
  void log_error(std::string, int, std::string msg) const { error_logs.push_back(msg); }
  bool on_connect() { return connect_result; }
  const info_type& get_info() const { return info_; }
  bool wants_data() { return wants_data_result; }
  bool has_data() { return has_data_result; }
  outbound_buffer_type get_outbound() {
    has_data_result = false;  // prevent infinite loop in do_process
    return outbound;
  }
  template <class Iterator>
  bool on_read(Iterator, Iterator) {
    on_read_count++;
    wants_data_result = false;  // prevent infinite loop in do_process
    return on_read_result;
  }
  void on_write() { on_write_count++; }
};

// ============================================================================
// Concrete test connection overriding pure virtuals
// ============================================================================
constexpr std::size_t BUF_SIZE = 1024;

class TestConnection : public socket_helpers::server::connection<MockProtocol, BUF_SIZE> {
  typedef socket_helpers::server::connection<MockProtocol, BUF_SIZE> parent_type;
  boost::asio::ip::tcp::socket socket_;
  bool open_;

 public:
  bool read_requested = false;
  bool write_requested = false;
  bool on_done_called = false;
  bool on_done_result = false;

  TestConnection(boost::asio::io_service& io, boost::shared_ptr<MockProtocol> protocol, bool open = false)
      : parent_type(io, protocol), socket_(io), open_(open) {}

  boost::asio::ip::tcp::socket& get_socket() override { return socket_; }
  bool is_open() override { return open_; }
  void set_open(bool v) { open_ = v; }

  void start_read_request() override { read_requested = true; }
  void start_write_request(const boost::asio::const_buffer&) override { write_requested = true; }

  // Override to avoid shared_from_this() / async operations in tests
  void set_timeout(int) override {}

  // Override to track calls without socket side-effects
  void on_done(bool all_ok) override {
    on_done_called = true;
    on_done_result = all_ok;
    is_active_ = false;
  }

  // Expose protected members for testing
  using parent_type::buffer_;
  using parent_type::do_process;
  using parent_type::handle_read_request;
  using parent_type::handle_write_response;
  using parent_type::is_active_;
  using parent_type::protocol_;
};

// ============================================================================
// Test fixture
// ============================================================================
class ServerConnectionTest : public ::testing::Test {
 protected:
  boost::asio::io_service io_;
  boost::shared_ptr<MockProtocol> protocol_;

  void SetUp() override { protocol_ = boost::make_shared<MockProtocol>(); }

  boost::shared_ptr<TestConnection> make_connection(bool open = false) { return boost::shared_ptr<TestConnection>(new TestConnection(io_, protocol_, open)); }
};

}  // namespace

// =============================================================================
// do_process state machine
// =============================================================================

TEST_F(ServerConnectionTest, DoProcess_WantsData_RequestsRead) {
  auto conn = make_connection();
  protocol_->wants_data_result = true;
  conn->do_process();
  EXPECT_TRUE(conn->read_requested);
  EXPECT_FALSE(conn->write_requested);
  EXPECT_FALSE(conn->on_done_called);
}

TEST_F(ServerConnectionTest, DoProcess_HasDataAndOpen_RequestsWrite) {
  auto conn = make_connection(/*open=*/true);
  protocol_->has_data_result = true;
  conn->do_process();
  EXPECT_TRUE(conn->write_requested);
  EXPECT_FALSE(conn->read_requested);
  EXPECT_FALSE(conn->on_done_called);
}

TEST_F(ServerConnectionTest, DoProcess_HasDataButNotOpen_ErrorAndDone) {
  auto conn = make_connection(/*open=*/false);
  protocol_->has_data_result = true;
  conn->do_process();
  EXPECT_FALSE(conn->write_requested);
  EXPECT_TRUE(conn->on_done_called);
  EXPECT_FALSE(conn->on_done_result);
  EXPECT_FALSE(protocol_->error_logs.empty());
}

TEST_F(ServerConnectionTest, DoProcess_NeitherWantsNorHas_DoneTrue) {
  auto conn = make_connection();
  conn->do_process();
  EXPECT_TRUE(conn->on_done_called);
  EXPECT_TRUE(conn->on_done_result);
}

TEST_F(ServerConnectionTest, DoProcess_WantsData_NotActive_NoRead) {
  auto conn = make_connection();
  conn->is_active_ = false;
  protocol_->wants_data_result = true;
  conn->do_process();
  EXPECT_FALSE(conn->read_requested);
  EXPECT_FALSE(conn->on_done_called);
}

TEST_F(ServerConnectionTest, DoProcess_NeitherWantsNorHas_NotActive_NoDone) {
  auto conn = make_connection();
  conn->is_active_ = false;
  conn->do_process();
  EXPECT_FALSE(conn->on_done_called);
}

TEST_F(ServerConnectionTest, DoProcess_HasData_Open_NotActive_NoWrite) {
  auto conn = make_connection(/*open=*/true);
  conn->is_active_ = false;
  protocol_->has_data_result = true;
  conn->do_process();
  EXPECT_FALSE(conn->write_requested);
  EXPECT_FALSE(conn->on_done_called);
}

// =============================================================================
// handle_read_request
// =============================================================================

TEST_F(ServerConnectionTest, HandleReadRequest_NoError_OnReadTrue_ProcessesFurther) {
  auto conn = make_connection();
  protocol_->on_read_result = true;
  boost::system::error_code no_error;
  conn->handle_read_request(no_error, 10);
  EXPECT_EQ(protocol_->on_read_count, 1);
  // do_process called -> wants_data=false (set by on_read), has_data=false -> on_done(true)
  EXPECT_TRUE(conn->on_done_called);
  EXPECT_TRUE(conn->on_done_result);
}

TEST_F(ServerConnectionTest, HandleReadRequest_NoError_OnReadFalse_DoneFalse) {
  auto conn = make_connection();
  protocol_->on_read_result = false;
  boost::system::error_code no_error;
  conn->handle_read_request(no_error, 5);
  EXPECT_EQ(protocol_->on_read_count, 1);
  EXPECT_TRUE(conn->on_done_called);
  EXPECT_FALSE(conn->on_done_result);
}

TEST_F(ServerConnectionTest, HandleReadRequest_WithError_DoneFalse) {
  auto conn = make_connection();
  boost::system::error_code err = boost::asio::error::eof;
  conn->handle_read_request(err, 0);
  EXPECT_EQ(protocol_->on_read_count, 0);
  EXPECT_TRUE(conn->on_done_called);
  EXPECT_FALSE(conn->on_done_result);
}

TEST_F(ServerConnectionTest, HandleReadRequest_WithError_LogsDebug) {
  auto conn = make_connection();
  boost::system::error_code err = boost::asio::error::eof;
  conn->handle_read_request(err, 0);
  EXPECT_FALSE(protocol_->debug_logs.empty());
}

// =============================================================================
// handle_write_response
// =============================================================================

TEST_F(ServerConnectionTest, HandleWriteResponse_NoError_CallsOnWriteAndProcesses) {
  auto conn = make_connection();
  boost::system::error_code no_error;
  conn->handle_write_response(no_error, 10);
  EXPECT_EQ(protocol_->on_write_count, 1);
  // do_process called -> wants_data=false, has_data=false -> on_done(true)
  EXPECT_TRUE(conn->on_done_called);
  EXPECT_TRUE(conn->on_done_result);
}

TEST_F(ServerConnectionTest, HandleWriteResponse_WithError_DoneFalse) {
  auto conn = make_connection();
  boost::system::error_code err = boost::asio::error::eof;
  conn->handle_write_response(err, 0);
  EXPECT_EQ(protocol_->on_write_count, 0);
  EXPECT_TRUE(conn->on_done_called);
  EXPECT_FALSE(conn->on_done_result);
}

TEST_F(ServerConnectionTest, HandleWriteResponse_WithError_LogsError) {
  auto conn = make_connection();
  boost::system::error_code err = boost::asio::error::eof;
  conn->handle_write_response(err, 0);
  EXPECT_FALSE(protocol_->error_logs.empty());
}

// =============================================================================
// start
// =============================================================================

TEST_F(ServerConnectionTest, Start_OnConnectTrue_CallsDoProcess) {
  auto conn = make_connection();
  protocol_->connect_result = true;
  conn->start();
  // do_process -> wants_data=false, has_data=false -> on_done(true)
  EXPECT_TRUE(conn->on_done_called);
  EXPECT_TRUE(conn->on_done_result);
}

TEST_F(ServerConnectionTest, Start_OnConnectFalse_DoneFalse) {
  auto conn = make_connection();
  protocol_->connect_result = false;
  conn->start();
  EXPECT_TRUE(conn->on_done_called);
  EXPECT_FALSE(conn->on_done_result);
}

// =============================================================================
// timeout
// =============================================================================

TEST_F(ServerConnectionTest, Timeout_NonAborted_CallsDoneFalse) {
  auto conn = make_connection();
  boost::system::error_code success;
  conn->timeout(success);
  EXPECT_TRUE(conn->on_done_called);
  EXPECT_FALSE(conn->on_done_result);
}

TEST_F(ServerConnectionTest, Timeout_Aborted_DoesNotCallDone) {
  auto conn = make_connection();
  boost::system::error_code aborted = boost::asio::error::operation_aborted;
  conn->timeout(aborted);
  EXPECT_FALSE(conn->on_done_called);
}

// =============================================================================
// on_done
// =============================================================================

TEST_F(ServerConnectionTest, OnDone_SetsInactive) {
  auto conn = make_connection();
  EXPECT_TRUE(conn->is_active_);
  conn->on_done(true);
  EXPECT_FALSE(conn->is_active_);
}

TEST_F(ServerConnectionTest, OnDone_TracksResult) {
  auto conn = make_connection();
  conn->on_done(true);
  EXPECT_TRUE(conn->on_done_result);

  // Reset and test with false
  auto conn2 = make_connection();
  conn2->on_done(false);
  EXPECT_FALSE(conn2->on_done_result);
}
