#ifndef _MONGOOSE_REQUEST_H
#define _MONGOOSE_REQUEST_H

#include "Response.h"

#include "dll_defines.hpp"

#include <iostream>
#include <sstream>
#include <vector>


using namespace std;

/**
 * Request is a wrapper for the clients requests
 */
namespace Mongoose {
    class NSCAPI_EXPORT Request {
	public:
		typedef pair<string, string> arg_entry;
		typedef std::vector<arg_entry> arg_vector;
		typedef map<string, string> headers_type;

        public:
            Request(const std::string ip, bool is_ssl, std::string method, std::string url, std::string query, headers_type headers, std::string data);

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
			bool get_bool(string key, bool fallback = false);
			template <class T>
			T get_number(string key, T fallback = 0) {
				return str::stox<T>(get("page", str::xtos(fallback)), fallback);
			}

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


			arg_vector getVariablesVector();

			std::string readHeader(const std::string key);
            //bool readVariable(const struct mg_str data, string key, string &output);

			std::string get_host();
			bool is_ssl() {
				return  is_ssl_;
			}
        protected:
			bool is_ssl_;
			string method;
			string url;
			string query;
			string data;
			std::string ip;
			headers_type headers;
    };
}

#endif
