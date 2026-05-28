#include "static_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <fstream>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#define BUF_SIZE 4096

namespace {
// Shown on `/` (and any page-navigation request) when the web bundle isn't
// installed. Kept small and self-contained: a single <style> block, no
// external fonts, scripts, or images. The whole point is that this page
// works when *nothing* else does.
const std::string kPlaceholderHtml = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>NSClient++ &mdash; Web UI not installed</title>
<style>
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f5f6fa; color: #1f2330; margin: 0; padding: 3rem 1.25rem; }
  main { max-width: 640px; margin: 0 auto; background: #fff; border-radius: 8px; padding: 2rem 2.25rem; box-shadow: 0 2px 8px rgba(0,0,0,0.06); }
  h1 { margin: 0 0 0.75rem; font-size: 1.45rem; }
  p { line-height: 1.55; margin: 0.5rem 0; }
  code { background: #eef0f4; padding: 0.1rem 0.4rem; border-radius: 3px; font-size: 0.95em; }
  pre { background: #1f2330; color: #dde0e8; padding: 0.85rem 1rem; border-radius: 5px; overflow-x: auto; }
  .muted { color: #6b7280; font-size: 0.9rem; margin-top: 1.5rem; }
</style>
</head>
<body>
<main>
<h1>NSClient++ is running &mdash; web UI not installed</h1>
<p>The REST API on this host is serving requests, but no web bundle
has been unpacked into the configured <code>web-path</code>.</p>
<p>Install the matching UI bundle with:</p>
<pre>sudo nscp web install-ui</pre>
<p>That command downloads the bundle that matches this daemon
version from the project's GitHub Release and unpacks it into
<code>web-path</code>. For offline / air-gapped installs use
<code>nscp web install-ui --from /path/to/NSCP-Web-X.Y.Z.zip</code>.</p>
<p class="muted">This placeholder is built into the daemon and only
appears when the web bundle hasn't been installed yet. Once it is,
this page is replaced by the real UI.</p>
</main>
</body>
</html>
)HTML";
}  // namespace

const std::string &StaticController::placeholder_html() { return kPlaceholderHtml; }

bool nonAsciiChar(const char c) { return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'); }
void stripNonAscii(std::string &str) { str.erase(std::remove_if(str.begin(), str.end(), nonAsciiChar), str.end()); }

bool nonPathChar(const char c) {
  return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '/' || c == '-');
}
std::string stripPath(std::string str) {
  str.erase(std::remove_if(str.begin(), str.end(), nonPathChar), str.end());
  return str;
}
StaticController::StaticController(const std::shared_ptr<session_manager_interface> &session, const std::string &path) : session(session), base(path) {}

Mongoose::Response *StaticController::handleRequest(Mongoose::Request &request) {
  const bool is_js = boost::algorithm::ends_with(request.getUrl(), ".js");
  const bool is_png = boost::algorithm::ends_with(request.getUrl(), ".png");
  auto *sr = new Mongoose::StreamResponse();
  std::string path = stripPath(request.getUrl());
  if (path != "/nscp.png" && path != "/assets/index.js") {
    path = "/index.html";
  }

  boost::filesystem::path file = boost::filesystem::path(base / path).lexically_normal();
  boost::filesystem::path base_normal = boost::filesystem::path(base).lexically_normal();
  // Ensure the resolved path stays within the served base directory
  const std::string file_str = file.string();
  std::string base_str = base_normal.string();
  if (!base_str.empty() && base_str.back() != static_cast<char>(boost::filesystem::path::preferred_separator))
    base_str += static_cast<char>(boost::filesystem::path::preferred_separator);
  if (file_str.rfind(base_str, 0) != 0) {
    sr->setCodeServerError("Invalid path: " + path);
    return sr;
  }

  if (!boost::filesystem::is_regular_file(file)) {
    // Page-navigation requests get coerced to /index.html above. When that
    // file is missing (no web bundle installed yet — see
    // docs/design/web-bundle-installer.md Phase 3) we serve a built-in
    // placeholder that points the operator at `nscp web install-ui`.
    // Missing JS / PNG assets keep returning 404 so genuine bundle bugs
    // stay visible.
    if (path == "/index.html") {
      sr->setHeader("Content-Type", "text/html");
      sr->write(kPlaceholderHtml.data(), kPlaceholderHtml.size());
      return sr;
    }
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
