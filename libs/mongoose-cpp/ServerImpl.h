#pragma once

#include "Server.h"
#include "Request.h"
#include "Response.h"
#include "Controller.h"

#include "ext/mongoose.h"

#include <has-threads.hpp>
#include <threads/queue.hpp>

#include "dll_defines.hpp"

#include <boost/thread.hpp>

#include <vector>
#include <iostream>

/**
 * Wrapper for the Mongoose server
 */
namespace Mongoose
{

	typedef unsigned long job_id;


	class ServerImpl;
	class request_job {
		ServerImpl *server;
		Controller *controller;
		Request request;
		boost::posix_time::ptime time;
		int id;

	public:
		request_job(ServerImpl *server, Controller *controller, Request request, boost::posix_time::ptime time, int id)
			: server(server)
			, controller(controller)
			, request(request)
			, time(time)
			, id(id) {}
		bool is_late(boost::posix_time::ptime now);
		void run();
		void toLate();
	};


    class NSCAPI_EXPORT ServerImpl : public Server
    {
        public:
            /**
             * Constructs the server
             *
             * @param int the number of the port to listen to
             * @param string documentRoot the root that should be used for static files
             */
			ServerImpl(std::string port = "80");
            virtual ~ServerImpl();


            /**
             * Runs the Mongoose server
             */
            void start(int thread_count);

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
			static void event_handler(struct mg_connection *connection, int ev, void *ev_data);

			void onHttpRequest(struct mg_connection *connection, struct http_message *message, job_id job_id);


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
			void setSsl(const char *certificate);

            /**
             * Polls the server
             */
            void poll();

            /**
             * Does the server handles url?
             */
            bool handles(std::string method, std::string url);

			void request_reply_async(job_id id, std::string data);
			bool execute_reply_async(job_id id, const void *buf, int len);

			void request_thread_proc(int id);
		protected:
			std::string port;
			struct mg_mgr mgr;
			struct mg_bind_opts opts;
			bool stopped;
            bool destroyed;
            struct mg_connection *server_connection;

            vector<Controller *> controllers;

			has_threads threads_;
			typedef nscp_thread::safe_queue<request_job, std::queue<request_job> > job_queue_type;
			job_queue_type job_queue_;
			boost::mutex idle_thread_mutex_;
			boost::condition_variable idle_thread_cond_;

	};
}
