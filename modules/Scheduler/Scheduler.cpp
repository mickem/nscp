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
#include "Scheduler.h"
#include <strEx.h>
#include <time.h>
#include <utils.h>

#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/macros.hpp>

#if BOOST_VERSION >= 105300
#include <boost/interprocess/detail/atomic.hpp>
#endif

namespace sh = nscapi::settings_helper;

bool Scheduler::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(alias, "scheduler");
	schedules_.set_path(settings.alias().get_settings_path("schedules"));

	settings.alias().add_path_to_settings()
		("SCHEDULER SECTION", "Section for the Scheduler module.")

		;

	settings.alias().add_key_to_settings()
		("threads", sh::int_fun_key<unsigned int>(boost::bind(&scheduler::simple_scheduler::set_threads, &scheduler_, _1), 5),
			"THREAD COUNT", "Number of threads to use.")
		;

	settings.alias().add_path_to_settings()
		("schedules", sh::fun_values_path(boost::bind(&Scheduler::add_schedule, this, _1, _2)),
			"SCHEDULER SECTION", "Section for the Scheduler module.",
			"SCHEDULE", "For more configuration options add a dedicated section")
		;

	settings.alias().add_templates()
		("schedules", "plus", "Add a simple schedule",
			"Add a simple scheduled job for passive monitoring",
			"{"
			"\"fields\": [ "
			" { \"id\": \"alias\",		\"title\" : \"Alias\",		\"type\" : \"input\",		\"desc\" : \"This will identify the command\"} , "
			" { \"id\": \"command\",	\"title\" : \"Command\",	\"type\" : \"data-choice\",	\"desc\" : \"The name of the command to execute\",\"exec\" : \"CheckExternalScripts list --json --query\" } , "
			" { \"id\": \"args\",		\"title\" : \"Arguments\",	\"type\" : \"input\",		\"desc\" : \"Command line arguments for the command\" } , "
			" { \"id\": \"cmd\",		\"key\" : \"command\", \"title\" : \"A\",	\"type\" : \"hidden\",		\"desc\" : \"A\" } "
			" ], "
			"\"events\": { "
			"\"onSave\": \"(function (node) { node.save_path = self.path; var f = node.get_field('cmd'); f.key = node.get_field('alias').value(); var val = node.get_field('command').value(); if (node.get_field('args').value()) { val += ' ' + node.get_field('args').value(); }; f.value(val)})\""
			"}"
			"}")
		;
	settings.register_all();
	settings.notify();

	schedules_.ensure_default();

	BOOST_FOREACH(const schedules::schedule_handler::object_list_type::value_type &o, schedules_.get_object_list()) {
		NSC_DEBUG_MSG("Adding scheduled item: " + o->to_string());
		scheduler_.add_task(o);
	}

	NSC_DEBUG_MSG_STD("Thread count: " + strEx::s::xtos(scheduler_.get_threads()));
	if (mode == NSCAPI::normalStart) {
		scheduler_.set_handler(this);
		scheduler_.start();
	}
	return true;
}

