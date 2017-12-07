#pragma once

#include "Request.h"
#include "Response.h"
#include "Controller.h"

#include "dll_defines.hpp"

#include <string>

/**
 * Wrapper for the Mongoose server
 */
namespace Mongoose {

	class NSCAPI_EXPORT Server {

	public:
		static Server* make_server(std::string port = "80");

		virtual ~Server() {}


		/**
		 * Runs the Mongoose server
		 */
		virtual void start(int thread_count) = 0;

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
