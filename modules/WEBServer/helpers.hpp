#pragma once

#include <StreamResponse.h>
#include <nscapi/nscapi_protobuf.hpp>

#include <json_spirit.h>

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

  inline void parse_result_v2(const ::google::protobuf::RepeatedPtrField<Plugin::RegistryResponseMessage::Response> &payload, Mongoose::StreamResponse &response, std::string task) {
	  json_spirit::Object node;
	  node["result"] = "unknown";
	  node["message"] = "Failed to " + task;
	  BOOST_FOREACH(const ::Plugin::RegistryResponseMessage::Response &r, payload) {
		  // TODO: FIXME: If more then one return a list.
		  if (r.has_result() && r.result().code() == ::Plugin::Common_Result_StatusCodeType_STATUS_ERROR) {
			  response.setCode(HTTP_SERVER_ERROR);
			  node["result"] = r.result().code();
			  node["message"] = "Failed to " + task;
		  } else if (r.has_result() && r.result().code() == ::Plugin::Common_Result_StatusCodeType_STATUS_WARNING) {
			  response.setCode(HTTP_OK);
			  node["result"] = r.result().code();
			  node["message"] = "Warning in " + task;
		  } else if (r.has_result() && r.result().code() == ::Plugin::Common_Result_StatusCodeType_STATUS_OK) {
			  response.setCode(HTTP_OK);
			  node["result"] = r.result().code();
			  node["message"] = "Success " + task;
		  } else {
			  response.setCode(HTTP_SERVER_ERROR);
			  node["result"] = "unknown";
			  node["message"] = "Failed to " + task;
		  }
	  }
	  response.append(json_spirit::write(node));
  }

}
