#pragma once

#include <StreamResponse.h>

#include <boost/json.hpp>

#include <string>
#include "../CommandClient/CommandClient.h"

namespace helpers {
namespace json = boost::json;

inline void parse_result(const ::google::protobuf::RepeatedPtrField<PB::Registry::RegistryResponseMessage::Response> &payload,
                         Mongoose::StreamResponse &response, const std::string &task) {
  for (const PB::Registry::RegistryResponseMessage::Response &r : payload) {
    if (r.has_result() && r.result().code() == PB::Common::Result_StatusCodeType_STATUS_ERROR) {
      response.get_headers()["Content-Type"] = "text/plain";
      response.setCodeServerError("Failed to " + task);
      return;
    }
    if (r.has_result() && r.result().code() == PB::Common::Result_StatusCodeType_STATUS_WARNING) {
      response.get_headers()["Content-Type"] = "text/plain";
      response.setCodeOk();
      response.append("Warning in " + task);
      return;
    }
    if (r.has_result() && r.result().code() == PB::Common::Result_StatusCodeType_STATUS_OK) {
      response.get_headers()["Content-Type"] = "text/plain";
      response.setCodeOk();
      response.append("Success " + task);
      return;
    }
  }
  response.setCodeServerError("Failed to " + task);
}

inline void parse_result_v2(const ::google::protobuf::RepeatedPtrField<PB::Registry::RegistryResponseMessage::Response> &payload,
                            Mongoose::StreamResponse &response, const std::string &task) {
  json::object node;
  node["result"] = "unknown";
  node["message"] = "Failed to " + task;
  for (const PB::Registry::RegistryResponseMessage::Response &r : payload) {
    // TODO: FIXME: If more then one return a list.
    if (r.has_result() && r.result().code() == PB::Common::Result::STATUS_ERROR) {
      response.setCode(HTTP_SERVER_ERROR, REASON_SERVER_ERROR);
      node["result"] = r.result().code();
      node["message"] = "Failed to " + task;
    } else if (r.has_result() && r.result().code() == PB::Common::Result::STATUS_WARNING) {
      response.setCodeOk();
      node["result"] = r.result().code();
      node["message"] = "Warning in " + task;
    } else if (r.has_result() && r.result().code() == PB::Common::Result::STATUS_OK) {
      response.setCodeOk();
      node["result"] = r.result().code();
      node["message"] = "Success " + task;
    } else {
      response.setCode(HTTP_SERVER_ERROR, REASON_SERVER_ERROR);
      node["result"] = "unknown";
      node["message"] = "Failed to " + task;
    }
  }
  response.append(json::serialize(node));
}

}  // namespace helpers
