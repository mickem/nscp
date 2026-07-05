// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "web_installer.hpp"

#include "web_installer_detail.hpp"

#include <config.h>

#include <bytes/unzip.hpp>
#include <net/http/client.hpp>
#include <net/http/http_request.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>

#ifdef USE_SSL
#include <openssl/evp.h>
#include <openssl/sha.h>
#endif

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace fs = boost::filesystem;
namespace json = boost::json;

namespace nsclient {
namespace web {

namespace detail {

// Bare-bones URL parser: "https://host[:port]/path". Returns false on anything
// that doesn't look like an absolute HTTP/HTTPS URL — sufficient for the
// well-formed redirect targets GitHub emits.
bool parse_url(const std::string& url, parsed_url& out) {
  const auto sep = url.find("://");
  if (sep == std::string::npos) return false;
  out.protocol = url.substr(0, sep);
  if (out.protocol != "http" && out.protocol != "https") return false;
  const auto rest = sep + 3;
  const auto slash = url.find('/', rest);
  std::string authority = (slash == std::string::npos) ? url.substr(rest) : url.substr(rest, slash - rest);
  out.path = (slash == std::string::npos) ? "/" : url.substr(slash);
  const auto colon = authority.find(':');
  if (colon == std::string::npos) {
    out.host = authority;
    out.port = (out.protocol == "https") ? "443" : "80";
  } else {
    out.host = authority.substr(0, colon);
    out.port = authority.substr(colon + 1);
  }
  return !out.host.empty();
}

bool is_unsafe_zip_path(const std::string& filename) {
  if (filename.empty()) return false;
  // Leading separator (either kind) → absolute path.
  if (filename.front() == '/' || filename.front() == '\\') return true;
  // Windows drive prefix like "C:" / "c:".
  if (filename.size() >= 2 && std::isalpha(static_cast<unsigned char>(filename[0])) && filename[1] == ':') return true;
  // Walk segments split on either separator, reject any equal to "..".
  std::string seg;
  for (const char c : filename) {
    if (c == '/' || c == '\\') {
      if (seg == "..") return true;
      seg.clear();
    } else {
      seg += c;
    }
  }
  return seg == "..";
}

// "<hex>  <filename>" → "<hex>", lowercased. Tolerant of trailing newline and
// of either two-space or single-space separators (sha256sum and shasum differ).
std::string parse_sha256_manifest(const std::string& contents) {
  const auto nl = contents.find_first_of("\r\n");
  std::string first_line = (nl == std::string::npos) ? contents : contents.substr(0, nl);
  boost::algorithm::trim(first_line);
  const auto sp = first_line.find_first_of(" \t");
  std::string hex = (sp == std::string::npos) ? first_line : first_line.substr(0, sp);
  for (char& c : hex) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  // SHA-256 is 64 hex chars; reject anything else as malformed.
  if (hex.size() != 64) return {};
  for (const char c : hex) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return {};
  }
  return hex;
}

}  // namespace detail

namespace {

constexpr int kMaxRedirects = 5;
constexpr auto kManifestFile = ".nscp-web-manifest.json";
// Default GitHub release URL base. Overridable via --url; the project's own
// fork/mirror is enough for the default install.
constexpr auto kDefaultUrlBase = "https://github.com/mickem/nscp/releases/download";

using detail::parsed_url;
using detail::parse_url;
using detail::parse_sha256_manifest;
using detail::is_unsafe_zip_path;

// One GET that does *not* throw on 3xx, so we can read the Location header.
// Mirrors what `simple_client::download` does but with fetch() instead of
// execute() and with our own redirect loop on top.
bool http_get_once(const std::string& ca_path, const parsed_url& u, http::response& resp, std::string& error) {
  try {
    const std::string tls_version = "tlsv1.2+";
    const std::string verify_mode = "peer-cert";
    const http::http_client_options options(u.protocol, tls_version, verify_mode, ca_path, http::proxy_config());
    http::simple_client client(options);
    http::request req("GET", u.host, u.path);
    // GitHub anonymous downloads work without an explicit UA, but a real one
    // makes server-side logs and rate-limit accounting saner.
    req.add_header("User-Agent", "nscp-web-installer");
    resp = client.fetch(u.host, u.port, req);
    return true;
  } catch (const socket_helpers::socket_exception& e) {
    error = e.reason();
    return false;
  } catch (const std::exception& e) {
    error = std::string("HTTP error: ") + e.what();
    return false;
  }
}

bool http_get(const std::string& ca_path, std::string url, std::string& body, std::string& final_url, std::string& error) {
  for (int hop = 0; hop <= kMaxRedirects; ++hop) {
    parsed_url u;
    if (!parse_url(url, u)) {
      error = "Malformed URL: " + url;
      return false;
    }
    http::response resp;
    if (!http_get_once(ca_path, u, resp, error)) return false;
    if (resp.is_2xx()) {
      body = std::move(resp.payload_);
      final_url = url;
      return true;
    }
    // Treat any 3xx with a Location: header as a redirect; GitHub uses 302.
    if (resp.status_code_ >= 300 && resp.status_code_ < 400) {
      const auto it = resp.headers_.find("location");
      if (it == resp.headers_.end() || it->second.empty()) {
        error = "Redirect " + std::to_string(resp.status_code_) + " without Location header";
        return false;
      }
      std::string next = it->second;
      boost::algorithm::trim(next);
      // Relative redirect → glue back onto the previous origin.
      if (next.find("://") == std::string::npos) {
        if (next.empty() || next[0] != '/') next = "/" + next;
        next = u.protocol + "://" + u.host + (u.port == "80" || u.port == "443" ? "" : ":" + u.port) + next;
      }
      url = next;
      continue;
    }
    error = "HTTP " + std::to_string(resp.status_code_) + " " + resp.status_message_;
    return false;
  }
  error = "Too many redirects (>" + std::to_string(kMaxRedirects) + ")";
  return false;
}

#ifdef USE_SSL
std::string sha256_hex(const std::string& bytes) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");
  if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
      EVP_DigestUpdate(ctx, bytes.data(), bytes.size()) != 1 ||
      EVP_DigestFinal_ex(ctx, digest, nullptr) != 1) {
    EVP_MD_CTX_free(ctx);
    throw std::runtime_error("SHA-256 digest failed");
  }
  EVP_MD_CTX_free(ctx);
  std::ostringstream oss;
  for (const unsigned char c : digest) oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  return oss.str();
}
#else
// This build has no OpenSSL, so the bundle integrity check can't run. Fail
// closed rather than silently installing unverified content: sha256_file()
// propagates this and install() reports it (return 1). Mirrors
// http::simple_client, which likewise throws for HTTPS in no-TLS builds — and
// since the download path needs HTTPS to reach GitHub, it would fail there too.
std::string sha256_hex(const std::string& /*bytes*/) {
  throw std::runtime_error("SHA-256 verification requires an OpenSSL-enabled build");
}
#endif

