// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#include <gtest/gtest.h>

#include "check_radius_protocol.hpp"

using namespace check_net::radius;
#ifdef USE_SSL
namespace {
packet hex(const std::string &value) {
  packet out;
  for (std::size_t i = 0; i < value.size(); i += 2) out.push_back(static_cast<unsigned char>(std::stoul(value.substr(i, 2), nullptr, 16)));
  return out;
}
const std::string secret = "radius-test-secret";
const packet request_vector =
    hex("01070049000102030405060708090a0b0c0d0e0f01067465737402122723fb568137191bd36a423cc25a01e6200b6e7363702d7465737450121cb25edb347afa90068f544be0ef6a10");
const packet response_vector = hex("020700265042a9af8ab41237f9698f6869cf6243501276a47dc5889d7d9df14b53b37bee76d2");
}  // namespace
TEST(RadiusProtocol, Rfc2202HmacVector) {
  const std::string input = "what do ya want for nothing?";
  const auto value = hmac(packet(input.begin(), input.end()), "Jefe");
  EXPECT_EQ(packet(value.begin(), value.end()), hex("750c783e6ab0b503eaa86e310a5db738"));
}
TEST(RadiusProtocol, PapRequestMatchesIndependentNodeCryptoVector) {
  digest authenticator{};
  for (unsigned char i = 0; i < 16; ++i) authenticator[i] = i;
  EXPECT_EQ(request(7, authenticator, secret, "test", "password", false, "nscp-test"), request_vector);
}
TEST(RadiusProtocol, AuthenticatedResponseAndPadding) {
  EXPECT_EQ(validate(response_vector, request_vector, secret), "");
  auto padded = response_vector;
  padded.push_back(255);
  EXPECT_EQ(validate(padded, request_vector, secret), "");
}
TEST(RadiusProtocol, RejectsWrongSecretAndReplay) {
  EXPECT_EQ(validate(response_vector, request_vector, "wrong"), "invalid_authenticator");
  auto request = request_vector;
  request[4] ^= 1;
  EXPECT_EQ(validate(response_vector, request, secret), "invalid_authenticator");
  request[1] ^= 1;
  EXPECT_EQ(validate(response_vector, request, secret), "wrong_identifier");
}
TEST(RadiusProtocol, RejectsTruncationAndMalformedAttributes) {
  for (std::size_t n = 0; n < response_vector.size(); ++n)
    EXPECT_FALSE(validate(packet(response_vector.begin(), response_vector.begin() + n), request_vector, secret).empty());
  auto response = response_vector;
  response[21] = 0;
  EXPECT_EQ(validate(response, request_vector, secret), "invalid_attribute");
  response[21] = 17;
  EXPECT_EQ(validate(response, request_vector, secret), "invalid_message_authenticator");
}
TEST(RadiusProtocol, RequiresMessageAuthenticatorEvenWithAValidResponseAuthenticator) {
  auto response = response_vector;
  response.resize(20);
  response[3] = 20;
  std::copy_n(request_vector.begin() + 4, 16, response.begin() + 4);
  auto input = response;
  input.insert(input.end(), secret.begin(), secret.end());
  const auto auth = md5(input);
  std::copy(auth.begin(), auth.end(), response.begin() + 4);
  EXPECT_EQ(validate(response, request_vector, secret), "missing_message_authenticator");
}
TEST(RadiusProtocol, VerifiesHmacIndependentlyOfResponseAuthenticator) {
  auto response = response_vector;
  response.back() ^= 1;
  std::copy_n(request_vector.begin() + 4, 16, response.begin() + 4);
  auto input = response;
  input.insert(input.end(), secret.begin(), secret.end());
  const auto auth = md5(input);
  std::copy(auth.begin(), auth.end(), response.begin() + 4);
  EXPECT_EQ(validate(response, request_vector, secret), "invalid_message_authenticator");
}
TEST(RadiusProtocol, EnforcesPapAndAttributeLimits) {
  EXPECT_THROW(request(0, {}, secret, "test", std::string(129, 'x'), false, "nscp"), std::runtime_error);
  EXPECT_THROW(request(0, {}, secret, std::string(254, 'x'), "pw", false, "nscp"), std::runtime_error);
  EXPECT_NO_THROW(request(0, {}, secret, "test", std::string(128, 'x'), false, "nscp"));
}
TEST(RadiusProtocol, StatusRequestContainsNoCredentials) {
  const auto p = request(1, {}, secret, "", "", true, "nscp");
  EXPECT_EQ(p[0], 12);
  for (std::size_t i = 20; i < p.size(); i += p[i + 1]) EXPECT_TRUE(p[i] == 32 || p[i] == 80);
}
#endif
TEST(RadiusProtocol, ModesDoNotConfuseResponsivenessWithAuthentication) {
  EXPECT_TRUE(expected("auth", 2));
  EXPECT_FALSE(expected("auth", 3));
  EXPECT_FALSE(expected("auth", 11));
  EXPECT_TRUE(expected("reject", 3));
  EXPECT_FALSE(expected("reject", 2));
  EXPECT_TRUE(expected("status", 5));
  EXPECT_FALSE(expected("status", 11));
}
