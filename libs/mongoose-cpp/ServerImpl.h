#pragma once

#include "Server.h"
#include "Request.h"
#include "Response.h"
#include "Controller.h"

#include "ext/mongoose.h"

#include <has-threads.hpp>
#include <threads/queue.hpp>

#include "dll_defines.hpp"

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/atomic/atomic.hpp>

#include <vector>

/**
 * Wrapper for the Mongoose server
 */
namespace Mongoose
{

      class NSCAPI_EXPORT ServerImpl : public Server
    {
        public:
            /**
             * Constructs the server
             *
             * @param int the number of the port to listen to
             * @param string documentRoot the root that should be used for static files
             */
			ServerImpl(WebLoggerPtr logger);
            virtual ~ServerImpl();


            /**
             * Runs the Mongoose server
             */
            void start(std::string bind);

            /**
             * Stops the Mongoose server
             */
            void stop();

            /**
             * Register a new controller on the server
             *
             * @param Controller* a pointer to a controller
             */
            void registerController(Controller *);

            /**
             * Main event handler (called by mongoose when something happens)
             *
			 * @param struct mg_connection* the mongoose connection
			 * @param int ev event type
			 * @param void* ev_data event data
			 */
			static void event_handler(struct mg_connection *connection, int ev, void *ev_data, void *fn_data);

			void onHttpRequest(struct mg_connection *connection, struct mg_http_message *message);


            /**
             * Process the request by controllers
             *
             * @param Request the request
             *
             * @return Response the response if one of the controllers can handle it,
             *         NULL else
             */
            Response *handleRequest(Request &request);

			/**
			* Setup the mongoose ssl options section
			*
			* @param certificate the name of the certificate to use
			*/
#if MG_ENABLE_OPENSSL
			void initTls(struct mg_connection *connection);
#endif
			void setSsl(const char *certificate, const char *new_chipers);

			/**
             * Polls the server
             */
            void poll();

            /**
             * Does the server handles url?
             */
            bool handles(std::string method, std::string url);


			void thread_proc();



		protected:
      WebLoggerPtr logger_;
      std::string certificate;
      std::string ciphers;
			struct mg_mgr mgr;
            struct mg_connection *server_connection;

            std::vector<Controller *> controllers;

			boost::atomic<bool> stop_thread_;
			boost::timed_mutex mutex_;
			boost::shared_ptr<boost::thread> thread_;

	};
}