std::string sha256_file(const fs::path& path) {
  std::ifstream in(path.string(), std::ios::binary);
  if (!in) throw std::runtime_error("Cannot open for hashing: " + path.string());
  std::ostringstream buf;
  buf << in.rdbuf();
  return sha256_hex(buf.str());
}

std::string iso8601_utc_now() {
  using namespace std::chrono;
  const auto now = system_clock::to_time_t(system_clock::now());
  std::tm tm{};
#ifdef WIN32
  gmtime_s(&tm, &now);
#else
  gmtime_r(&now, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

// Binary-safe whole-file write. Used for both the (textual) manifest JSON and
// the (binary) downloaded zip body — open in binary mode so neither path gets
// CRLF-translated.
bool write_file(const fs::path& p, const std::string& contents) {
  std::ofstream out(p.string(), std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  return static_cast<bool>(out);
}

bool read_file(const fs::path& p, std::string& out) {
  std::ifstream in(p.string(), std::ios::binary);
  if (!in) return false;
  std::ostringstream buf;
  buf << in.rdbuf();
  out = buf.str();
  return true;
}

// Pull every entry out of the zip into `dest`, creating subdirs as needed.
// Returns the relative paths of every file extracted, or empty on failure.
std::vector<std::string> extract_all(const fs::path& zip_path, const fs::path& dest, std::string& error) {
  bytes::unzip::reader archive;
  if (!archive.open(zip_path.string())) {
    error = "Failed to open zip: " + zip_path.string();
    return {};
  }
  std::vector<std::string> extracted;
  for (unsigned int i = 0; i < archive.size(); ++i) {
    bytes::unzip::file_entry entry;
    if (!archive.stat(i, entry)) {
      error = "Failed to read entry " + std::to_string(i);
      return {};
    }
    // Zip directory entries end in '/'. Skip them; subdirectories are created
    // implicitly when we create_directories() for each file entry below.
    if (!entry.filename.empty() && entry.filename.back() == '/') continue;
    // Path-traversal guard: refuse any entry that escapes the destination.
    if (is_unsafe_zip_path(entry.filename)) {
      error = "Rejecting unsafe zip entry: " + entry.filename;
      return {};
    }
    const fs::path target = dest / entry.filename;
    boost::system::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    if (!archive.extract_to_file(entry.filename, target.string())) {
      error = "Failed to extract " + entry.filename;
      return {};
    }
    extracted.push_back(entry.filename);
  }
  return extracted;
}

bool web_path_contains_files(const fs::path& web_path) {
  boost::system::error_code ec;
  if (!fs::is_directory(web_path, ec)) return false;
  fs::directory_iterator it(web_path, ec), end;
  return it != end;
}

bool write_manifest(const fs::path& web_path, const std::string& version, const std::string& source_url, const std::string& sha256,
                    const std::vector<std::string>& files) {
  json::object root;
  root["version"] = version;
  root["source_url"] = source_url;
  root["sha256"] = sha256;
  root["installed_at"] = iso8601_utc_now();
  json::array files_arr;
  for (const std::string& f : files) files_arr.emplace_back(f);
  root["files"] = std::move(files_arr);
  return write_file(web_path / kManifestFile, json::serialize(root));
}

bool read_manifest(const fs::path& web_path, json::object& out, std::string& error) {
  std::string raw;
  if (!read_file(web_path / kManifestFile, raw)) {
    error = "No manifest found at " + (web_path / kManifestFile).string();
    return false;
  }
  try {
    out = json::parse(raw).as_object();
    return true;
  } catch (const std::exception& e) {
    error = std::string("Manifest is not valid JSON: ") + e.what();
    return false;
  }
}

}  // namespace

web_installer::web_installer(path_resolver resolve) : resolve_(std::move(resolve)) {}

int web_installer::install(const options& opts, std::ostream& out) const {
  // Use STRPRODUCTVER (just "X.Y.Z") rather than CURRENT_SERVICE_VERSION
  // (which is "X.Y.Z YYYY-MM-DD") so the asset filename matches what CI
  // publishes in NSCP-Web-${VERSION}.zip.
  const std::string version = opts.version.empty() ? std::string(STRPRODUCTVER) : opts.version;
  const std::string url_base = opts.url_base.empty() ? std::string(kDefaultUrlBase) : opts.url_base;
  const std::string asset_zip = "NSCP-Web-" + version + ".zip";
  const std::string asset_sha = "NSCP-Web-" + version + ".sha256";

  const fs::path web_path = resolve_("${web-path}");
  const fs::path temp_path = resolve_("${temp}");

  out << "Web bundle install" << std::endl;
  out << "  version: " << version << std::endl;
  out << "  target : " << web_path.string() << std::endl;

  if (web_path_contains_files(web_path) && !opts.force) {
    // Refuse silently-destructive installs. If a previous install is present
    // (manifest exists), tell the user how to remove it; otherwise flag the
    // foreign content and refer them to --force.
    if (fs::exists(web_path / kManifestFile)) {
      out << "A web bundle is already installed at " << web_path << "." << std::endl;
      out << "Run `nscp web uninstall-ui` first, or pass --force to overwrite." << std::endl;
    } else {
      out << web_path << " already contains files (no install manifest)." << std::endl;
      out << "Refusing to overwrite without --force." << std::endl;
    }
    return 2;
  }

  fs::path zip_local;
  std::string expected_hash;
  std::string source_url;
  // True when zip_local started in --from but got copied to temp because
  // it lived under web_path (see the --force branch below). The cleanup
  // logic at the end uses this to decide whether to remove zip_local.
  bool stashed_from_path = false;

  if (!opts.from_path.empty()) {
    zip_local = opts.from_path;
    source_url = "file://" + fs::absolute(zip_local).string();
    if (!fs::is_regular_file(zip_local)) {
      out << "Local zip not found: " << zip_local << std::endl;
      return 1;
    }
    // Optional sibling sha256 file (basename + .sha256, e.g. foo.zip.sha256,
    // or the .zip extension swapped for .sha256, e.g. foo.sha256). When
    // present we verify; when absent we skip — the user opted into trusting
    // a local path. But a present-yet-unreadable/malformed file is a hard
    // error: the user asked for verification by placing it there, so silently
    // downgrading to "no checksum" would defeat the point (this mirrors the
    // strict handling of the downloaded manifest below).
    fs::path sha_file = zip_local.string() + ".sha256";
    if (!fs::is_regular_file(sha_file)) {
      sha_file = zip_local.parent_path() / (zip_local.stem().string() + ".sha256");
    }
    if (fs::is_regular_file(sha_file)) {
      std::string raw;
      if (!read_file(sha_file, raw)) {
        out << "Failed to read checksum file: " << sha_file << std::endl;
        return 1;
      }
      expected_hash = parse_sha256_manifest(raw);
      if (expected_hash.empty()) {
        out << "Checksum file is malformed (expected SHA-256 hex + filename): " << sha_file << std::endl;
        return 1;
      }
    }
    out << "  source : " << source_url << std::endl;
  } else {
    const std::string ca = resolve_("${ca-path}");
    const std::string url_zip = url_base + "/" + version + "/" + asset_zip;
    const std::string url_sha = url_base + "/" + version + "/" + asset_sha;
    out << "  source : " << url_zip << std::endl;

    if (opts.dry_run) {
      out << "Dry-run: would download zip + sha256, verify, extract to " << web_path << std::endl;
      return 0;
    }

    std::string body, final_url, error;
    if (!http_get(ca, url_sha, body, final_url, error)) {
      out << "Failed to fetch checksum manifest (" << url_sha << "): " << error << std::endl;
      return 1;
    }
    expected_hash = parse_sha256_manifest(body);
    if (expected_hash.empty()) {
      out << "Checksum manifest is malformed (expected SHA-256 hex + filename): " << body << std::endl;
      return 1;
    }

    std::string zip_body;
    if (!http_get(ca, url_zip, zip_body, final_url, error)) {
      out << "Failed to fetch web bundle (" << url_zip << "): " << error << std::endl;
      return 1;
    }
    source_url = url_zip;

    boost::system::error_code ec;
    fs::create_directories(temp_path, ec);
    zip_local = temp_path / ("nsclient-web-" + version + ".zip");
    if (!write_file(zip_local, zip_body)) {
      out << "Failed to write temp zip: " << zip_local << std::endl;
      return 1;
    }
  }

  if (opts.dry_run) {
    out << "Dry-run: would verify and extract " << zip_local << " to " << web_path << std::endl;
    return 0;
  }

  // Hash + verify before touching the destination, so a tampered/corrupt
  // download never half-installs.
  std::string actual_hash;
  try {
    actual_hash = sha256_file(zip_local);
  } catch (const std::exception& e) {
    out << "Failed to hash " << zip_local << ": " << e.what() << std::endl;
    return 1;
  }
  if (!expected_hash.empty() && actual_hash != expected_hash) {
    out << "SHA-256 mismatch!" << std::endl;
    out << "  expected: " << expected_hash << std::endl;
    out << "  actual  : " << actual_hash << std::endl;
    out << "Leaving tampered file in place for inspection: " << zip_local << std::endl;
    return 3;
  }
  if (expected_hash.empty()) {
    out << "  warn   : no checksum to verify against (--from path without sibling .sha256)" << std::endl;
  } else {
    out << "  sha256 : " << actual_hash << " (verified)" << std::endl;
  }

  // Wipe destination if --force; in the non-force branch we already returned
  // above when files existed. Guard against the case where zip_local lives
  // *inside* web_path (e.g. `--from ${web-path}/saved.zip --force`): wiping
  // web_path would delete the source out from under us before extract_all
  // can re-open it. Copy it into the temp dir first.
  if (opts.force && fs::exists(web_path)) {
    boost::system::error_code rel_ec;
    const fs::path zip_abs = fs::absolute(zip_local).lexically_normal();
    const fs::path web_abs = fs::absolute(web_path).lexically_normal();
    const fs::path rel = zip_abs.lexically_relative(web_abs);
    const bool under_web = !rel.empty() && rel.begin() != rel.end() && rel.begin()->string() != "..";
    if (under_web) {
      boost::system::error_code mk_ec;
      fs::create_directories(temp_path, mk_ec);
      const fs::path stash = temp_path / fs::unique_path("nsclient-web-stash-%%%%-%%%%.zip");
      boost::system::error_code cp_ec;
      fs::copy_file(zip_local, stash, cp_ec);
      if (cp_ec) {
        out << "Failed to stash source zip before wipe (" << zip_local << " -> " << stash << "): " << cp_ec.message() << std::endl;
        return 1;
      }
      zip_local = stash;
      // Mark this stash as ours so the cleanup at the end of install() removes
      // it even though the user passed --from.
      stashed_from_path = true;
    }
    boost::system::error_code ec;
    fs::remove_all(web_path, ec);
  }
  boost::system::error_code ec;
  fs::create_directories(web_path, ec);

  std::string err;
  const std::vector<std::string> files = extract_all(zip_local, web_path, err);
  if (files.empty()) {
    out << "Extraction failed: " << err << std::endl;
    return 1;
  }

  if (!write_manifest(web_path, version, source_url, actual_hash, files)) {
    out << "Failed to write manifest at " << (web_path / kManifestFile) << std::endl;
    return 1;
  }

  // Best-effort cleanup of the temp download or the stashed copy of a
  // --from zip. Leave it on failure so the user can re-check, but on success
  // it has served its purpose. We never remove the user's original --from
  // path — only the temp file we created.
  if (opts.from_path.empty() || stashed_from_path) {
    boost::system::error_code rm_ec;
    fs::remove(zip_local, rm_ec);
  }

  out << "Installed " << files.size() << " file(s) to " << web_path << "." << std::endl;
  return 0;
}

int web_installer::uninstall(const bool force, std::ostream& out) const {
  const fs::path web_path = resolve_("${web-path}");
  std::string error;
  json::object manifest;
  if (!read_manifest(web_path, manifest, error)) {
    out << error << std::endl;
    if (!force) return 2;
    out << "Continuing because --force was given." << std::endl;
  }

  std::size_t removed = 0;
  if (manifest.contains("files")) {
    for (const auto& v : manifest["files"].as_array()) {
      const fs::path rel(v.as_string().c_str());
      const fs::path full = web_path / rel;
      boost::system::error_code ec;
      if (fs::exists(full, ec)) {
        fs::remove(full, ec);
        if (!ec) ++removed;
      }
    }
  }
  boost::system::error_code ec;
  fs::remove(web_path / kManifestFile, ec);

  // Clean up empty subdirectories that the install created. Walk bottom-up so
  // parents are checked after their children. Operator-owned files survive
  // because their containing directories will still be non-empty.
  if (fs::exists(web_path, ec)) {
    std::vector<fs::path> dirs;
    for (fs::recursive_directory_iterator it(web_path, ec), end; it != end; ++it) {
      if (fs::is_directory(it->status())) dirs.push_back(it->path());
    }
    std::sort(dirs.begin(), dirs.end(), [](const fs::path& a, const fs::path& b) { return a.string().size() > b.string().size(); });
    for (const fs::path& d : dirs) {
      boost::system::error_code rm_ec;
      if (fs::is_empty(d, rm_ec)) fs::remove(d, rm_ec);
    }
  }

  out << "Removed " << removed << " file(s) from " << web_path << "." << std::endl;
  return 0;
}

int web_installer::status(std::ostream& out) const {
  const fs::path web_path = resolve_("${web-path}");
  out << "Web path: " << web_path << std::endl;
  if (!fs::exists(web_path)) {
    out << "Status  : not installed (directory does not exist)" << std::endl;
    return 2;
  }
  json::object manifest;
  std::string error;
  if (!read_manifest(web_path, manifest, error)) {
    out << "Status  : not installed (" << error << ")" << std::endl;
    return 2;
  }
  const auto get_str = [&](const char* key) -> std::string {
    if (!manifest.contains(key)) return "<missing>";
    const auto& v = manifest[key];
    return v.is_string() ? std::string(v.as_string().c_str()) : json::serialize(v);
  };
  std::size_t file_count = 0;
  if (manifest.contains("files") && manifest["files"].is_array()) file_count = manifest["files"].as_array().size();
  out << "Status  : installed" << std::endl;
  out << "Version : " << get_str("version") << std::endl;
  out << "Source  : " << get_str("source_url") << std::endl;
  out << "SHA-256 : " << get_str("sha256") << std::endl;
  out << "Date    : " << get_str("installed_at") << std::endl;
  out << "Files   : " << file_count << std::endl;
  return 0;
}

}  // namespace web
}  // namespace nsclient
