#ifndef _MONGOOSE_REQUEST_H
#define _MONGOOSE_REQUEST_H

#include "Response.h"

#include "ext/mongoose.h"

#include "dll_defines.hpp"

#include <iostream>
#include <sstream>
#include <vector>


using namespace std;

/**
 * Request is a wrapper for the clients requests
 */
namespace Mongoose
{
    class NSCAPI_EXPORT Request
    {
        public:
            Request(struct mg_connection *connection, struct http_message *message, bool is_ssl);

            /**
             * Sends a given response to the client
             *
             * @param Response a response for this request
             */
            void writeResponse(Response *response);

            /**
             * Check if the variable given by key is present in GET or POST data
             *
             * @param string the name of the variable
             *
             * @return bool true if the param is present, false else
             */
            bool hasVariable(string key);

            /**
             * Get the value for a certain variable
             *
             * @param string the name of the variable
             * @param string the fallback value if the variable doesn't exists
             *
             * @return string the value of the variable if it exists, fallback else
             */
            string get(string key, string fallback = "");

            /**
             * Try to get the cookie value
             *
             * @param string the name of the cookie
             * @param string the fallback value
             *
             * @retun the value of the cookie if it exists, fallback else
             */
            string getCookie(string key, string fallback = "");

            /**
             * Handle uploads to the target directory
             *
             * @param string the target directory
             * @param path the posted file path
             */
            void handleUploads();

            string getUrl();
            string getMethod();
            string getData();
            string getRemoteIp();


			typedef pair<string,string> arg_entry;
			typedef std::vector<arg_entry> arg_vector;
			typedef map<string,string> arg_map;
			arg_vector getVariablesVector();

			std::string readHeader(const std::string key);
            bool readVariable(const struct mg_str data, string key, string &output);

			bool is_ssl() {
				return  is_ssl_;
			}
        protected:
            string method;
			string url;
			bool is_ssl_;
			string query;
			string data;
			std::string ip;
			std::map<std::string, std::string> headers;
    };
}

#endif
