// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "filter.hpp"

#include <ctime>
#include <cstdio>
#include <memory>
#include <str/xtos.hpp>

#ifdef CHECKDISK_HAVE_OPENSSL
#include <openssl/evp.h>

#include <array>
#endif

using namespace parsers::where;

constexpr int file_type_file = 1;
constexpr int file_type_dir = 2;
constexpr int file_type_error = -1;

//////////////////////////////////////////////////////////////////////////
// Shared filter_obj helpers (platform-neutral)

std::string file_filter::filter_obj::format_local(long long t) {
  const std::time_t tt = static_cast<std::time_t>(t);
  struct tm tmv;
#ifdef WIN32
  if (localtime_s(&tmv, &tt) != 0) return "";
#else
  if (localtime_r(&tt, &tmv) == nullptr) return "";
#endif
  char buf[64];
  if (strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv) == 0) return "";
  return buf;
}

unsigned long long file_filter::filter_obj::get_type() const { return is_dir_ ? file_type_dir : file_type_file; }
std::string file_filter::filter_obj::get_type_su() const { return is_dir_ ? "dir" : "file"; }

unsigned long file_filter::filter_obj::get_line_count() {
  if (cached_count) return *cached_count;
  unsigned long count = 0;
  FILE *pFile = fopen((path / filename).string().c_str(), "r");
  if (pFile != nullptr) {
    int c;
    do {
      c = fgetc(pFile);
      if (c == '\r') {
        c = fgetc(pFile);
        count++;
      } else if (c == '\n') {
        c = fgetc(pFile);
        count++;
      }
    } while (c != EOF);
    fclose(pFile);
  }
  cached_count = count;
  return count;
}

namespace {
#ifdef CHECKDISK_HAVE_OPENSSL
// Stream `path` through the given OpenSSL digest and return the lower-case hex
// digest, or "" if the file cannot be opened or hashing fails.
std::string hash_file(const std::string &path, const EVP_MD *md) {
  if (md == nullptr) return "";
  FILE *fp = fopen(path.c_str(), "rb");
  if (fp == nullptr) return "";
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (ctx == nullptr) {
    fclose(fp);
    return "";
  }
  std::string result;
  if (EVP_DigestInit_ex(ctx, md, nullptr) == 1) {
    std::array<unsigned char, 65536> buf{};
    size_t n;
    bool ok = true;
    while ((n = fread(buf.data(), 1, buf.size(), fp)) > 0) {
      if (EVP_DigestUpdate(ctx, buf.data(), n) != 1) {
        ok = false;
        break;
      }
    }
    if (ok && ferror(fp) == 0) {
      unsigned char digest[EVP_MAX_MD_SIZE];
      unsigned int len = 0;
      if (EVP_DigestFinal_ex(ctx, digest, &len) == 1) {
        static const char *hex = "0123456789abcdef";
        result.reserve(len * 2);
        for (unsigned int i = 0; i < len; ++i) {
          result.push_back(hex[digest[i] >> 4]);
          result.push_back(hex[digest[i] & 0x0f]);
        }
      }
    }
  }
  EVP_MD_CTX_free(ctx);
  fclose(fp);
  return result;
}
#else
std::string hash_file(const std::string &, const void *) { return ""; }
#endif
}  // namespace

std::string file_filter::filter_obj::get_md5() {
  if (!cached_md5) {
#ifdef CHECKDISK_HAVE_OPENSSL
    cached_md5 = hash_file((path / filename).string(), EVP_md5());
#else
    cached_md5 = std::string();
#endif
  }
  return *cached_md5;
}
std::string file_filter::filter_obj::get_sha1() {
  if (!cached_sha1) {
#ifdef CHECKDISK_HAVE_OPENSSL
    cached_sha1 = hash_file((path / filename).string(), EVP_sha1());
#else
    cached_sha1 = std::string();
#endif
  }
  return *cached_sha1;
}
std::string file_filter::filter_obj::get_sha256() {
  if (!cached_sha256) {
#ifdef CHECKDISK_HAVE_OPENSSL
    cached_sha256 = hash_file((path / filename).string(), EVP_sha256());
#else
    cached_sha256 = std::string();
#endif
  }
  return *cached_sha256;
}
std::string file_filter::filter_obj::get_sha384() {
  if (!cached_sha384) {
#ifdef CHECKDISK_HAVE_OPENSSL
    cached_sha384 = hash_file((path / filename).string(), EVP_sha384());
#else
    cached_sha384 = std::string();
#endif
  }
  return *cached_sha384;
}
std::string file_filter::filter_obj::get_sha512() {
  if (!cached_sha512) {
#ifdef CHECKDISK_HAVE_OPENSSL
    cached_sha512 = hash_file((path / filename).string(), EVP_sha512());
#else
    cached_sha512 = std::string();
#endif
  }
  return *cached_sha512;
}

