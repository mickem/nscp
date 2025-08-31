#include "static_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <fstream>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#define BUF_SIZE 4096

bool nonAsciiChar(const char c) { return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'); }
void stripNonAscii(std::string &str) { str.erase(std::remove_if(str.begin(), str.end(), nonAsciiChar), str.end()); }

bool nonPathChar(const char c) {
  return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '/' || c == '-');
}
std::string stripPath(std::string str) {
  str.erase(std::remove_if(str.begin(), str.end(), nonPathChar), str.end());
  return str;
}
StaticController::StaticController(const boost::shared_ptr<session_manager_interface> &session, const std::string &path) : session(session), base(path) {}

Mongoose::Response *StaticController::handleRequest(Mongoose::Request &request) {
  const bool is_js = boost::algorithm::ends_with(request.getUrl(), ".js");
  const bool is_png = boost::algorithm::ends_with(request.getUrl(), ".png");
  auto *sr = new Mongoose::StreamResponse();
  std::string path = stripPath(request.getUrl());
  if (path.find("..") != std::string::npos) {
    sr->setCodeServerError("Invalid path: " + path);
    return sr;
  }
  if (path != "/nscp.png" && path != "/assets/index.js") {
    path = "/index.html";
  }

  boost::filesystem::path file = base / path;
  if (!boost::filesystem::is_regular_file(file)) {
    NSC_LOG_ERROR("Requested resource was not found: " + file.string());
    sr->setCodeNotFound("Not found: " + path);
    return sr;
  }

  if (is_js)
    sr->setHeader("Content-Type", "application/javascript");
  else if (is_png)
    sr->setHeader("Content-Type", "image/png");
  else {
    sr->setHeader("Content-Type", "text/html");
  }
  if (is_png || is_js) {
    sr->setHeader("Cache-Control", "max-age=3600");  // 1 hour (60*60)
  }
  std::ifstream in(file.string().c_str(), std::ios_base::in | std::ios_base::binary);
  char buf[BUF_SIZE];

  do {
    in.read(&buf[0], BUF_SIZE);
    sr->write(&buf[0], in.gcount());
  } while (in.gcount() > 0);
  in.close();
  return sr;
}
bool StaticController::handles(std::string method, const std::string url) {
  return boost::algorithm::ends_with(url, ".js") || boost::algorithm::ends_with(url, ".html") || boost::algorithm::ends_with(url, ".png") ||
         boost::algorithm::ends_with(url, "/") || boost::algorithm::starts_with(url, "/modules") || boost::algorithm::starts_with(url, "/queries") ||
         boost::algorithm::starts_with(url, "/settings") || boost::algorithm::starts_with(url, "/metrics") || boost::algorithm::starts_with(url, "/logs");
  ;
}
