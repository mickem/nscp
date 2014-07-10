/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "stdafx.h"

#include "DistributedServer.h"
#include <strEx.h>

#include "handler_impl.hpp"
#include "queue_manager.hpp"
#include "worker_manager.hpp"

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;

DistributedServer::DistributedServer() : context(NULL) {
}
DistributedServer::~DistributedServer() {
	delete context;
}

bool DistributedServer::loadModule() {
	return false;
}

bool DistributedServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	std::wstring host, suffix, server_mode;
	unsigned int thread_count;
	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("distributed"), alias, _T("server"));

		settings.alias().add_path_to_settings()
			(_T("DISTRIBUTED NSCP SERVER SECTION"), _T("Section for Distributed NSCP (DistributedServer) (check_nscp) protocol options."))
			;

		settings.alias().add_key_to_settings()
			(_T("host"), sh::wstring_key(&host, _T("tcp://*:5555")),
			_T("HOST TO BIND/CONNECT TO"), _T("The host to bind/connect to"))

			(_T("suffix"), sh::wstring_key(&suffix, _T("ncsp.dist")),
			_T("SUFFIX FOR INTERNAL CHANNELS"), _T("Has to be uniq on each server"))

			(_T("worker pool size"), sh::uint_key(&thread_count, 10),
			_T("WORKER POOL SIZE"), _T("Number of threads to spawn for the worker pool"))

			(_T("mode"), sh::wstring_key(&server_mode, _T("master")),
			_T("OPERATION MODE"), _T("Mode of operation can only be master now but will add more later on (such as slave)"))
			;

		settings.register_all();
		settings.notify();

		if (mode == NSCAPI::normalStart) {
			context = new zmq::context_t(2);

			zeromq_queue::connection_info queue_info(to_string(host), to_string(suffix));
			zeromq_queue::queue_manager queue;
			queue.start(context, threads, queue_info);

			zeromq_worker::connection_info worker_info(queue_info.get_backend(), to_string(suffix), thread_count);
			zeromq_worker::worker_manager workers;
			workers.start(context, threads, worker_info);
		}

	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + to_wstring(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}
	return true;
}


bool DistributedServer::unloadModule() {
	try {
		delete context;
		context = NULL;
		threads.join_all();
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN>"));
		return false;
	}
	return true;
}


bool DistributedServer::hasCommandHandler() {
	return false;
}
bool DistributedServer::hasMessageHandler() {
	return false;
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(DistributedServer);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
