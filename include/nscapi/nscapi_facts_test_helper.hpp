// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_facts_helper.hpp>
#include <string>

// What a producer's unit test asks of a response: one set of it, as JSON, or
// what the producer said went wrong with it. Header-only and gtest-free, so
// every facts test binary shares one reading of the message instead of a
// copy of these three functions.
//
// A set is asserted on as JSON because that is how a reader thinks of the
// document, and a mismatch shows the whole set at once. The field order is
// the producer's own: the core sorts a set when it stores it, and a test
// that asserts on order is asserting on the producer, which is the point.
namespace nscapi {
namespace facts {
namespace testing {

inline const PB::Facts::FactSet *find_set(const PB::Facts::FactsMessage &message, const std::string &id) {
  if (message.payload_size() == 0) return nullptr;
  for (const PB::Facts::FactSet &set : message.payload(0).sets()) {
    if (set.id() == id) return &set;
  }
  return nullptr;
}

// The JSON of one set, or a marker that says why there is none: "(no
// payload)", "(no such set)" when the producer did not mention it, "(no
// facts)" when it mentioned it without a document (an error, or a removal).
inline std::string json_of(const response &out, const std::string &id) {
  const PB::Facts::FactsMessage message = out.to_message();
  if (message.payload_size() == 0) return "(no payload)";
  const PB::Facts::FactSet *set = find_set(message, id);
  if (set == nullptr) return "(no such set)";
  return set->has_facts() ? tree::to_json(set->facts()) : "(no facts)";
}

// What the producer said went wrong with one set, or "" if it said nothing.
inline std::string error_of(const response &out, const std::string &id) {
  const PB::Facts::FactsMessage message = out.to_message();
  const PB::Facts::FactSet *set = find_set(message, id);
  return set == nullptr ? "" : set->error();
}

// When the producer said one set's values were read, or "" if it did not.
inline std::string gathered_of(const response &out, const std::string &id) {
  const PB::Facts::FactsMessage message = out.to_message();
  const PB::Facts::FactSet *set = find_set(message, id);
  return set == nullptr ? "" : set->gathered();
}

}  // namespace testing
}  // namespace facts
}  // namespace nscapi
