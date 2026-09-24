// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/date_time/posix_time/posix_time.hpp>
#include <nscapi/nscapi_facts_helper.hpp>

namespace nscapi {
namespace facts {

bool is_valid_key(const std::string &key) {
  if (key.empty() || key.size() > 64) return false;
  if (key[0] < 'a' || key[0] > 'z') return false;
  for (const char c : key) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    if (!ok) return false;
  }
  return true;
}

std::string format_time(const std::time_t when) {
  const boost::posix_time::ptime moment = boost::posix_time::from_time_t(when);
  // to_iso_extended_string renders fractional seconds when the ptime has any;
  // from_time_t never does, so this is always "YYYY-MM-DDTHH:MM:SS".
  return boost::posix_time::to_iso_extended_string(moment) + "Z";
}

std::string format_date(const std::time_t when) {
  const boost::gregorian::date day = boost::posix_time::from_time_t(when).date();
  return boost::gregorian::to_iso_extended_string(day);
}

PB::Facts::Value parse_document(const std::string &envelope) {
  PB::Facts::Value empty;
  empty.mutable_object_value();
  PB::Facts::FactsResponseMessage message;
  // The envelope comes from the core, so a parse failure is a core that does
  // not know the call (it answers with nothing) or a truncated buffer. Either
  // way the consumer gets an empty document rather than an error it cannot
  // act on.
  if (!message.ParseFromString(envelope)) return empty;
  if (message.payload_size() == 0) return empty;
  if (!message.payload(0).has_facts()) return empty;
  return message.payload(0).facts();
}

request::request(const std::string &buffer) {
  PB::Facts::FactsQueryMessage message;
  // Nothing to recover from a bad parse: the reason is a hint, and a producer
  // that cannot read it simply collects as it would on a scheduled round.
  if (!message.ParseFromString(buffer)) return;
  if (message.payload_size() == 0) return;
  reason_ = message.payload(0).reason();
}

PB::Facts::FactsMessage response::to_message() const {
  PB::Facts::FactsMessage message;
  PB::Facts::FactsMessage::Response *payload = message.add_payload();

  // A failed round says so through the result and carries no sets at all: the
  // core keeps what it has rather than reading silence as "stopped
  // producing".
  if (!failure_.empty()) {
    payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);
    payload->mutable_result()->set_message(failure_);
    return message;
  }
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);

  for (const std::string &name : order_) {
    const std::map<std::string, detail::node_ptr>::const_iterator it = sets_.find(name);
    if (it == sets_.end()) continue;
    PB::Facts::FactSet *set = payload->add_sets();
    set->set_id(name);
    it->second->build_object(set->mutable_facts());
    const std::map<std::string, std::string>::const_iterator problem = problems_->find(name);
    if (problem != problems_->end()) set->set_error(problem->second);
    const std::map<std::string, std::string>::const_iterator when = gathered_->find(name);
    if (when != gathered_->end()) set->set_gathered(when->second);
  }
  // `removed` drops the set, as opposed to not mentioning it, which leaves
  // what the core already has.
  for (const std::string &name : removed_) {
    PB::Facts::FactSet *set = payload->add_sets();
    set->set_id(name);
    set->set_removed(true);
  }
  // An error against a set the producer did not build at all - it could not
  // collect it this round. It still rides as a FactSet so the core learns the
  // module produces it and keeps the value it has.
  for (const std::pair<const std::string, std::string> &problem : *problems_) {
    if (sets_.find(problem.first) != sets_.end()) continue;
    PB::Facts::FactSet *set = payload->add_sets();
    set->set_id(problem.first);
    set->set_error(problem.second);
    // The set it could not collect still has a last-known age, and that is
    // exactly what a reader wants when a set goes stale.
    const std::map<std::string, std::string>::const_iterator when = gathered_->find(problem.first);
    if (when != gathered_->end()) set->set_gathered(when->second);
  }
  return message;
}

std::string response::serialize() const { return to_message().SerializeAsString(); }

}  // namespace facts
}  // namespace nscapi
