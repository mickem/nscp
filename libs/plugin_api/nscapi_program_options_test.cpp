// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_program_options.hpp>

namespace po = boost::program_options;

TEST(nscapiProgramOptionsTest, extract_default_value_plain) {
  EXPECT_EQ("", nscapi::program_options::extract_default_value("arg"));
}

TEST(nscapiProgramOptionsTest, extract_default_value_default) {
  EXPECT_EQ("10", nscapi::program_options::extract_default_value("arg (=10)"));
  EXPECT_EQ("%(status): Nothing found...", nscapi::program_options::extract_default_value("arg (=%(status): Nothing found...)"));
}

TEST(nscapiProgramOptionsTest, extract_default_value_implicit) {
  EXPECT_EQ("true", nscapi::program_options::extract_default_value("[=arg(=true)]"));
}

TEST(nscapiProgramOptionsTest, extract_default_value_implicit_and_default) {
  // The bool convention implicit_value(true)->default_value(false) formats as
  // "[=arg(=1)] (=0)"; the default is the part to surface.
  EXPECT_EQ("0", nscapi::program_options::extract_default_value("[=arg(=1)] (=0)"));
  EXPECT_EQ("1", nscapi::program_options::extract_default_value("[=arg(=1)] (=1)"));
}

namespace {
po::options_description make_desc(bool &debug, bool &flip, std::string &mode) {
  po::options_description desc("test");
  // clang-format off
  desc.add_options()
    ("debug", po::value<bool>(&debug)->implicit_value(true)->default_value(false), "Show debugging information")
    ("flip", po::value<bool>(&flip)->implicit_value(true)->default_value(true), "A default-enabled boolean")
    ("mode", po::value<std::string>(&mode)->default_value("plain"), "A string option")
    ;
  // clang-format on
  return desc;
}
}  // namespace

TEST(nscapiProgramOptionsTest, help_renders_bool_defaults_as_true_false) {
  bool debug = false, flip = true;
  std::string mode;
  const po::options_description desc = make_desc(debug, flip, mode);
  const std::string help = nscapi::program_options::help(desc);
  EXPECT_NE(std::string::npos, help.find("Default value: debug=false")) << help;
  EXPECT_NE(std::string::npos, help.find("Default value: flip=true")) << help;
  EXPECT_NE(std::string::npos, help.find("Default value: mode=plain")) << help;
}

TEST(nscapiProgramOptionsTest, help_pb_renders_bool_defaults_as_true_false) {
  bool debug = false, flip = true;
  std::string mode;
  const po::options_description desc = make_desc(debug, flip, mode);
  ::PB::Registry::ParameterDetails details;
  ASSERT_TRUE(details.ParseFromString(nscapi::program_options::help_pb(desc)));
  std::map<std::string, std::string> defaults;
  for (const auto &p : details.parameter()) {
    defaults[p.name()] = p.default_value();
  }
  EXPECT_EQ("false", defaults["debug"]);
  EXPECT_EQ("true", defaults["flip"]);
  EXPECT_EQ("plain", defaults["mode"]);
}
