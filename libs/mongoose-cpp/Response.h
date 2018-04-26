#ifndef _MONGOOSE_RESPONSE_H
#define _MONGOOSE_RESPONSE_H

#include "dll_defines.hpp"

#include <map>
#include <sstream>
#include <iostream>

#define HTTP_OK 200
#define HTTP_BAD_REQUEST 400
#define HTTP_NOT_FOUND 404
#define HTTP_FORBIDDEN 403
#define HTTP_SERVER_ERROR 500
#define HTTP_SERVICE_UNAVALIBLE 503


#define HTTP_HDR_AUTH "Authorization"
#define HTTP_HDR_AUTH_LC "authorization"
/**
 * A response to a request
 */
namespace Mongoose
{
    class NSCAPI_EXPORT Response 
    {
        public:
            Response();
            virtual ~Response();

            /**
             * Test if the given header is present
             *
             * @param string the header key
             *
             * @return bool true if the header is set
             */
            virtual bool hasHeader(std::string key);

            /**
             * Sets the header
             *
             * @param key the header key
             *
             * @param value the header value
             */
            virtual void setHeader(std::string key, std::string value);

            /**
             * Get the data of the response, this will contain headers and
             * body
             *
             * @return string the response data
             */
            virtual std::string getData();

            /**
             * Gets the response body
             *
             * @return string the response body
             */
            virtual std::string getBody()=0;

            /**
             * Sets the cookie, note that you can only define one cookie by request
             * for now
             *
             * @param string the key of the cookie
             * @param string value the cookie value
             */
            virtual void setCookie(std::string key, std::string value);

            /**
             * Sets the response code
             */
            virtual void setCode(int code);

			/**
			* Get a cookie from the cookie list.
			* @param string the key of the cookie
			*/
			virtual std::string getCookie(std::string key) const ;

			typedef std::map<std::string, std::string> header_type;

			virtual int get_response_code() const = 0;

			header_type& get_headers() {
				return headers;
			}
        protected:
            int code;
			header_type headers;
			header_type cookies;
	};
}

#endif
