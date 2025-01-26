#pragma once

#include "Request.h"
#include "Response.h"
#include "Controller.h"

#include "dll_defines.hpp"

#include <boost/shared_ptr.hpp>
#include <string>

/**
 * Wrapper for the Mongoose server
 */
namespace Mongoose {


  class NSCAPI_EXPORT WebLogger {
  public:
    virtual void log_error(const std::string &message) = 0;
    virtual void log_info(const std::string &message) = 0;
    virtual void log_debug(const std::string &message) = 0;
  };

  typedef boost::shared_ptr<WebLogger> WebLoggerPtr;


	class NSCAPI_EXPORT Server {

	public:
		static Server* make_server(WebLoggerPtr logger);

		virtual ~Server() {}


		/**
		 * Runs the Mongoose server
		 */
		virtual void start(std::string bind) = 0;

		/**
		 * Stops the Mongoose server
		 */
		virtual void stop() = 0;

		/**
		 * Register a new controller on the server
		 *
		 * @param Controller* a pointer to a controller
		 */
		virtual void registerController(Controller *) = 0;



		/**
		* Setup the mongoose ssl options section
		*
		* @param certificate the name of the certificate to use
		*/
		virtual void setSsl(const char *certificate) = 0;

		/**
		 * Does the server handles url?
		 */
	};
}
