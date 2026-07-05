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

#include <ctime>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

#include <gtest/gtest.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/helpers.hpp>

#include "cert_source.hpp"

// Normally provided by NSC_WRAP_DLL() in the auto-generated module.cpp; in the
// test binary there is no generated module, so define the singleton here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

namespace fs = boost::filesystem;

// Generate a self-signed certificate valid between now+not_before_days and
// now+not_after_days, write it to `path` (PEM unless der=true), return success.
// `bits` sizes the RSA key; `weak_sig` signs with SHA-1 instead of SHA-256.
bool write_test_cert(const std::string &path, const std::string &cn, long not_before_days, long not_after_days, bool der = false, int bits = 2048,
                     bool weak_sig = false) {
  EVP_PKEY *pkey = nullptr;
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
  if (pctx == nullptr) return false;
  EVP_PKEY_keygen_init(pctx);
  EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, bits);
  EVP_PKEY_keygen(pctx, &pkey);
  EVP_PKEY_CTX_free(pctx);
  if (pkey == nullptr) return false;

  X509 *x = X509_new();
  X509_set_version(x, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(x), 4711);
  X509_gmtime_adj(X509_getm_notBefore(x), static_cast<long>(not_before_days) * 24 * 3600);
  X509_gmtime_adj(X509_getm_notAfter(x), static_cast<long>(not_after_days) * 24 * 3600);
  X509_set_pubkey(x, pkey);
  X509_NAME *name = X509_get_subject_name(x);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>(cn.c_str()), -1, -1, 0);
  X509_set_issuer_name(x, name);
  X509_sign(x, pkey, weak_sig ? EVP_sha1() : EVP_sha256());

  BIO *bio = BIO_new_file(path.c_str(), "wb");
  bool ok = false;
  if (bio != nullptr) {
    ok = der ? (i2d_X509_bio(bio, x) == 1) : (PEM_write_bio_X509(bio, x) == 1);
    BIO_free(bio);
  }
  X509_free(x);
  EVP_PKEY_free(pkey);
  return ok;
}

class CertSourceTest : public ::testing::Test {
 protected:
  fs::path dir;
  void SetUp() override {
    dir = fs::temp_directory_path() / fs::unique_path("nscp-cert-%%%%%%%%");
    fs::create_directories(dir);
    parsers::where::constants::reset();
  }
  void TearDown() override {
    boost::system::error_code ec;
    fs::remove_all(dir, ec);
  }
  std::string file(const std::string &name) { return (dir / name).string(); }
};

TEST_F(CertSourceTest, ParsesAValidPemCertificate) {
  ASSERT_TRUE(write_test_cert(file("valid.pem"), "valid.example.com", -1, 365));
  std::vector<cert_filter::filter_obj_ptr> certs;
  std::vector<std::string> errors;
  cert_source::load_files({file("valid.pem")}, false, "", "", certs, errors);

  ASSERT_EQ(certs.size(), 1u) << (errors.empty() ? "" : errors[0]);
  const auto &c = certs[0];
  EXPECT_NE(c->get_subject().find("valid.example.com"), std::string::npos);
  EXPECT_EQ(c->get_store(), "file");
  EXPECT_EQ(c->get_expired(), 0);
  EXPECT_EQ(c->get_not_yet_valid(), 0);
  EXPECT_GT(c->get_expires_in_days(), 360);
  EXPECT_LE(c->get_expires_in_days(), 365);
  EXPECT_FALSE(c->get_thumbprint().empty());
}

TEST_F(CertSourceTest, FlagsAnExpiredCertificate) {
  ASSERT_TRUE(write_test_cert(file("old.pem"), "old.example.com", -30, -10));
  std::vector<cert_filter::filter_obj_ptr> certs;
  std::vector<std::string> errors;
  cert_source::load_files({file("old.pem")}, false, "", "", certs, errors);

  ASSERT_EQ(certs.size(), 1u);
  EXPECT_EQ(certs[0]->get_expired(), 1);
  EXPECT_LT(certs[0]->get_expires_in_days(), 0);
}

TEST_F(CertSourceTest, FlagsANotYetValidCertificate) {
  ASSERT_TRUE(write_test_cert(file("future.pem"), "future.example.com", 10, 400));
  std::vector<cert_filter::filter_obj_ptr> certs;
  std::vector<std::string> errors;
  cert_source::load_files({file("future.pem")}, false, "", "", certs, errors);

  ASSERT_EQ(certs.size(), 1u);
  EXPECT_EQ(certs[0]->get_not_yet_valid(), 1);
}

