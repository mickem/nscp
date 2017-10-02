#pragma once

#include <StreamResponse.h>
#include <nscapi/nscapi_protobuf.hpp>

#include <boost/foreach.hpp>

#include <string>

namespace helpers {
  inline void parse_result(const ::google::protobuf::RepeatedPtrField<Plugin::RegistryResponseMessage::Response> &payload, Mongoose::StreamResponse &response, std::string task) {
    BOOST_FOREACH(const ::Plugin::RegistryResponseMessage::Response &r, payload) {
      if (r.has_result() && r.result().code() == ::Plugin::Common_Result_StatusCodeType_STATUS_ERROR) {
        response.setCode(HTTP_SERVER_ERROR);
        response.append("Failed to " + task);
        return;
      } else if (r.has_result() && r.result().code() == ::Plugin::Common_Result_StatusCodeType_STATUS_WARNING) {
        response.setCode(HTTP_OK);
        response.append("Warning in " + task);
        return;
      } else if (r.has_result() && r.result().code() == ::Plugin::Common_Result_StatusCodeType_STATUS_OK) {
        response.setCode(HTTP_OK);
        response.append("Success " + task);
        return;
      }
    }
    response.setCode(HTTP_SERVER_ERROR);
    response.append("Failed to " + task);
  }

}
