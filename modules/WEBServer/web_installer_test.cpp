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

#include "web_installer.hpp"

#include "web_installer_detail.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <boost/json.hpp>

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace fs = boost::filesystem;
namespace json = boost::json;

namespace {

// Same fixture used in include/bytes/unzip_test.cpp: a deterministic deflate
// archive with three entries (module.json, scripts/hello.txt, empty.txt).
// Inlined rather than shared because the installer test only needs *some*
// valid zip — coupling the two test files would just create lifetime/build
// dependencies for no gain.
const unsigned char kZipFixture[] = {
    0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x21, 0x50, 0xe3, 0x9f, 0x71, 0xd9, 0x2d, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x00, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x6d, 0x6f, 0x64, 0x75, 0x6c, 0x65, 0x2e, 0x6a, 0x73, 0x6f, 0x6e, 0xab, 0x56, 0xca, 0x4b, 0xcc, 0x4d, 0x55, 0xb2, 0x52, 0x4a, 0x49,
    0xcd, 0xcd, 0x57, 0xd2, 0x51, 0x4a, 0x49, 0x2d, 0x4e, 0x2e, 0xca, 0x2c, 0x28, 0xc9, 0xcc, 0xcf, 0x53, 0xb2, 0x52, 0x72, 0x54, 0x00, 0x09, 0x2b, 0x14, 0xe4,
    0x94, 0xa6, 0x67, 0xe6, 0x29, 0xd5, 0x02, 0x00, 0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x21, 0x50, 0xcb, 0xd5, 0x17, 0x3b,
    0x10, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x73, 0x2f, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x2e,
    0x74, 0x78, 0x74, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51, 0x28, 0xcd, 0xab, 0xca, 0x2c, 0x50, 0xe4, 0x02, 0x00, 0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00, 0x00, 0x21, 0x50, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x65, 0x6d, 0x70,
    0x74, 0x79, 0x2e, 0x74, 0x78, 0x74, 0x03, 0x00, 0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x21, 0x50, 0xe3, 0x9f,
    0x71, 0xd9, 0x2d, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x6d, 0x6f, 0x64, 0x75, 0x6c, 0x65, 0x2e, 0x6a, 0x73, 0x6f, 0x6e, 0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
    0x00, 0x21, 0x50, 0xcb, 0xd5, 0x17, 0x3b, 0x10, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x01, 0x56, 0x00, 0x00, 0x00, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x73, 0x2f, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x2e, 0x74, 0x78, 0x74, 0x50, 0x4b,
    0x01, 0x02, 0x14, 0x00, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x21, 0x50, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x95, 0x00, 0x00, 0x00, 0x65, 0x6d, 0x70, 0x74, 0x79, 0x2e, 0x74, 0x78,
    0x74, 0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0xaf, 0x00, 0x00, 0x00, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00};

constexpr auto kManifestFile = ".nscp-web-manifest.json";

std::string sha256_hex_of_file(const fs::path& path) {
  std::ifstream in(path.string(), std::ios::binary);
  std::ostringstream buf;
  buf << in.rdbuf();
  const std::string bytes = buf.str();
  unsigned char digest[SHA256_DIGEST_LENGTH];
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
  EVP_DigestUpdate(ctx, bytes.data(), bytes.size());
  EVP_DigestFinal_ex(ctx, digest, nullptr);
  EVP_MD_CTX_free(ctx);
  std::ostringstream oss;
  for (const unsigned char c : digest) oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  return oss.str();
}

void write_file(const fs::path& p, const std::string& contents) {
  std::ofstream out(p.string(), std::ios::binary | std::ios::trunc);
  out << contents;
}

std::string read_file(const fs::path& p) {
  std::ifstream in(p.string(), std::ios::binary);
  std::ostringstream buf;
  buf << in.rdbuf();
  return buf.str();
}

class WebInstallerTest : public ::testing::Test {
 protected:
  fs::path root_;       // Sandbox root for this test.
  fs::path web_path_;   // ${web-path} for the resolver.
  fs::path temp_path_;  // ${temp} for the resolver.
  fs::path zip_path_;   // On-disk fixture zip.