void Scheduler::add_schedule(std::string key, std::string arg) {
	try {
		schedules_.add(get_settings_proxy(), key, arg, key == "default");
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

bool Scheduler::unloadModule() {
	scheduler_.unset_handler();
	scheduler_.stop();
	schedules_.clear();
	return true;
}

void Scheduler::on_error(std::string error) {
	NSC_LOG_ERROR(error);
}

#if BOOST_VERSION >= 105300
volatile boost::uint32_t taskes = 0;
volatile boost::uint32_t submitted = 0;
volatile boost::uint32_t errors = 0;
using namespace boost::interprocess::ipcdetail;
#else
volatile int taskes = 0;
volatile int submitted = 0;
volatile int errors = 0;
void atomic_inc32(volatile int *i) {}
#endif
#include <nscapi/functions.hpp>
void Scheduler::handle_schedule(schedules::schedule_object item) {
	atomic_inc32(&taskes);
	try {
		std::string response;
		nscapi::core_helper ch(get_core(), get_id());
		if (!ch.simple_query(item.command.c_str(), item.arguments, response)) {
			NSC_LOG_ERROR("Failed to execute: " + item.command);
			if (item.channel.empty()) {
				NSC_LOG_ERROR_WA("No channel specified for ", item.alias);
				atomic_inc32(&errors);
				return;
			}
			nscapi::protobuf::functions::create_simple_submit_request(item.channel, item.command, NSCAPI::query_return_codes::returnUNKNOWN, "Command was not found: " + item.command, "", response);
			std::string result;
			get_core()->submit_message(item.channel, response, result);
			return;
		}
		Plugin::QueryResponseMessage resp_msg;
		resp_msg.ParseFromString(response);
		Plugin::QueryResponseMessage resp_msg_send;
		resp_msg_send.mutable_header()->CopyFrom(resp_msg.header());
		BOOST_FOREACH(const Plugin::QueryResponseMessage::Response &p, resp_msg.payload()) {
			if (nscapi::report::matches(item.report, nscapi::protobuf::functions::gbp_to_nagios_status(p.result())))
				resp_msg_send.add_payload()->CopyFrom(p);
		}
		if (resp_msg_send.payload_size() > 0) {
			if (item.channel.empty()) {
				NSC_LOG_ERROR_STD("No channel specified for " + utf8::cvt<std::string>(item.alias) + " mssage will not be sent.");
				atomic_inc32(&errors);
				return;
			}
			nscapi::protobuf::functions::make_submit_from_query(response, item.channel, item.alias, item.target_id, item.source_id);
			std::string result;
			atomic_inc32(&submitted);
			if (!get_core()->submit_message(item.channel, response, result)) {
				NSC_LOG_ERROR_STD("Failed to submit: " + item.alias);
				atomic_inc32(&errors);
				return;
			}
			std::string error;
			if (!nscapi::protobuf::functions::parse_simple_submit_response(result, error)) {
				NSC_LOG_ERROR_STD("Failed to submit " + item.alias + ": " + error);
				atomic_inc32(&errors);
				return;
			}
		} else {
			NSC_DEBUG_MSG("Filter not matched for: " + utf8::cvt<std::string>(item.alias) + " so nothing is reported");
		}
	} catch (nscapi::nscapi_exception &e) {
		atomic_inc32(&errors);
		NSC_LOG_ERROR_EXR("Failed to register command: ", e);
		scheduler_.remove_task(item.id);
	} catch (std::exception &e) {
		atomic_inc32(&errors);
		NSC_LOG_ERROR_EXR("Exception: ", e);
		scheduler_.remove_task(item.id);
	} catch (...) {
		atomic_inc32(&errors);
		NSC_LOG_ERROR_EX(item.alias);
		scheduler_.remove_task(item.id);
	}
}

void Scheduler::fetchMetrics(Plugin::MetricsMessage::Response *response) {

#if BOOST_VERSION >= 105300
	boost::uint64_t taskes__ = atomic_read32(&taskes);
	boost::uint64_t submitted__ = atomic_read32(&submitted);
	boost::uint64_t errors__ = atomic_read32(&errors);

	Plugin::Common::MetricsBundle *bundle = response->add_bundles();
	Plugin::Common::Metric *m = bundle->add_value();
	m->set_key("jobs");
	m->mutable_value()->set_int_data(taskes__);
	m = bundle->add_value();
	m->set_key("submitted");
	m->mutable_value()->set_int_data(submitted__);
	m = bundle->add_value();
	m->set_key("errors");
	m->mutable_value()->set_int_data(errors__);
	bundle->set_key("scheduler");
#else
	Plugin::Common::MetricsBundle *bundle = response->add_bundles();
	Plugin::Common::Metric *m = bundle->add_value();
	m->set_key("unavailable");
	m->mutable_value()->set_string_data("true");
	m = bundle->add_value();
#endif
}