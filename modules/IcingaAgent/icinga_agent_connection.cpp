// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_connection.hpp"

#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <chrono>

namespace icinga_agent {

namespace asio = boost::asio;
namespace json = boost::json;

namespace {
double unix_now() { return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); }

std::string peer_cn(SSL *ssl) {
  X509 *cert = SSL_get1_peer_certificate(ssl);
  if (!cert) return "";
  char buf[256] = {0};
  X509_NAME_get_text_by_NID(X509_get_subject_name(cert), NID_commonName, buf, sizeof(buf));
  X509_free(cert);
  return buf;
}
}  // namespace

connection::connection(asio::io_context &io, asio::ssl::context &ctx, connection_config config, callbacks cb)
    : io_(io), resolver_(io), stream_(io, ctx), heartbeat_timer_(io), idle_timer_(io), config_(std::move(config)), cb_(std::move(cb)) {}

void connection::start() {
  auto self = shared_from_this();
  cb_.log_debug("Connecting to " + config_.host + ":" + config_.port);
  arm_idle_timer();
  resolver_.async_resolve(config_.host, config_.port,
                          [self](const boost::system::error_code &ec, const asio::ip::tcp::resolver::results_type &r) { self->on_resolve(ec, r); });
}

void connection::on_resolve(const boost::system::error_code &ec, const asio::ip::tcp::resolver::results_type &results) {
  if (closed_) return;
  if (ec) return close("Failed to resolve " + config_.host + ": " + ec.message());
  cb_.log_debug("Resolved " + config_.host);
  auto self = shared_from_this();
  asio::async_connect(stream_.lowest_layer(), results, [self](const boost::system::error_code &e, const asio::ip::tcp::endpoint &) { self->on_connect(e); });
}

void connection::on_connect(const boost::system::error_code &ec) {
  if (closed_) return;
  if (ec) return close("Failed to connect to " + config_.host + ":" + config_.port + ": " + ec.message());
  cb_.log_debug("TCP connected, starting TLS handshake");
  // SNI: Icinga does not need it, but proxies in front of a master might.
  SSL_set_tlsext_host_name(stream_.native_handle(), config_.host.c_str());
  auto self = shared_from_this();
  stream_.async_handshake(asio::ssl::stream_base::client, [self](const boost::system::error_code &e) { self->on_handshake(e); });
}

void connection::on_handshake(const boost::system::error_code &ec) {
  if (closed_) return;
  if (ec) return close("TLS handshake with " + config_.host + " failed: " + ec.message());
  const std::string cn = peer_cn(stream_.native_handle());
  if (!config_.parent_endpoint.empty() && cn != config_.parent_endpoint) {
    return close("Parent presented the certificate of '" + cn + "', expected '" + config_.parent_endpoint + "'");
  }
  established_ = true;
  cb_.log_info("Connected to Icinga endpoint '" + cn + "' at " + config_.host + ":" + config_.port);
  // Same opening as Icinga for Windows: Hello, then ask for a (renewed)
  // certificate. The master answers the latter with pki::UpdateCertificate
  // once it has signed one - immediately with a valid ticket, or after
  // `icinga2 ca sign <fingerprint>` without one.
  send(make_hello(config_.agent_version));
  send(make_certificate_request(config_.ticket));
  schedule_heartbeat();
  read();
}

void connection::read() {
  auto self = shared_from_this();
  stream_.async_read_some(asio::buffer(buffer_), [self](const boost::system::error_code &ec, std::size_t n) { self->on_read(ec, n); });
}

void connection::on_read(const boost::system::error_code &ec, const std::size_t bytes) {
  if (closed_) return;
  if (ec) return close(ec == asio::error::eof || ec == asio::ssl::error::stream_truncated ? "Parent closed the connection" : "Read failed: " + ec.message());
  arm_idle_timer();
  std::vector<std::string> messages;
  try {
    messages = reader_.feed(buffer_.data(), bytes);
  } catch (const std::exception &e) {
    return close(std::string("Protocol error: ") + e.what());
  }
  for (const auto &m : messages) {
    try {
      dispatch(m);
    } catch (const std::exception &e) {
      cb_.log_error(std::string("Failed to handle a message: ") + e.what());
    }
    if (closed_) return;
  }
  read();
}

void connection::dispatch(const std::string &payload) {
  const json::value msg = json::parse(payload);
  const json::object &obj = msg.as_object();
  const auto m = obj.find("method");
  if (m == obj.end() || !m->value().is_string()) {
    // A response to one of our requests (we send none with an id) or junk.
    cb_.log_debug("Ignoring message without method: " + payload.substr(0, 200));
    return;
  }
  const std::string method(m->value().as_string());
  const auto p = obj.find("params");
  const json::value params = p == obj.end() ? json::value() : p->value();
  cb_.log_debug("<- " + method + " " + payload.substr(0, 2000));
  if (method == "event::ExecuteCommand") {
    cb_.execute(shared_from_this(), params);
  } else if (method == "event::Heartbeat") {
    // Tell the parent we have nothing to replay; keeps it from buffering a
    // replay log for us.
    send(make_log_position(unix_now()));
  } else if (method == "pki::UpdateCertificate") {
    cb_.update_certificate(params);
    // Reconnect with the new certificate.
    close("Received a signed certificate, reconnecting with it");
  } else if (method == "icinga::Hello") {
    cb_.log_debug("Parent hello: " + json::serialize(params));
  } else {
    // config::Update, log::SetLogPosition, event::SetNextCheck, ... are for
    // agents that keep their own object state; we have none.
  }
}

void connection::send(std::string framed) {
  auto self = shared_from_this();
  asio::post(io_, [self, framed = std::move(framed)]() mutable {
    if (self->closed_) return;
    self->outbox_.push_back(std::move(framed));
    if (!self->writing_) self->write_next();
  });
}

void connection::write_next() {
  if (outbox_.empty() || closed_) {
    writing_ = false;
    return;
  }
  writing_ = true;
  auto self = shared_from_this();
  asio::async_write(stream_, asio::buffer(outbox_.front()), [self](const boost::system::error_code &ec, std::size_t) {
    if (ec) return self->close("Write failed: " + ec.message());
    self->outbox_.pop_front();
    self->write_next();
  });
}

void connection::schedule_heartbeat() {
  auto self = shared_from_this();
  heartbeat_timer_.expires_after(std::chrono::seconds(config_.heartbeat_interval));
  heartbeat_timer_.async_wait([self](const boost::system::error_code &ec) {
    if (ec || self->closed_) return;
    self->send(make_heartbeat(self->config_.idle_timeout));
    self->schedule_heartbeat();
  });
}

void connection::arm_idle_timer() {
  auto self = shared_from_this();
  idle_timer_.expires_after(std::chrono::seconds(config_.idle_timeout));
  idle_timer_.async_wait([self](const boost::system::error_code &ec) {
    if (ec || self->closed_) return;
    self->close(self->established_ ? "No data from the parent for " + std::to_string(self->config_.idle_timeout) + "s" : "Connection attempt timed out");
  });
}

void connection::close(const std::string &reason) {
  if (closed_) return;
  closed_ = true;
  if (established_) {
    cb_.log_info("Disconnected from " + config_.host + ": " + reason);
  } else {
    cb_.log_error(reason);
  }
  boost::system::error_code ignored;
  resolver_.cancel();
  heartbeat_timer_.cancel();
  idle_timer_.cancel();
  stream_.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
  stream_.lowest_layer().close(ignored);
  if (cb_.closed) cb_.closed();
}

}  // namespace icinga_agent
