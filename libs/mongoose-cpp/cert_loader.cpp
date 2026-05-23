#include "cert_loader.h"

#include <fstream>
#include <nsclient/nsclient_exception.hpp>
#include <sstream>

namespace Mongoose {
namespace cert_loader {

std::string load_file(const std::string& path, const std::string& hint) {
  try {
    const std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  } catch (const std::exception& e) {
    throw nsclient::nsclient_exception("Failed to load " + hint + " from " + path + ": " + e.what());
  }
}

std::pair<std::string, std::string> load_certificates(const std::string& cert_path, const std::string& key_path) {
  auto cert = load_file(cert_path, "certificate");
  if (key_path.empty()) {
    return {cert, cert};
  }
  auto key = load_file(key_path, "private key");
  return {cert, key};
}

}  // namespace cert_loader
}  // namespace Mongoose