std::shared_ptr<file_filter::filter_obj> file_filter::filter_obj::create(const boost::filesystem::path &dir, const std::string &name, unsigned long long size,
                                                                         long long creation_time, long long access_time, long long write_time, bool is_dir,
                                                                         long long now) {
  auto o = std::make_shared<filter_obj>();
  o->path = dir;
  o->filename = name;
  o->ullSize = size;
  o->creation_time = creation_time;
  o->access_time = access_time;
  o->write_time = write_time;
  o->is_dir_ = is_dir;
  o->now = now;
  return o;
}

std::shared_ptr<file_filter::filter_obj> file_filter::filter_obj::get_total(unsigned long long now) {
  auto o = std::make_shared<filter_obj>();
  o->filename = "total";
  o->now = o->creation_time = o->access_time = o->write_time = static_cast<long long>(now);
  o->is_total_ = true;
  return o;
}

//////////////////////////////////////////////////////////////////////////
// Field registration (platform-neutral)

namespace {
node_type fun_convert_type(std::shared_ptr<file_filter::filter_obj>, evaluation_context context, const node_type &subject) {
  try {
    const std::string key = subject->get_string_value(context);
    if (key == "file") return factory::create_int(file_type_file);
    if (key == "dir") return factory::create_int(file_type_dir);
    context->error("Failed to convert: " + key + " not file or dir");
  } catch (const std::exception &e) {
    context->error(std::string("Failed to convert type expression: ") + e.what());
  }
  return factory::create_int(file_type_error);
}
}  // namespace

file_filter::filter_obj_handler::filter_obj_handler() {
  constexpr value_type type_custom_type = type_custom_int_2;

  registry_.add_string_var("path", &filter_obj::get_path, "Path of file")
      .add_string_var_w_context("version", &filter_obj::get_version, "Windows exe/dll file version (empty on Unix)")
      .add_string_var("filename", &filter_obj::get_filename, "The name of the file")
      .add_string_var("extension", &filter_obj::get_extension, "The filename extension")
      .add_string_var("file", &filter_obj::get_filename, "The name of the file")
      .add_string_var("name", &filter_obj::get_filename, "The name of the file")
      .add_string_var("access_l", &filter_obj::get_access_sl, "Last access time (local time)")
      .add_string_var("creation_l", &filter_obj::get_creation_sl, "When file was created (local time)")
      .add_string_var("written_l", &filter_obj::get_written_sl, "When file was last written  to (local time)")
      .add_string_var("access_u", &filter_obj::get_access_su, "Last access time (UTC)")
      .add_string_var("creation_u", &filter_obj::get_creation_su, "When file was created (UTC)")
      .add_string_var("written_u", &filter_obj::get_written_su, "When file was last written  to (UTC)")
      .add_string_var("md5_checksum", &filter_obj::get_md5, "MD5 checksum of the file content (hex)")
      .add_string_var("sha1_checksum", &filter_obj::get_sha1, "SHA-1 checksum of the file content (hex)")
      .add_string_var("sha256_checksum", &filter_obj::get_sha256, "SHA-256 checksum of the file content (hex)")
      .add_string_var("sha384_checksum", &filter_obj::get_sha384, "SHA-384 checksum of the file content (hex)")
      .add_string_var("sha512_checksum", &filter_obj::get_sha512, "SHA-512 checksum of the file content (hex)");

  registry_.add_int_var("line_count", &filter_obj::get_line_count, "Number of lines in the file (text files)")
      .add_int_var("access", type_date, &filter_obj::get_access, "Last access time")
      .add_int_var("creation", type_date, &filter_obj::get_creation, "When file was created")
      .add_int_var("written", type_date, &filter_obj::get_write, "When file was last written to")
      .add_int_var("write", type_date, &filter_obj::get_write, "Alias for written")
      .add_int_var("age", type_int, &filter_obj::get_age, "Seconds since file was last written")
      .add_int_var("type", type_custom_type, &filter_obj::get_type, "Type of item (file or dir)");

  // clang-format off
  registry_.add_int_legacy()
    ("size", type_size, [] (auto obj, auto context) { return obj->get_size(); }, "File size").add_scaled_byte(std::string(""), " size")
    ("total", type_bool, [] (auto obj, auto context) { return obj->is_total(); }, "True if this is the total object").no_perf();
    ;
  // clang-format on

  registry_.add_converter(type_custom_type, &fun_convert_type);

  registry_.add_human_string("access", &filter_obj::get_access_su, "")
      .add_human_string("creation", &filter_obj::get_creation_su, "")
      .add_human_string("written", &filter_obj::get_written_su, "")
      .add_human_string("type", &filter_obj::get_type_su, "");
}
