// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <net/nrpe/packet.hpp>
#include <net/nrpe/server/handler.hpp>
#include <net/nrpe/server/protocol.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_targets.hpp>

class NRPEServer : public nscapi::impl::simple_plugin, nrpe::server::handler {
 private:
  unsigned int payload_length_;
  bool noPerfData_;
  bool allowNasty_;
  bool allowArgs_;
  bool multiple_packets_;
  std::string encoding_;
  // "none" -> empty principal (legacy behaviour).
  // "cn"   -> Common Name value of the verified client cert (e.g.
  //           "icinga-master"). Requires SSL with verify_mode containing
  //           `peer` and `fail-if-no-peer-cert` and a `ca path` pointing
  //           at the trusted issuer; otherwise the module refuses to
  //           start because an unverified CN would be attacker-supplied.
  //           CN-only (not full RFC 2253 DN) because INI key syntax uses
  //           `=` as the key/value separator and would corrupt any
  //           DN-shaped policy key like `NRPEServer:CN=foo`. If/when an
  //           identity-map indirection lands, a `dn` mode can be added
  //           here to use the alias rather than the raw DN.
  std::string client_identity_source_;

  void set_perf_data(bool v) {
    noPerfData_ = !v;
    if (noPerfData_) log_debug("nrpe", __FILE__, __LINE__, "Performance data disabled!");
  }

 public:
  NRPEServer();
  virtual ~NRPEServer();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  void prepareShutdown();
  bool unloadModule();

  // Handler
  std::list<nrpe::packet> handle(nrpe::packet packet, const std::string& peer_identity);

  nrpe::packet create_error(std::string msg) { return nrpe::packet::create_response(nrpe::data::version2, 3, msg, payload_length_); }

  void log_debug(std::string module, std::string file, int line, std::string msg) const {
    if (get_core()->should_log(NSCAPI::log_level::debug)) {
      get_core()->log(NSCAPI::log_level::debug, file, line, msg);
    }
  }
  void log_error(std::string module, std::string file, int line, std::string msg) const {
    if (get_core()->should_log(NSCAPI::log_level::error)) {
      get_core()->log(NSCAPI::log_level::error, file, line, msg);
    }
  }
  unsigned int get_payload_length() { return payload_length_; }

 private:
  socket_helpers::connection_info info_;
  std::shared_ptr<nrpe::server::server> server_;
};