TEST_F(CertSourceTest, ParsesDerEncoding) {
  ASSERT_TRUE(write_test_cert(file("valid.der"), "der.example.com", -1, 200, /*der=*/true));
  std::vector<cert_filter::filter_obj_ptr> certs;
  std::vector<std::string> errors;
  cert_source::load_files({file("valid.der")}, false, "", "", certs, errors);

  ASSERT_EQ(certs.size(), 1u) << (errors.empty() ? "" : errors[0]);
  EXPECT_NE(certs[0]->get_subject().find("der.example.com"), std::string::npos);
}

TEST_F(CertSourceTest, ScansADirectoryAndParsesBundles) {
  ASSERT_TRUE(write_test_cert(file("a.pem"), "a.example.com", -1, 100));
  ASSERT_TRUE(write_test_cert(file("b.pem"), "b.example.com", -1, 200));
  std::vector<cert_filter::filter_obj_ptr> certs;
  std::vector<std::string> errors;
  cert_source::load_files({dir.string()}, false, "", "", certs, errors);

  EXPECT_EQ(certs.size(), 2u);
}

TEST_F(CertSourceTest, ReportsMissingFile) {
  std::vector<cert_filter::filter_obj_ptr> certs;
  std::vector<std::string> errors;
  cert_source::load_files({file("does-not-exist.pem")}, false, "", "", certs, errors);

  EXPECT_TRUE(certs.empty());
  EXPECT_FALSE(errors.empty());
}

TEST_F(CertSourceTest, PopulatesSecurityFieldsForAStrongCert) {
  ASSERT_TRUE(write_test_cert(file("strong.pem"), "strong.example.com", -1, 365));
  std::vector<cert_filter::filter_obj_ptr> certs;
  std::vector<std::string> errors;
  cert_source::load_files({file("strong.pem")}, false, "", "", certs, errors);

  ASSERT_EQ(certs.size(), 1u);
  const auto &c = certs[0];
  EXPECT_EQ(c->get_self_signed(), 1);
  EXPECT_EQ(c->get_key_type(), "RSA");
  EXPECT_EQ(c->get_key_size(), 2048);
  EXPECT_EQ(c->get_weak_key(), 0);
  EXPECT_EQ(c->get_weak_signature(), 0);
  EXPECT_NE(c->get_signature_algorithm().find("sha256"), std::string::npos);
}

TEST_F(CertSourceTest, FlagsWeakKeyAndWeakSignature) {
  ASSERT_TRUE(write_test_cert(file("weak.pem"), "weak.example.com", -1, 365, /*der=*/false, /*bits=*/1024, /*weak_sig=*/true));
  std::vector<cert_filter::filter_obj_ptr> certs;
  std::vector<std::string> errors;
  cert_source::load_files({file("weak.pem")}, false, "", "", certs, errors);

  ASSERT_EQ(certs.size(), 1u);
  EXPECT_EQ(certs[0]->get_key_size(), 1024);
  EXPECT_EQ(certs[0]->get_weak_key(), 1);
  EXPECT_EQ(certs[0]->get_weak_signature(), 1);
  EXPECT_NE(certs[0]->get_signature_algorithm().find("sha1"), std::string::npos);
}

TEST_F(CertSourceTest, TrustDependsOnTheCaBundle) {
  ASSERT_TRUE(write_test_cert(file("root.pem"), "root.example.com", -1, 365));

  // Not in the system trust store -> untrusted.
  std::vector<cert_filter::filter_obj_ptr> untrusted;
  std::vector<std::string> e1;
  cert_source::load_files({file("root.pem")}, false, "", "", untrusted, e1);
  ASSERT_EQ(untrusted.size(), 1u);
  EXPECT_EQ(untrusted[0]->get_trusted(), 0);

  // Passed as its own CA bundle -> the self-signed root verifies against itself.
  std::vector<cert_filter::filter_obj_ptr> trusted;
  std::vector<std::string> e2;
  cert_source::load_files({file("root.pem")}, false, file("root.pem"), "", trusted, e2);
  ASSERT_EQ(trusted.size(), 1u);
  EXPECT_EQ(trusted[0]->get_trusted(), 1);
}

}  // namespace