  void SetUp() override {
    root_ = fs::temp_directory_path() / fs::unique_path("nscp-webinst-%%%%-%%%%");
    web_path_ = root_ / "web";
    temp_path_ = root_ / "tmp";
    fs::create_directories(temp_path_);
    zip_path_ = root_ / "fixture.zip";
    std::ofstream out(zip_path_.string(), std::ios::binary);
    out.write(reinterpret_cast<const char*>(kZipFixture), sizeof(kZipFixture));
  }

  void TearDown() override {
    boost::system::error_code ec;
    fs::remove_all(root_, ec);
  }

  // Resolver returning sandboxed paths for the well-known variables the
  // installer expands. ${ca-path} is irrelevant in the from_path code path
  // (no HTTP) so we just return a placeholder.
  nsclient::web::web_installer::path_resolver resolver() const {
    const fs::path web = web_path_;
    const fs::path tmp = temp_path_;
    return [web, tmp](const std::string& p) -> std::string {
      if (p == "${web-path}") return web.string();
      if (p == "${temp}") return tmp.string();
      if (p == "${ca-path}") return "";
      return p;
    };
  }

  nsclient::web::web_installer::options local_install_opts() const {
    nsclient::web::web_installer::options opts;
    opts.from_path = zip_path_.string();
    opts.version = "0.0.0-test";
    return opts;
  }
};

// ---------------------------------------------------------------------------
// detail::parse_url
// ---------------------------------------------------------------------------

TEST(WebInstallerParseUrl, ParsesHttpsWithImplicitPort) {
  nsclient::web::detail::parsed_url u;
  ASSERT_TRUE(nsclient::web::detail::parse_url("https://example.com/path/to/file", u));
  EXPECT_EQ(u.protocol, "https");
  EXPECT_EQ(u.host, "example.com");
  EXPECT_EQ(u.port, "443");
  EXPECT_EQ(u.path, "/path/to/file");
}

TEST(WebInstallerParseUrl, ParsesHttpWithImplicitPort) {
  nsclient::web::detail::parsed_url u;
  ASSERT_TRUE(nsclient::web::detail::parse_url("http://example.com/x", u));
  EXPECT_EQ(u.protocol, "http");
  EXPECT_EQ(u.port, "80");
  EXPECT_EQ(u.path, "/x");
}

TEST(WebInstallerParseUrl, ParsesExplicitPort) {
  nsclient::web::detail::parsed_url u;
  ASSERT_TRUE(nsclient::web::detail::parse_url("https://example.com:8443/foo", u));
  EXPECT_EQ(u.host, "example.com");
  EXPECT_EQ(u.port, "8443");
  EXPECT_EQ(u.path, "/foo");
}

TEST(WebInstallerParseUrl, DefaultsPathToSlashWhenAbsent) {
  nsclient::web::detail::parsed_url u;
  ASSERT_TRUE(nsclient::web::detail::parse_url("https://example.com", u));
  EXPECT_EQ(u.host, "example.com");
  EXPECT_EQ(u.path, "/");
}

TEST(WebInstallerParseUrl, KeepsQueryStringInPath) {
  nsclient::web::detail::parsed_url u;
  // GitHub redirect targets often carry a signed query string — the parser
  // must keep it intact since the server uses it for auth.
  ASSERT_TRUE(nsclient::web::detail::parse_url("https://example.com/a?token=xyz&x=1", u));
  EXPECT_EQ(u.path, "/a?token=xyz&x=1");
}

TEST(WebInstallerParseUrl, RejectsNonAbsoluteUrl) {
  nsclient::web::detail::parsed_url u;
  EXPECT_FALSE(nsclient::web::detail::parse_url("example.com/foo", u));
  EXPECT_FALSE(nsclient::web::detail::parse_url("/just/a/path", u));
  EXPECT_FALSE(nsclient::web::detail::parse_url("", u));
}

TEST(WebInstallerParseUrl, RejectsUnsupportedScheme) {
  nsclient::web::detail::parsed_url u;
  EXPECT_FALSE(nsclient::web::detail::parse_url("ftp://example.com/foo", u));
  EXPECT_FALSE(nsclient::web::detail::parse_url("file:///etc/passwd", u));
}

TEST(WebInstallerParseUrl, RejectsEmptyHost) {
  nsclient::web::detail::parsed_url u;
  EXPECT_FALSE(nsclient::web::detail::parse_url("https:///path", u));
}

// ---------------------------------------------------------------------------
// detail::parse_sha256_manifest
// ---------------------------------------------------------------------------

TEST(WebInstallerParseSha256, AcceptsTwoSpaceFormat) {
  // sha256sum default output.
  const std::string hex(64, 'a');
  EXPECT_EQ(nsclient::web::detail::parse_sha256_manifest(hex + "  NSCP-Web-1.2.3.zip\n"), hex);
}

TEST(WebInstallerParseSha256, AcceptsSingleSpaceFormat) {
  // BSD shasum / some CI tooling.
  const std::string hex(64, 'b');
  EXPECT_EQ(nsclient::web::detail::parse_sha256_manifest(hex + " file.zip"), hex);
}

TEST(WebInstallerParseSha256, AcceptsHexOnlyNoFilename) {
  const std::string hex(64, 'c');
  EXPECT_EQ(nsclient::web::detail::parse_sha256_manifest(hex), hex);
}

TEST(WebInstallerParseSha256, LowercasesUppercaseHex) {
  std::string upper(64, 'A');
  std::string lower(64, 'a');
  EXPECT_EQ(nsclient::web::detail::parse_sha256_manifest(upper + "  f"), lower);
}

TEST(WebInstallerParseSha256, RejectsShortHex) {
  EXPECT_TRUE(nsclient::web::detail::parse_sha256_manifest(std::string(63, 'a')).empty());
}

TEST(WebInstallerParseSha256, RejectsLongHex) {
  EXPECT_TRUE(nsclient::web::detail::parse_sha256_manifest(std::string(65, 'a')).empty());
}

TEST(WebInstallerParseSha256, RejectsNonHexChars) {
  std::string bad = std::string(63, 'a') + "z";
  EXPECT_TRUE(nsclient::web::detail::parse_sha256_manifest(bad + "  file").empty());
}

TEST(WebInstallerParseSha256, IgnoresLinesAfterFirst) {
  const std::string hex(64, 'd');
  // Multi-line manifests do exist (one digest per file); only the first one
  // matters because we feed it the single-asset manifest URL.
  EXPECT_EQ(nsclient::web::detail::parse_sha256_manifest(hex + "  a.zip\n" + std::string(64, 'e') + "  b.zip\n"), hex);
}

TEST(WebInstallerParseSha256, RejectsEmptyInput) {
  EXPECT_TRUE(nsclient::web::detail::parse_sha256_manifest("").empty());
  EXPECT_TRUE(nsclient::web::detail::parse_sha256_manifest("\n").empty());
}

// ---------------------------------------------------------------------------
// detail::is_unsafe_zip_path
// ---------------------------------------------------------------------------

TEST(WebInstallerIsUnsafeZipPath, AcceptsOrdinaryPaths) {
  EXPECT_FALSE(nsclient::web::detail::is_unsafe_zip_path("index.html"));
  EXPECT_FALSE(nsclient::web::detail::is_unsafe_zip_path("assets/app.js"));
  EXPECT_FALSE(nsclient::web::detail::is_unsafe_zip_path("a/b/c/d.txt"));
  EXPECT_FALSE(nsclient::web::detail::is_unsafe_zip_path(""));
}

TEST(WebInstallerIsUnsafeZipPath, AcceptsDotsInsideSegment) {
  // ".." anywhere in the byte stream used to false-positive. Real filenames
  // containing dots, including consecutive dots, must be allowed.
  EXPECT_FALSE(nsclient::web::detail::is_unsafe_zip_path("foo..bar.txt"));
  EXPECT_FALSE(nsclient::web::detail::is_unsafe_zip_path("..hidden"));
  EXPECT_FALSE(nsclient::web::detail::is_unsafe_zip_path("a/...secret"));
  EXPECT_FALSE(nsclient::web::detail::is_unsafe_zip_path("trailing.."));
}

TEST(WebInstallerIsUnsafeZipPath, RejectsDotDotSegmentForwardSlash) {
  EXPECT_TRUE(nsclient::web::detail::is_unsafe_zip_path("../etc/passwd"));
  EXPECT_TRUE(nsclient::web::detail::is_unsafe_zip_path("a/../b"));
  EXPECT_TRUE(nsclient::web::detail::is_unsafe_zip_path("a/b/.."));
}

TEST(WebInstallerIsUnsafeZipPath, RejectsDotDotSegmentBackslash) {
  // Old guard accepted these because it only checked "/" prefixes and the
  // ".." substring (which the segments don't contain literally on their own
  // here, but mixing separators sneaks past the substring check on Windows
  // where boost::filesystem treats '\\' as a separator).
  EXPECT_TRUE(nsclient::web::detail::is_unsafe_zip_path("..\\Windows\\evil"));
  EXPECT_TRUE(nsclient::web::detail::is_unsafe_zip_path("a\\..\\b"));
  EXPECT_TRUE(nsclient::web::detail::is_unsafe_zip_path("a/b\\..\\c"));
}

TEST(WebInstallerIsUnsafeZipPath, RejectsAbsolutePaths) {
  EXPECT_TRUE(nsclient::web::detail::is_unsafe_zip_path("/etc/passwd"));
  EXPECT_TRUE(nsclient::web::detail::is_unsafe_zip_path("\\Windows\\evil"));
}

TEST(WebInstallerIsUnsafeZipPath, RejectsWindowsDrivePrefix) {
  EXPECT_TRUE(nsclient::web::detail::is_unsafe_zip_path("C:\\Windows\\evil"));
  EXPECT_TRUE(nsclient::web::detail::is_unsafe_zip_path("c:/Users/x"));
  EXPECT_TRUE(nsclient::web::detail::is_unsafe_zip_path("Z:foo"));
}

// ---------------------------------------------------------------------------
// web_installer (public API, exercised via local install path)
// ---------------------------------------------------------------------------

TEST_F(WebInstallerTest, StatusReportsNotInstalledWhenDirMissing) {
  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  EXPECT_EQ(installer.status(out), 2);
  EXPECT_NE(out.str().find("not installed"), std::string::npos);
}

TEST_F(WebInstallerTest, InstallFromLocalZipSucceeds) {
  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  EXPECT_EQ(installer.install(local_install_opts(), out), 0) << out.str();

  EXPECT_TRUE(fs::is_regular_file(web_path_ / "module.json"));
  EXPECT_TRUE(fs::is_regular_file(web_path_ / "scripts" / "hello.txt"));
  EXPECT_TRUE(fs::is_regular_file(web_path_ / "empty.txt"));
  EXPECT_TRUE(fs::is_regular_file(web_path_ / kManifestFile));

  const std::string hello = read_file(web_path_ / "scripts" / "hello.txt");
  EXPECT_EQ(hello, "Hello, unzip!\n");
}

TEST_F(WebInstallerTest, InstallWritesManifestWithVersionAndFileList) {
  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  ASSERT_EQ(installer.install(local_install_opts(), out), 0) << out.str();

  const json::value v = json::parse(read_file(web_path_ / kManifestFile));
  const json::object& obj = v.as_object();
  EXPECT_EQ(obj.at("version").as_string(), "0.0.0-test");
  ASSERT_TRUE(obj.at("files").is_array());
  EXPECT_EQ(obj.at("files").as_array().size(), 3u);
  EXPECT_FALSE(std::string(obj.at("installed_at").as_string().c_str()).empty());
  EXPECT_FALSE(std::string(obj.at("source_url").as_string().c_str()).empty());
}

TEST_F(WebInstallerTest, InstallRejectsExistingInstallWithoutForce) {
  nsclient::web::web_installer installer(resolver());
  std::ostringstream first;
  ASSERT_EQ(installer.install(local_install_opts(), first), 0) << first.str();

  std::ostringstream second;
  EXPECT_EQ(installer.install(local_install_opts(), second), 2);
  EXPECT_NE(second.str().find("uninstall-ui"), std::string::npos);
}

TEST_F(WebInstallerTest, InstallRejectsForeignFilesWithoutForce) {
  // Operator-owned content present, no manifest. Refuse to clobber it.
  fs::create_directories(web_path_);
  write_file(web_path_ / "operator-file.txt", "hands off");

  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  EXPECT_EQ(installer.install(local_install_opts(), out), 2);
  EXPECT_NE(out.str().find("--force"), std::string::npos);
  // File survived.
  EXPECT_EQ(read_file(web_path_ / "operator-file.txt"), "hands off");
}

TEST_F(WebInstallerTest, InstallWithForceOverwritesExisting) {
  nsclient::web::web_installer installer(resolver());
  std::ostringstream first;
  ASSERT_EQ(installer.install(local_install_opts(), first), 0) << first.str();

  auto opts = local_install_opts();
  opts.force = true;
  std::ostringstream second;
  EXPECT_EQ(installer.install(opts, second), 0) << second.str();
  EXPECT_TRUE(fs::is_regular_file(web_path_ / "module.json"));
}

TEST_F(WebInstallerTest, ForceInstallStashesFromPathUnderWebPath) {
  // Regression: when --force is set and --from points at a file inside
  // ${web-path}, the wipe used to destroy the source zip before extract_all
  // could re-open it. Stage exactly that scenario.
  fs::create_directories(web_path_);
  const fs::path inside_zip = web_path_ / "saved.zip";
  {
    std::ifstream src(zip_path_.string(), std::ios::binary);
    std::ofstream dst(inside_zip.string(), std::ios::binary);
    dst << src.rdbuf();
  }
  // Drop some other "previous install" content so web_path looks populated.
  write_file(web_path_ / "old-file.txt", "old");

  auto opts = local_install_opts();
  opts.from_path = inside_zip.string();
  opts.force = true;

  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  EXPECT_EQ(installer.install(opts, out), 0) << out.str();

  // Install succeeded — files from the bundle are present.
  EXPECT_TRUE(fs::is_regular_file(web_path_ / "module.json"));
  EXPECT_TRUE(fs::is_regular_file(web_path_ / kManifestFile));
  // The "previous install" was wiped by --force as documented.
  EXPECT_FALSE(fs::exists(web_path_ / "old-file.txt"));
  // The user's original --from path was inside web_path and therefore wiped
  // along with everything else; the install still completed because the
  // installer stashed a copy to ${temp} before the wipe.
  EXPECT_FALSE(fs::exists(inside_zip));
}

TEST_F(WebInstallerTest, InstallRejectsMissingLocalZip) {
  auto opts = local_install_opts();
  opts.from_path = (root_ / "does-not-exist.zip").string();

  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  EXPECT_EQ(installer.install(opts, out), 1);
  EXPECT_FALSE(fs::exists(web_path_ / kManifestFile));
}

TEST_F(WebInstallerTest, InstallVerifiesShaMatchSucceeds) {
  // Sibling .sha256 in the format `sha256sum` emits: "<hex>  <filename>\n".
  const std::string hex = sha256_hex_of_file(zip_path_);
  write_file(zip_path_.string() + ".sha256", hex + "  fixture.zip\n");

  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  ASSERT_EQ(installer.install(local_install_opts(), out), 0) << out.str();
  EXPECT_NE(out.str().find("verified"), std::string::npos);

  const json::value v = json::parse(read_file(web_path_ / kManifestFile));
  EXPECT_EQ(v.as_object().at("sha256").as_string(), hex.c_str());
}

TEST_F(WebInstallerTest, InstallRejectsShaMismatch) {
  // Known-wrong hex (still 64 hex chars so parse_sha256_manifest accepts it).
  const std::string wrong(64, 'a');
  write_file(zip_path_.string() + ".sha256", wrong + "  fixture.zip\n");

  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  EXPECT_EQ(installer.install(local_install_opts(), out), 3);
  EXPECT_NE(out.str().find("SHA-256 mismatch"), std::string::npos);

  // Destination must not be populated on hash failure.
  EXPECT_FALSE(fs::exists(web_path_ / "module.json"));
  EXPECT_FALSE(fs::exists(web_path_ / kManifestFile));
}

TEST_F(WebInstallerTest, InstallDryRunFromPathDoesNotTouchDest) {
  auto opts = local_install_opts();
  opts.dry_run = true;

  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  EXPECT_EQ(installer.install(opts, out), 0);
  EXPECT_NE(out.str().find("Dry-run"), std::string::npos);

  EXPECT_FALSE(fs::exists(web_path_ / "module.json"));
  EXPECT_FALSE(fs::exists(web_path_ / kManifestFile));
}

TEST_F(WebInstallerTest, UninstallRemovesTrackedFilesAndManifest) {
  nsclient::web::web_installer installer(resolver());
  {
    std::ostringstream out;
    ASSERT_EQ(installer.install(local_install_opts(), out), 0) << out.str();
  }

  std::ostringstream out;
  EXPECT_EQ(installer.uninstall(false, out), 0) << out.str();
  EXPECT_FALSE(fs::exists(web_path_ / "module.json"));
  EXPECT_FALSE(fs::exists(web_path_ / "scripts" / "hello.txt"));
  EXPECT_FALSE(fs::exists(web_path_ / kManifestFile));
}

TEST_F(WebInstallerTest, UninstallLeavesOperatorFilesAndTheirDirsAlone) {
  nsclient::web::web_installer installer(resolver());
  {
    std::ostringstream out;
    ASSERT_EQ(installer.install(local_install_opts(), out), 0) << out.str();
  }
  // Operator drops a custom file inside the bundle dir.
  write_file(web_path_ / "scripts" / "user-script.txt", "keep me");

  std::ostringstream out;
  EXPECT_EQ(installer.uninstall(false, out), 0) << out.str();

  // The directory and operator file survive; the bundle's hello.txt is gone.
  EXPECT_TRUE(fs::is_regular_file(web_path_ / "scripts" / "user-script.txt"));
  EXPECT_FALSE(fs::exists(web_path_ / "scripts" / "hello.txt"));
}

TEST_F(WebInstallerTest, UninstallWithoutManifestRequiresForce) {
  fs::create_directories(web_path_);
  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  EXPECT_EQ(installer.uninstall(false, out), 2);
  EXPECT_NE(out.str().find("manifest"), std::string::npos);
}

TEST_F(WebInstallerTest, UninstallWithoutManifestAndForceProceeds) {
  fs::create_directories(web_path_);
  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  EXPECT_EQ(installer.uninstall(true, out), 0);
}

TEST_F(WebInstallerTest, StatusAfterInstallReportsVersion) {
  nsclient::web::web_installer installer(resolver());
  {
    std::ostringstream out;
    ASSERT_EQ(installer.install(local_install_opts(), out), 0) << out.str();
  }
  std::ostringstream out;
  EXPECT_EQ(installer.status(out), 0);
  const std::string s = out.str();
  EXPECT_NE(s.find("installed"), std::string::npos);
  EXPECT_NE(s.find("0.0.0-test"), std::string::npos);
}

TEST_F(WebInstallerTest, StatusReportsNotInstalledWhenManifestMissing) {
  // Directory exists with content but no manifest.
  fs::create_directories(web_path_);
  write_file(web_path_ / "stray.txt", "x");

  nsclient::web::web_installer installer(resolver());
  std::ostringstream out;
  EXPECT_EQ(installer.status(out), 2);
  EXPECT_NE(out.str().find("not installed"), std::string::npos);
}

}  // namespace
